/* linkedlist helpers for continuation objects
   Each continuation is an OCaml block with tag Cont_tag and at least
   three fields. We use Field(cont,2) as "next" pointer for singly-linked list.

   This module maintains three domain-local lists of continuations:
     - todo list (continuations to be processed during major GC)
     - toclean list (continuations considered as dead, awaiting discontinue)
     - minor_todo list (minor heap continuations to be processed during minor GC)

   It exposes C helpers to insert a continuation into a list and
   to scan the todo list applying either a C callback or an OCaml closure.
   
   All lists are stored in the domain state to enable per-domain tracking.
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

/* Guard flags to prevent re-entrant processing during GC or discontinuation.
   These are thread-local (per domain). */
static CAMLthread_local int processing_mark_and_shift = 0;
static CAMLthread_local int processing_discontinue = 0;

/* Initialization: This is called from domain_create for proper per-domain setup.
   The domain state fields and GC root registration are now handled in domain.c.
   This function is kept for backward compatibility and additional init if needed. */

/* Internal helper for accessing next field in a continuation block.
   We assume the continuation has at least 3 fields and that field 2
   is reserved for next pointer. */

static int is_valid_cont(value v) {
  return Is_block(v) && Tag_val(v) == Cont_tag;
}

Caml_inline value cont_next(value cont) {
  return is_valid_cont(cont) ? Field(cont, 2) : Val_long(0);
}

/* Insert [cont] at the head of the todo list.
   Skip over any already-taken continuations at the head (field 0 == NULL)
   and insert before the first non-taken node. */
CAMLexport void caml_cont_ll_insert_todo(value cont)
{ 
  /* Skip over used continuations at the head to find first non-taken node.
     Note: Field(cont, 0) is set to Val_ptr(NULL) when continuation is used. */
  while (Is_block(Caml_state->cont_todo_head) && 
         atomic_load_relaxed(&Field(Caml_state->cont_todo_head, 0)) == Val_ptr(NULL)) {
    Caml_state->cont_todo_head = atomic_load_relaxed(&Field(Caml_state->cont_todo_head, 2));
  }

  /* defensive: only operate on blocks */
  if (!is_valid_cont(cont)) return;
  
  Field(cont, 2) = Caml_state->cont_todo_head;  /* new node points to head */
  
  Caml_state->cont_todo_head = cont; /* new node becomes the head */
}

/* Insert [cont] at the head of the toclean list. */
CAMLexport void caml_cont_ll_insert_toclean(value cont)
{
  if (is_valid_cont(cont)) {
    Field(cont, 2) = Caml_state->cont_toclean_head;        /* next = old head */
    Caml_state->cont_toclean_head = cont;
  }
}

/* Accessors for testing/inspection from C */
CAMLexport value caml_cont_ll_get_todo_head(void) { return Caml_state->cont_todo_head; }
CAMLexport value caml_cont_ll_get_toclean_head(void) { return Caml_state->cont_toclean_head; }

/* Debugging: print the two lists using caml_gc_log. [tag] is a short label
   indicating the context of the print (e.g. "before-sweep"). */
CAMLexport void caml_cont_ll_print(const char *tag)
{
  /* print header */
  caml_gc_log("cont_ll[%s]: todo_head=%p, toclean_head=%p", tag, 
              (void*)Caml_state->cont_todo_head, (void*)Caml_state->cont_toclean_head);

  /* Walk todo list */
  int i=1;
  value cur = Caml_state->cont_todo_head;
  while (Is_block(cur)) {
    caml_gc_log("  todo: %d. cont=%p", i, (void*)cur);
    i++;
    cur = cont_next(cur);
  }

  /* Walk toclean list */
  i=1;
  cur = Caml_state->cont_toclean_head;
  while (Is_block(cur)) {
    caml_gc_log("  toclean: %d. cont=%p", i, (void*)cur);
    i++;
    cur = cont_next(cur);
  }
}

