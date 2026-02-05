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

Caml_inline int is_used_cont(value cont) {
  return atomic_load_relaxed(&Field(cont, 0)) == Val_ptr(NULL);
}

Caml_inline value next_cont(value cont) {
  return Field(cont, 2);
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
  skip_used_at_head(&Caml_state->cont_major_todo_head);
  Field(cont, 2) = Caml_state->cont_major_todo_head;
  Caml_state->cont_major_todo_head = cont;
}

/* Insert cont at head of major toclean list */
CAMLexport void caml_cont_insert_major_toclean(value cont)
{
  Field(cont, 2) = Caml_state->cont_major_toclean_head;
  Caml_state->cont_major_toclean_head = cont;
}

/* Debug: print major GC lists */
CAMLexport void caml_cont_print_major(const char *tag)
{
#ifdef DEBUG
  caml_gc_log("cont_major[%s]: todo=%p toclean=%p", tag,
              (void*)Caml_state->cont_major_todo_head, (void*)Caml_state->cont_major_toclean_head);

  int i = 1;
  for (value cur = Caml_state->cont_major_todo_head; Is_block(cur); cur = next_cont(cur))
    caml_gc_log("  todo[%d]: %p", i++, (void*)cur);

  i = 1;
  for (value cur = Caml_state->cont_major_toclean_head; Is_block(cur); cur = next_cont(cur))
    caml_gc_log("  toclean[%d]: %p", i++, (void*)cur);
#endif
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
    value next = next_cont(cur);
    
    if (is_used_cont(cur)) {
      caml_gc_log("  removing used: %p", (void*)cur);
      if (prev == Val_long(0))
        Caml_state->cont_major_todo_head = next;
      else
        Field(prev, 2) = next;
    } else {
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
  for (cur = Caml_state->cont_major_toclean_head; Is_block(cur); cur = next_cont(cur)) {
    caml_darken_cont(cur);
    darkened = 1;
  }

  caml_gc_log("cont_major: mark_and_shift done (darkened=%d)", darkened);
  processing_mark_and_shift = 0;
  return darkened;
}

/* Call Effect.runtime_discontinue on each toclean continuation.
   Must be called from a safe point (e.g., caml_process_pending_actions). */
CAMLexport void caml_cont_discontinue_toclean(void)
{
  CAMLparam0();
  CAMLlocal3(exn, runtime_discontinue_fn, cur);
  
  if (processing_discontinue || !Is_block(Caml_state->cont_major_toclean_head)) {
    CAMLreturn0;
  }
  
  processing_discontinue = 1;
  
  const value *runtime_discontinue_fn_ptr = caml_named_value("Effect.runtime_discontinue");
  const value *exn_ptr = caml_named_value("Effect.Gc_unreachable");

#ifdef DEBUG
  if (runtime_discontinue_fn_ptr == NULL) {
    caml_gc_log("cont_major: Effect.runtime_discontinue not registered");
    processing_discontinue = 0;
    CAMLreturn0;
  }
  if (exn_ptr == NULL) {
    caml_gc_log("cont_major: Effect.Gc_unreachable not registered");
    processing_discontinue = 0;
    CAMLreturn0;
  }
#endif
  
  runtime_discontinue_fn = *runtime_discontinue_fn_ptr;
  exn = *exn_ptr;
  
  caml_gc_log("cont_major: discontinuing toclean");
  
  while (Is_block(Caml_state->cont_major_toclean_head)) {
    cur = Caml_state->cont_major_toclean_head;
    
    value next = next_cont(cur);
    Field(cur, 2) = Val_long(0);
    Caml_state->cont_major_toclean_head = next;

    caml_gc_log("  discontinue: %p", (void*)cur);
    caml_callback2_exn(runtime_discontinue_fn, cur, exn);
    cur = Val_long(0);
  }

  caml_gc_log("cont_major: toclean done");
  processing_discontinue = 0;
  CAMLreturn0;
}

/* ========================================================================== */
/* Minor GC                                                                   */
/* ========================================================================== */

/* Insert minor heap continuation into minor todo list */
CAMLexport void caml_cont_insert_minor_todo(value cont)
{
  if (!Is_young(cont)) {
    caml_cont_insert_major_todo(cont);
    return;
  }
  
  caml_gc_log("cont_minor: insert %p", (void*)cont);
  
  /* Skip used at head */
  skip_used_at_head(&Caml_state->cont_minor_todo_head);
  
  Field(cont, 2) = Caml_state->cont_minor_todo_head;
  Caml_state->cont_minor_todo_head = cont;
}

/* Debug: print minor todo list */
CAMLexport void caml_cont_print_minor(const char *tag)
{
#ifdef DEBUG
  caml_gc_log("cont_minor[%s]: head=%p", tag, (void*)Caml_state->cont_minor_todo_head);
  
  int i = 1;
  for (value cur = Caml_state->cont_minor_todo_head; Is_block(cur); cur = next_cont(cur)) {
    int forwarded = Is_young(cur) && (Hd_val(cur) == 0);
    caml_gc_log("  [%d]: %p young=%d fwd=%d", i++, (void*)cur, Is_young(cur), forwarded);
  }
#endif
}

/* Process minor todo list during minor GC:
   - Forwarded (reachable): add to major_todo
   - Not forwarded (unreachable): promote and add to major_todo */
CAMLexport void caml_cont_process_minor_todo(
  void (*oldify_fn)(void*, value, volatile value*),
  void* oldify_state,
  caml_domain_state* domain_state)
{
  caml_gc_log("cont_minor: processing");  
  value cur = Caml_state->cont_minor_todo_head;
  
  while (Is_block(cur)) {
    value next = next_cont(cur);
    header_t hd = Hd_val(cur);
    
    
    /* If taken cont then ignore */
    if (is_used_cont(cur)) {
      caml_gc_log("  skip used: %p", (void*)cur);
    } else if (hd == 0) {
    /* If NOT taken and Reachable (Promoted) cont then add to major_todo list */
      value fwd = Field(cur, 0);
      caml_gc_log("  forwarded: %p -> %p", (void*)cur, (void*)fwd);
      caml_cont_insert_major_todo(fwd);
    } else {
      value stack_val = atomic_load_relaxed(&Field(cur, 0));
      struct stack_info* stk = Ptr_val(stack_val);
      
      if (stk == domain_state->current_stack) {
        caml_gc_log("  skip current stack: %p", (void*)cur);
        cur = next;
        continue;
      }

    /* If NOT taken unreachable cont then promote and add to major_todo list */
      caml_gc_log("  unreachable: %p -> promoting", (void*)cur);
      volatile value promoted = Val_long(0);
      oldify_fn(oldify_state, cur, &promoted);
      
      if (Is_block(promoted) && !Is_young(promoted)) {
        caml_gc_log("  promoted->todo: %p", (void*)promoted);
        caml_cont_insert_major_todo(promoted);
      }
    }

    cur = next;
  }
  
  Caml_state->cont_minor_todo_head = Val_long(0);
  caml_gc_log("cont_minor: done");
}
