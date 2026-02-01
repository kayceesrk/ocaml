/* Continuation linked list management for GC.
   
   Maintains three per-domain lists using Field(cont,2) as next pointer:
   - cont_major_todo_head: continuations to track during major GC
   - cont_major_toclean_head: unreachable continuations awaiting discontinue
   - cont_minor_todo_head: minor heap continuations for minor GC processing
*/

#define CAML_INTERNALS

#include "caml/config.h"
#include "caml/mlvalues.h"
#include "caml/memory.h"
#include "caml/callback.h"
#include "caml/misc.h"
#include "caml/alloc.h"
#include "caml/cont_ll.h"
#include "caml/domain.h"
#include "caml/shared_heap.h"
#include "caml/major_gc.h"
#include "caml/fiber.h"

/* Re-entrancy guards (per-domain) */
static CAMLthread_local int processing_mark_and_shift = 0;
static CAMLthread_local int processing_discontinue = 0;

Caml_inline int is_valid_cont(value v) {
  return Is_block(v) && Tag_val(v) == Cont_tag;
}

Caml_inline int is_used_cont(value cont) {
  return atomic_load_relaxed(&Field(cont, 0)) == Val_ptr(NULL);
}

Caml_inline value cont_next(value cont) {
  return is_valid_cont(cont) ? Field(cont, 2) : Val_long(0);
}

/* Skip used continuations at head of a list */
Caml_inline void skip_used_at_head(volatile value *head) {
  while (Is_block(*head) && is_used_cont(*head)) {
    *head = atomic_load_relaxed(&Field(*head, 2));
  }
}

/* Insert cont at head of major todo list */
CAMLexport void caml_cont_insert_major_todo(value cont)
{
  if (!is_valid_cont(cont)) return;
  skip_used_at_head(&Caml_state->cont_major_todo_head);
  Field(cont, 2) = Caml_state->cont_major_todo_head;
  Caml_state->cont_major_todo_head = cont;
}

/* Insert cont at head of major toclean list */
CAMLexport void caml_cont_insert_major_toclean(value cont)
{
  if (!is_valid_cont(cont)) return;
  Field(cont, 2) = Caml_state->cont_major_toclean_head;
  Caml_state->cont_major_toclean_head = cont;
}

/* Debug: print major GC lists */
CAMLexport void caml_cont_print_major(const char *tag)
{
  caml_gc_log("cont_major[%s]: todo=%p toclean=%p", tag,
              (void*)Caml_state->cont_major_todo_head, (void*)Caml_state->cont_major_toclean_head);

  int i = 1;
  for (value cur = Caml_state->cont_major_todo_head; Is_block(cur); cur = cont_next(cur))
    caml_gc_log("  todo[%d]: %p", i++, (void*)cur);

  i = 1;
  for (value cur = Caml_state->cont_major_toclean_head; Is_block(cur); cur = cont_next(cur))
    caml_gc_log("  toclean[%d]: %p", i++, (void*)cur);
}

/* Process todo list after marking: move unmarked to toclean, darken them.
   Returns 1 if any were darkened (caller should mark again), 0 otherwise. */
CAMLexport int caml_cont_mark_and_shift_toclean(void)
{
  if (processing_mark_and_shift || processing_discontinue) {
    caml_gc_log("cont_major: mark_and_shift skipped (re-entrant)");
    return 0;
  }
  
  processing_mark_and_shift = 1;
  caml_gc_log("cont_major: mark_and_shift starting");
  
  skip_used_at_head(&Caml_state->cont_major_todo_head);
  
  value cur = Caml_state->cont_major_todo_head;
  value prev = Val_long(0);
  
  while (Is_block(cur)) {
    value next = cont_next(cur);
    
    if (is_used_cont(cur)) {
      caml_gc_log("  removing used: %p", (void*)cur);
      if (prev == Val_long(0))
        Caml_state->cont_major_todo_head = next;
      else
        Field(prev, 2) = next;
    } else {
      CAMLassert(is_valid_cont(cur));
      header_t hd = Hd_val(cur);
      
      if (Has_status_hd(hd, caml_global_heap_state.UNMARKED)) {
        caml_gc_log("  unmarked->toclean: %p", (void*)cur);
        if (prev == Val_long(0))
          Caml_state->cont_major_todo_head = next;
        else
          Field(prev, 2) = next;
        
        Field(cur, 2) = Caml_state->cont_major_toclean_head;
        Caml_state->cont_major_toclean_head = cur;
      } else {
        caml_gc_log("  keeping marked: %p", (void*)cur);
        prev = cur;
      }
    }
    cur = next;
  }
  
  /* Darken toclean list to keep them alive until discontinue */
  int darkened = 0;
  for (cur = Caml_state->cont_major_toclean_head; Is_block(cur); cur = cont_next(cur)) {
    CAMLassert(is_valid_cont(cur));
    caml_darken_cont(cur);
    darkened = 1;
  }

  caml_gc_log("cont_major: mark_and_shift done (darkened=%d)", darkened);
  processing_mark_and_shift = 0;
  return darkened;
}

/* Call Effect.discontinue on each toclean continuation.
   Must be called from a safe point (e.g., caml_process_pending_actions). */