/* Process todo list after marking phase:
   - Remove already-taken continuations (field 0 is null)
   - Move unmarked continuations to toclean list and mark them
   - Then perform a marking scan on all toclean list continuations
   
   This should be called after the major GC marking phase completes.
*/
CAMLexport void caml_cont_mark_and_shift_toclean(void)
{
  /* Prevent re-entrant calls during GC or while discontinuing */
  if (processing_mark_and_shift || processing_discontinue) {
    caml_gc_log("cont_ll: Skipping mark_and_shift_toclean (already processing)");
    return;
  }
  
  processing_mark_and_shift = 1;
  caml_gc_log("cont_ll: Starting mark_and_shift_toclean");
  
  /* Skip over used continuations at the head to find first non-taken node.
     Note: Field(cont, 0) is set to Val_ptr(NULL) (i.e., 0) when continuation is used. */
  while (Is_block(Caml_state->cont_todo_head) && 
         atomic_load_relaxed(&Field(Caml_state->cont_todo_head, 0)) == Val_ptr(NULL)) {
    caml_gc_log("  Removing Used cont: %p", (void*)Caml_state->cont_todo_head);
    Caml_state->cont_todo_head = atomic_load_relaxed(&Field(Caml_state->cont_todo_head, 2));
  }
  value cur = Caml_state->cont_todo_head;
  value prev = Val_long(0);  /* previous node (or 0 if at head) */
  
  
  /* Walk through todo list */
  while (Is_block(cur)) {
    value next = cont_next(cur);
    
    /* Check if continuation is used or uninitialized.
       - Used: Field(0) = Val_ptr(NULL) = 1 (set by caml_continuation_use_noexc)
       - Uninitialized: Field(0) = Val_long(0) = 1 (initial allocation value)
       Both cases result in Field(0) == 1, so we check for that.
       
       Note: Valid stack pointers are stored as Val_ptr(stack) = stack + 1,
       which will be > 1 for any non-NULL stack pointer. */
    value field0 = atomic_load_relaxed(&Field(cur, 0));
    
    if (field0 == Val_ptr(NULL)) {
      /* Used or uninitialized (both have field0 == 1) */
      caml_gc_log("  Removing Used/uninitialized cont: %p", (void*)cur);
      /* Remove from todo list by skipping this node */
      if (prev == Val_long(0)) {
        Caml_state->cont_todo_head = next;
      } else {
        Field(prev, 2) = next;
      }
    } else {
      /* Has a valid stack pointer (field0 = Val_ptr(stack) where stack != NULL) */
      if (!is_valid_cont(cur)) {
        caml_fatal_error("cont_ll: INVALID node in todo list: %p (tag=%d, wosize=%lu)",
                         (void*)cur, (int)Tag_val(cur), (unsigned long)Wosize_val(cur));
      }

      header_t hd = Hd_val(cur);
      
      /* Check if unmarked */
      if (Has_status_hd(hd, caml_global_heap_state.UNMARKED)) {
        /* Remove from todo list */
        if (prev == Val_long(0)) {
          Caml_state->cont_todo_head = next;
        } else {
          Field(prev, 2) = next;
        }
        
        /* Insert into toclean list */
        Field(cur, 2) = Caml_state->cont_toclean_head;
        Caml_state->cont_toclean_head = cur;

        caml_gc_log("  Moving unmarked cont to toclean: %p", (void*)cur);

      } else {
        /* Continuation is marked, keep it in todo list */
        prev = cur;
        caml_gc_log("  Keeping marked cont in todo: %p", (void*)cur);
      }
    }
    
    cur = next;
  }
  
  /* Mark the entire toclean list so these continuations (and everything
     reachable from their stacks) stay alive until caml_discontinue_toclean
     runs. We call caml_darken_cont on each continuation to traverse its
     stack and schedule any reachable objects for marking. */
  caml_gc_log("  Marking toclean list to keep continuations alive");
  cur = Caml_state->cont_toclean_head;
  int darkened_any = 0;
  while (Is_block(cur)) {
    if (!is_valid_cont(cur)) {
      caml_fatal_error("cont_ll: INVALID node in toclean list: %p (tag=%d, wosize=%lu)",
                       (void*)cur, (int)Tag_val(cur), (unsigned long)Wosize_val(cur));
    }
    caml_darken_cont(cur);
    darkened_any = 1;
    cur = cont_next(cur);
  }

  /* Finish marking any objects that were reachable from toclean continuations.
     Only drain the mark stack if we actually scheduled work above; calling
     caml_empty_mark_stack() unconditionally could re-enter this code path
     and cause an infinite loop when nothing new was scheduled. */
  if (darkened_any) {
    caml_empty_mark_stack();
  } else {
    caml_gc_log("  No toclean entries darkened; skipping empty mark stack drain");
  }
  
  caml_gc_log("cont_ll: Finished mark_and_shift_toclean");
  processing_mark_and_shift = 0;
}

/* Process toclean list by calling OCaml discontinue function:
   - Walk through toclean list
   - Call Effect.discontinue with each continuation and an exception
   - Clear the toclean list after processing
   
   This should be called from a safe point where OCaml callbacks are allowed,
   such as caml_process_pending_actions.
*/
CAMLexport void caml_discontinue_toclean(void)
{
  CAMLparam0();
  CAMLlocal3(exn, discontinue_closure, cur);
  
  /* Prevent re-entrant calls */
  if (processing_discontinue) {
    caml_gc_log("cont_ll: Skipping discontinue_toclean (already processing)");
    CAMLreturn0;
  }
  
  /* Check if there's anything to process */
  if (!Is_block(Caml_state->cont_toclean_head)) {
    caml_gc_log("cont_ll: No continuations in toclean list");
    CAMLreturn0;
  }
  
  processing_discontinue = 1;
  
  /* Get the discontinue function from Effect module */
  const value *discontinue_fn = caml_named_value("Effect.discontinue");
  if (discontinue_fn == NULL) {
    caml_gc_log("cont_ll: Effect.discontinue not registered");
    processing_discontinue = 0;
    CAMLreturn0;
  }
  
  discontinue_closure = *discontinue_fn;
  
  /* Create an exception for unreachable continuations.
     Prefer the pre-registered Effect.Gc_unreachable for stability. */
  {
    const value *gc_unreachable = caml_named_value("Effect.Gc_unreachable");
    exn = *gc_unreachable;
  }
  
  caml_gc_log("cont_ll: Processing toclean list with discontinue");
  
  /* Process one continuation at a time, removing from head before callback.
     This keeps the current continuation reachable (through cur) while avoiding
     traversing pointers that the callback might mutate. */
  while (Is_block(Caml_state->cont_toclean_head)) {
    cur = Caml_state->cont_toclean_head;
    if (!is_valid_cont(cur)) {
      caml_fatal_error("cont_ll: INVALID node in toclean during discontinue: %p (tag=%d, wosize=%lu)",
                       (void*)cur, (int)Tag_val(cur), (unsigned long)Wosize_val(cur));
    }
    
    /* Remove from list BEFORE discontinuing to avoid dangling references */
    value next = cont_next(cur);
    Field(cur, 2) = Val_long(0);  /* Clear the next pointer */
    Caml_state->cont_toclean_head = next;

    caml_gc_log("  Discontinuing cont: %p", (void*)cur);

    /* Call Effect.discontinue(cont, exn) */
    value result = caml_callback2_exn(discontinue_closure, cur, exn);

    /* Check if the callback raised an exception */
    if (Is_exception_result(result)) {
      caml_gc_log("  Warning: discontinue raised exception for cont %p", (void*)cur);
      /* Continue processing other continuations even if one fails */
    }
    
    /* After discontinue, the continuation is used (field 0 = null).
       Clear our local reference before the next iteration. */
    cur = Val_long(0);
  }

  /* Ensure the list head is properly terminated */
  Caml_state->cont_toclean_head = Val_long(0);
  
  caml_gc_log("cont_ll: Finished discontinuing toclean list");
  processing_discontinue = 0;
  CAMLreturn0;
}

/* ========================================================================== */
/* Minor GC continuation tracking                                             */
/* ========================================================================== */


/* Check if a value is in the minor heap */
static int is_young(value v) {
  return Is_block(v) && Is_young(v);
}

/* Insert a continuation into the minor todo list.
   Only accepts minor heap continuations. */
CAMLexport void caml_cont_ll_insert_minor_todo(value cont)
{
  /* Only insert if it's a valid continuation in the minor heap */
  if (!is_valid_cont(cont)) return;
  if (!is_young(cont)) {
    /* If not in minor heap, insert into major todo list instead */
    caml_cont_ll_insert_todo(cont);
    return;
  }
  
  caml_gc_log("cont_ll_minor: Inserting cont %p into minor todo", (void*)cont);
  
  /* Skip over used continuations at the head */
  while (Is_block(Caml_state->cont_minor_todo_head) && 
         is_young(Caml_state->cont_minor_todo_head) && 
         atomic_load_relaxed(&Field(Caml_state->cont_minor_todo_head, 0)) == Val_ptr(NULL)) {
    Caml_state->cont_minor_todo_head = atomic_load_relaxed(&Field(Caml_state->cont_minor_todo_head, 2));
  }
  
  Field(cont, 2) = Caml_state->cont_minor_todo_head;
  Caml_state->cont_minor_todo_head = cont;
}