CAMLexport void caml_cont_discontinue_toclean(void)
{
  CAMLparam0();
  CAMLlocal3(exn, discontinue_fn, cur);
  
  if (processing_discontinue || !Is_block(Caml_state->cont_major_toclean_head)) {
    CAMLreturn0;
  }
  
  processing_discontinue = 1;
  
  const value *fn = caml_named_value("Effect.discontinue");
  if (fn == NULL) {
    caml_gc_log("cont_major: Effect.discontinue not registered");
    processing_discontinue = 0;
    CAMLreturn0;
  }
  discontinue_fn = *fn;
  
  const value *exn_ptr = caml_named_value("Effect.Gc_unreachable");
  exn = *exn_ptr;
  
  caml_gc_log("cont_major: discontinuing toclean");
  
  while (Is_block(Caml_state->cont_major_toclean_head)) {
    cur = Caml_state->cont_major_toclean_head;
    CAMLassert(is_valid_cont(cur));
    
    value next = cont_next(cur);
    Field(cur, 2) = Val_long(0);
    Caml_state->cont_major_toclean_head = next;

    caml_gc_log("  discontinue: %p", (void*)cur);
    value result = caml_callback2_exn(discontinue_fn, cur, exn);
    
    if (Is_exception_result(result)) {
      caml_gc_log("  discontinue failed: %p", (void*)cur);
    }
    cur = Val_long(0);
  }

  Caml_state->cont_major_toclean_head = Val_long(0);
  caml_gc_log("cont_major: toclean done");
  processing_discontinue = 0;
  CAMLreturn0;
}

/* ========================================================================== */
/* Minor GC                                                                   */
/* ========================================================================== */

Caml_inline int is_young(value v) {
  return Is_block(v) && Is_young(v);
}

/* Insert minor heap continuation into minor todo list */
CAMLexport void caml_cont_insert_minor_todo(value cont)
{
  if (!is_valid_cont(cont)) return;
  if (!is_young(cont)) {
    caml_cont_insert_major_todo(cont);
    return;
  }
  
  caml_gc_log("cont_minor: insert %p", (void*)cont);
  
  /* Skip used at head */
  while (Is_block(Caml_state->cont_minor_todo_head) && 
         is_young(Caml_state->cont_minor_todo_head) && 
         is_used_cont(Caml_state->cont_minor_todo_head)) {
    Caml_state->cont_minor_todo_head = atomic_load_relaxed(&Field(Caml_state->cont_minor_todo_head, 2));
  }
  
  Field(cont, 2) = Caml_state->cont_minor_todo_head;
  Caml_state->cont_minor_todo_head = cont;
}

/* Debug: print minor todo list */
CAMLexport void caml_cont_print_minor(const char *tag)
{
  caml_gc_log("cont_minor[%s]: head=%p", tag, (void*)Caml_state->cont_minor_todo_head);
  
  int i = 1;
  for (value cur = Caml_state->cont_minor_todo_head; Is_block(cur); ) {
    int forwarded = is_young(cur) && (Hd_val(cur) == 0);
    caml_gc_log("  [%d]: %p young=%d fwd=%d", i++, (void*)cur, is_young(cur), forwarded);
    cur = is_young(cur) ? Field(cur, 2) : cont_next(cur);
  }
}

/* Process minor todo list during minor GC:
   - Forwarded: add to major todo
   - Unreachable (not forwarded): promote and add to toclean */
CAMLexport void caml_cont_process_minor_todo(
  void (*oldify_fn)(void*, value, volatile value*),
  void* oldify_state,
  void* domain_ptr)
{
  caml_gc_log("cont_minor: processing");
  caml_domain_state* domain_state = (caml_domain_state*)domain_ptr;
  
  value cur = Caml_state->cont_minor_todo_head;
  
  while (Is_block(cur)) {
    value next = is_young(cur) ? Field(cur, 2) : Val_long(0);
    
    if (!is_young(cur)) {
      caml_gc_log("  major heap cont %p, stopping", (void*)cur);
      break;
    }
    
    header_t hd = Hd_val(cur);
    
    if (hd == 0) {
      /* Forwarded - add to major todo if valid */
      value fwd = Field(cur, 0);
      caml_gc_log("  forwarded: %p -> %p", (void*)cur, (void*)fwd);
      if (Is_block(fwd) && is_valid_cont(fwd) && !is_used_cont(fwd)) {
        caml_cont_insert_major_todo(fwd);
      }
      cur = Is_block(next) && is_young(next) ? next : Val_long(0);
      continue;
    }
    
    if (!is_valid_cont(cur)) {
      cur = next;
      continue;
    }
    
    value stack_val = atomic_load_relaxed(&Field(cur, 0));
    
    if (stack_val == Val_ptr(NULL) || stack_val == 0 || (stack_val & 1) == 0) {
      caml_gc_log("  skip used/invalid: %p", (void*)cur);
      cur = next;
      continue;
    }
    
    struct stack_info* stk = Ptr_val(stack_val);
    
    if (stk == NULL || stk == domain_state->current_stack || stk->magic != 42) {
      caml_gc_log("  skip invalid stack: %p", (void*)cur);
      cur = next;
      continue;
    }
    
    /* Check parent stack chain */
    int is_parent = 0;
    for (struct stack_info* s = domain_state->current_stack; s; s = Stack_parent(s)) {
      if (Stack_parent(s) == stk) { is_parent = 1; break; }
    }
    if (is_parent) {
      cur = next;
      continue;
    }
    
    /* Promote unreachable to toclean */
    caml_gc_log("  unreachable: %p -> promoting", (void*)cur);
    volatile value promoted = Val_long(0);
    oldify_fn(oldify_state, cur, &promoted);
    
    if (Is_block(promoted) && !is_young(promoted)) {
      caml_gc_log("  promoted->toclean: %p", (void*)promoted);
      caml_cont_insert_major_toclean(promoted);
    }
    
    cur = next;
  }
  
  Caml_state->cont_minor_todo_head = Val_long(0);
  caml_gc_log("cont_minor: done");
}