/* Get the head of the minor todo list */
CAMLexport value caml_cont_ll_get_minor_todo_head(void)
{
  return Caml_state->cont_minor_todo_head;
}


/* Print the minor todo list for debugging */
CAMLexport void caml_cont_ll_print_minor(const char *tag)
{
  caml_gc_log("cont_ll_minor[%s]: minor_todo_head=%p", tag, 
              (void*)Caml_state->cont_minor_todo_head);
  
  int i = 1;
  value cur = Caml_state->cont_minor_todo_head;
  while (Is_block(cur)) {
    /* Check if forwarded (header == 0) */
    int is_forwarded = 0;
    if (is_young(cur)) {
      header_t hd = Hd_val(cur);
      is_forwarded = (hd == 0);
    }
    
    caml_gc_log("  minor_todo: %d. cont=%p (young=%d, forwarded=%d)", 
                i, (void*)cur, is_young(cur), is_forwarded);
    i++;
    
    /* For young blocks, read Field(2) directly even if forwarded.
       For major heap blocks or non-continuations, use cont_next. */
    if (is_young(cur)) {
      cur = Field(cur, 2);
    } else {
      cur = cont_next(cur);
    }
  }
}

/* Process the minor todo list during minor GC.
   
   This walks through the minor todo list and handles each continuation:
   1. If Field(cont, 0) == Val_ptr(NULL): continuation was used, skip it
   2. If the continuation header is 0 (forwarded): it was promoted, 
      follow the forwarding pointer and add to major todo list
   3. If the continuation is still in minor heap and not forwarded:
      it's unreachable - promote it explicitly and add to toclean list
      
   The tricky part is handling the linked list itself - we need to ensure
   the entire list gets promoted properly so we can traverse it.
*/
CAMLexport void caml_cont_ll_process_minor_todo(
  void (*oldify_fn)(void*, value, volatile value*),
  void* oldify_state,
  void* domain_ptr)
{
  caml_gc_log("cont_ll_minor: Processing minor todo list");
  
  value cur = Caml_state->cont_minor_todo_head;
  
  while (Is_block(cur)) {
    /* We must ALWAYS save the next pointer from the minor heap location
       before doing anything else, because promotion can change the memory.
       
       IMPORTANT: For forwarded blocks (header == 0), is_valid_cont() returns
       false because Tag_val returns 0. But Field(2) still contains the original
       next pointer in the minor heap, so we must read it anyway! */
    value next_in_minor = Val_long(0);
    if (is_young(cur)) {
      /* For minor heap blocks, always try to read Field(2) as the next pointer.
         Even if the block is forwarded, Field(2) is preserved. */
      next_in_minor = Field(cur, 2);
    }
    
    /* For minor heap blocks, check if they've been forwarded */
    if (!is_young(cur)) {
      /* This continuation is already in major heap - this shouldn't happen
         at the start, but can happen if we're traversing a promoted chain.
         Stop traversing to avoid loops. */
      caml_gc_log("  cont_ll_minor: Encountered major heap cont %p, stopping chain traversal", (void*)cur);
      break;
    }
    
    /* Check if the continuation was forwarded (header == 0) */
    header_t hd = Hd_val(cur);
    if (hd == 0) {
      /* Forwarded - follow the forwarding pointer */
      value forwarded = Field(cur, 0);
      caml_gc_log("  cont_ll_minor: Cont %p forwarded to %p", (void*)cur, (void*)forwarded);
      
      /* The forwarded continuation is in major heap, add to major todo */
      if (Is_block(forwarded) && is_valid_cont(forwarded) && 
          Field(forwarded, 0) != Val_ptr(NULL)) {
        caml_cont_ll_insert_todo(forwarded);
      }
      
      /* Move to next using the saved minor heap pointer.
         The next pointer in minor heap still points to the next minor block. */
      if (Is_block(next_in_minor)) {
        if (is_young(next_in_minor)) {
          /* Next is still in minor heap (may or may not be forwarded) */
          cur = next_in_minor;
        } else {
          /* Next is already in major heap - shouldn't happen normally */
          caml_gc_log("  cont_ll_minor: next_in_minor %p is in major heap, stopping", (void*)next_in_minor);
          break;
        }
      } else {
        /* End of list (Val_long(0)) */
        cur = next_in_minor;
      }
      continue;
    }
    
    /* Not forwarded - check if it's a valid continuation */
    if (!is_valid_cont(cur)) {
      caml_gc_log("  cont_ll_minor: Invalid cont %p, skipping", (void*)cur);
      cur = next_in_minor;
      continue;
    }
    
    /* Check if continuation was used or uninitialized.
       - Val_ptr(NULL) == 1: continuation was used (stack consumed)
       - 0: continuation was never initialized (allocation but no perform yet)
       Both cases mean we should skip this continuation. */
    value stack_value = atomic_load_relaxed(&Field(cur, 0));
    if (stack_value == Val_ptr(NULL) || stack_value == 0) {
      caml_gc_log("  cont_ll_minor: Used/uninitialized cont %p (field0=%p), skipping", 
                  (void*)cur, (void*)stack_value);
      cur = next_in_minor;
      continue;
    }
    
    /* Validate that stack_value looks like a valid Val_ptr (lowest bit should be 1) */
    if ((stack_value & 1) == 0) {
      caml_gc_log("  cont_ll_minor: Cont %p has invalid stack_value %p (not Val_ptr), skipping",
                  (void*)cur, (void*)stack_value);
      cur = next_in_minor;
      continue;
    }
    
    /* Still in minor heap and not forwarded - this is unreachable!
       We need to promote it to major heap and add to toclean for discontinue.
       
       IMPORTANT: We must validate the stack before promoting. The stack
       might be invalid (corrupted, already freed, etc.) for some unreachable
       continuations (e.g., from unhandled effects that crashed). 
       
       If the stack is invalid, we skip the continuation and let it be
       garbage collected - we cannot safely discontinue it anyway. */
    struct stack_info* stk = Ptr_val(stack_value);
    
    /* Validate the stack pointer */
    if (stk == NULL) {
      caml_gc_log("  cont_ll_minor: Unreachable cont %p has NULL stack, skipping", (void*)cur);
      cur = next_in_minor;
      continue;
    }
    
    /* CRITICAL: Check if this stack is the current execution stack!
       
       For unhandled effects, a continuation is allocated pointing to the
       current stack, then an exception is raised. The continuation appears
       unreachable but its stack is actually the live execution stack.
       
       We must NOT try to discontinue or free the current stack! */
    caml_domain_state* domain_state = (caml_domain_state*)domain_ptr;
    if (stk == domain_state->current_stack) {
      caml_gc_log("  cont_ll_minor: Unreachable cont %p points to current stack, skipping", (void*)cur);
      cur = next_in_minor;
      continue;
    }
    
    /* Also check if it's a parent of the current stack */
    int is_parent_stack = 0;
    struct stack_info* check = domain_state->current_stack;
    while (check != NULL) {
      if (Stack_parent(check) == stk) {
        is_parent_stack = 1;
        break;
      }
      check = Stack_parent(check);
    }
    if (is_parent_stack) {
      caml_gc_log("  cont_ll_minor: Unreachable cont %p points to parent stack, skipping", (void*)cur);
      cur = next_in_minor;
      continue;
    }
    
    /* Check stack magic number for validity */
    if (stk->magic != 42) {
      caml_gc_log("  cont_ll_minor: Unreachable cont %p has invalid stack (magic=%lu), skipping", 
                  (void*)cur, (unsigned long)stk->magic);
      cur = next_in_minor;
      continue;
    }
    
    caml_gc_log("  cont_ll_minor: Unreachable cont %p with valid stack %p, promoting to toclean", 
                (void*)cur, (void*)stk);
    
    /* Promote the continuation using oldify_fn.
       This will allocate in major heap, set up forwarding, and scan the stack. */
    volatile value promoted_cont = Val_long(0);
    oldify_fn(oldify_state, cur, &promoted_cont);
    
    /* After oldify, if the continuation was promoted, add to toclean */
    if (Is_block(promoted_cont) && !is_young(promoted_cont)) {
      caml_gc_log("  cont_ll_minor: Promoted unreachable cont to %p, adding to toclean", 
                  (void*)promoted_cont);
      caml_cont_ll_insert_toclean(promoted_cont);
    }
    
    /* Move to next. After promotion, next_in_minor still points to the 
       original minor heap address. */
    cur = next_in_minor;
  }
  
  /* Clear the minor todo list since all continuations have been processed */
  Caml_state->cont_minor_todo_head = Val_long(0);
  
  caml_gc_log("cont_ll_minor: Finished processing minor todo list");
}
