/* linkedlist helpers for continuation objects
   Each continuation is an OCaml block with tag Cont_tag and at least
   three fields. We use Field(cont,2) as "next" pointer for singly-linked list.

   This module maintains two global lists of continuations:
     - todo list (continuations to be processed)
     - toclean list (continuations considered as dead)

   It exposes C helpers to insert a continuation into a list and
   to scan the todo list applying either a C callback or an OCaml closure.
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

/* Heads of the two lists. Only [toclean_head] is registered as a global root
  so we can keep those continuations alive until we run discontinue on them.
  The [todo_head] is NOT a GC root on purpose: we want to detect which
  continuations become unreachable by normal roots after the marking phase.
  Initialized to the integer 0 (null). */
static value todo_head = Val_long(0);
static value toclean_head = Val_long(0);

/* Guard flags to prevent re-entrant processing during GC or discontinuation */
static int processing_mark_and_shift = 0;
static int processing_discontinue = 0;

/* Initialization: register the heads as global roots. Call at startup
   from the runtime init sequence (or from C code that uses these lists). */
CAMLexport void caml_cont_ll_init(void)
{
  /* Only root the toclean list; todo is intentionally not a root. */
  caml_register_global_root(&toclean_head);
}

/* Internal helper for accessing next field in a continuation block.
   We assume the continuation has at least 3 fields and that field 2
   is reserved for next pointer. */

static int is_valid_cont(value v) {
  return Is_block(v) && Tag_val(v) == Cont_tag && Wosize_val(v) >= 3;
}

Caml_inline value cont_next(value cont) {
  return is_valid_cont(cont) ? Field(cont, 2) : Val_long(0);
}

/* Insert [cont] at the head of the todo list.
   Skip over any already-collected continuations at the head (field 0 == NULL)
   and insert before the first non-collected node. */
CAMLexport void caml_cont_ll_insert_todo(value cont)
{ 
  /* Skip over used continuations at the head to find first non-collected node.
     Note: Field(cont, 0) is set to Val_ptr(NULL) when continuation is used. */
  while (Is_block(todo_head) && Field(todo_head, 0) == Val_ptr(NULL)) {
    todo_head = Field(todo_head, 2);  /* move to next */
  }

  /* defensive: only operate on blocks */
  if (!is_valid_cont(cont)) return;
  
  Field(cont, 2) = todo_head;  /* new node points to head */
  
  todo_head = cont; /* new node becomes the head */
}

/* Insert [cont] at the head of the toclean list. */
CAMLexport void caml_cont_ll_insert_toclean(value cont)
{
  if (is_valid_cont(cont)) {
    Field(cont, 2) = toclean_head;        /* next = old head */
    toclean_head = cont;
  }
}

/* Accessors for testing/inspection from C */
CAMLexport value caml_cont_ll_get_todo_head(void) { return todo_head; }
CAMLexport value caml_cont_ll_get_toclean_head(void) { return toclean_head; }

/* Debugging: print the two lists using caml_gc_log. [tag] is a short label
   indicating the context of the print (e.g. "before-sweep"). */
CAMLexport void caml_cont_ll_print(const char *tag)
{
  /* print header */
  caml_gc_log("cont_ll[%s]: todo_head=%p, toclean_head=%p", tag, (void*)todo_head, (void*)toclean_head);

  /* Walk todo list */
  int i=1;
  value cur = todo_head;
  while (Is_block(cur)) {
    if (!is_valid_cont(cur)) {
      caml_gc_log("  INVALID node in todo (Dropping the entire list): %p (tag=%d, wosize=%lu)",
                  (void*)cur, (int)Tag_val(cur), (unsigned long)Wosize_val(cur));
      todo_head = Val_long(0);
      break;
    }
    caml_gc_log("  todo: %d. cont=%p", i, (void*)cur);
    i++;
    cur = cont_next(cur);
  }

  /* Walk toclean list */
  i=1;
  cur = toclean_head;
  while (Is_block(cur)) {
    if (!is_valid_cont(cur)) {
      caml_gc_log("  INVALID node in toclean (Dropping the entire list): %p (tag=%d, wosize=%lu)",
                  (void*)cur, (int)Tag_val(cur), (unsigned long)Wosize_val(cur));
      toclean_head = Val_long(0);
      break;
    }
    caml_gc_log("  toclean: %d. cont=%p", i, (void*)cur);
    i++;
    cur = cont_next(cur);
  }
}

/* Process todo list after marking phase:
   - Remove already-collected continuations (field 0 is null)
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
  
  /* Skip over used continuations at the head to find first non-collected node.
     Note: Field(cont, 0) is set to Val_ptr(NULL) (i.e., 0) when continuation is used. */
  while (Is_block(todo_head) && Field(todo_head, 0) == Val_ptr(NULL)) {
    caml_gc_log("  Removing Used cont: %p", (void*)todo_head);
    todo_head = Field(todo_head, 2);  /* move to next */
  }
  value cur = todo_head;
  value prev = Val_long(0);  /* previous node (or 0 if at head) */
  
  
  /* Walk through todo list */
  while (Is_block(cur)) {
    value next = cont_next(cur);
    
    /* Check if continuation is already used (field 0 is null pointer, not tagged 0).
       When discontinue/continue is called, caml_continuation_use_noexc sets
       Field(cont, 0) to Val_ptr(NULL), not Val_long(0). */
    if (Field(cur, 0) == Val_ptr(NULL)) {
      caml_gc_log("  Removing Used cont: %p", (void*)cur);
      /* Remove from todo list by skipping this node */
      if (prev == Val_long(0)) {
        todo_head = next;
      } else {
        Field(prev, 2) = next;
      }
    } else if (Is_long(Field(cur, 0))) {
      /* Continuation not yet initialized by perform.
         Check if it's still reachable - if unmarked, remove it. */
      if (!is_valid_cont(cur)) {
        caml_gc_log("  INVALID uninitialized cont (Dropping the entire list): %p", (void*)cur);
        todo_head = Val_long(0);
        processing_mark_and_shift = 0;
        return;
      }
      
      header_t hd = Hd_val(cur);
      if (Has_status_hd(hd, caml_global_heap_state.UNMARKED)) {
        /* Uninitialized and unreachable - remove it */
        caml_gc_log("  Removing unreachable uninitialized cont: %p", (void*)cur);
        if (prev == Val_long(0)) {
          todo_head = next;
        } else {
          Field(prev, 2) = next;
        }
      } else {
        /* Still marked/reachable, keep it for next cycle */
        caml_gc_log("  Keeping reachable uninitialized cont: %p", (void*)cur);
        prev = cur;
      }
    } else {
      if (!is_valid_cont(cur)) {
        caml_gc_log("  INVALID node in todo (Dropping the entire list): %p (tag=%d, wosize=%lu)",
                    (void*)cur, (int)Tag_val(cur), (unsigned long)Wosize_val(cur));
        todo_head = Val_long(0);
        processing_mark_and_shift = 0;
        return;
      }

      header_t hd = Hd_val(cur);
      
      /* Check if unmarked */
      if (Has_status_hd(hd, caml_global_heap_state.UNMARKED)) {
        /* Remove from todo list */
        if (prev == Val_long(0)) {
          todo_head = next;
        } else {
          Field(prev, 2) = next;
        }
        
        /* Insert into toclean list */
        Field(cur, 2) = toclean_head;
        toclean_head = cur;

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
  cur = toclean_head;
  int darkened_any = 0;
  while (Is_block(cur)) {
    if (!is_valid_cont(cur)) {
      caml_gc_log("  INVALID node in toclean (Dropping the entire list): %p (tag=%d, wosize=%lu)",
                  (void*)cur, (int)Tag_val(cur), (unsigned long)Wosize_val(cur));
      toclean_head = Val_long(0);
      processing_mark_and_shift = 0;
      return;
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
  if (!Is_block(toclean_head)) {
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
  while (Is_block(toclean_head)) {
    cur = toclean_head;
    if (!is_valid_cont(cur)) {
      caml_gc_log("  INVALID node in toclean (Dropping the entire list): %p (tag=%d, wosize=%lu)",
                  (void*)cur, (int)Tag_val(cur), (unsigned long)Wosize_val(cur));
      toclean_head = Val_long(0);
      processing_discontinue = 0;
      CAMLreturn0;
    }
    
    /* Remove from list BEFORE discontinuing to avoid dangling references */
    value next = cont_next(cur);
    Field(cur, 2) = Val_long(0);  /* Clear the next pointer */
    toclean_head = next;

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
  toclean_head = Val_long(0);
  
  caml_gc_log("cont_ll: Finished discontinuing toclean list");
  processing_discontinue = 0;
  CAMLreturn0;
}

/* ========================================================================== */
/* Minor GC continuation tracking                                             */
/* ========================================================================== */

/* Head of the minor todo list. This is NOT a GC root - we only store
   minor heap continuations here, and they get processed during minor GC.
   Initialized to the integer 0 (null). */
static value minor_todo_head = Val_long(0);

/* Initialize the minor continuation list */
CAMLexport void caml_cont_ll_minor_init(void)
{
  minor_todo_head = Val_long(0);
}

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
  while (Is_block(minor_todo_head) && is_young(minor_todo_head) && 
         Field(minor_todo_head, 0) == Val_ptr(NULL)) {
    minor_todo_head = Field(minor_todo_head, 2);
  }
  
  Field(cont, 2) = minor_todo_head;
  minor_todo_head = cont;
}

/* Get the head of the minor todo list */
CAMLexport value caml_cont_ll_get_minor_todo_head(void)
{
  return minor_todo_head;
}

/* Clear the minor todo list */
CAMLexport void caml_cont_ll_clear_minor_todo(void)
{
  minor_todo_head = Val_long(0);
}

/* Print the minor todo list for debugging */
CAMLexport void caml_cont_ll_print_minor(const char *tag)
{
  caml_gc_log("cont_ll_minor[%s]: minor_todo_head=%p", tag, (void*)minor_todo_head);
  
  int i = 1;
  value cur = minor_todo_head;
  while (Is_block(cur)) {
    if (!is_valid_cont(cur)) {
      caml_gc_log("  INVALID node in minor_todo: %p", (void*)cur);
      break;
    }
    caml_gc_log("  minor_todo: %d. cont=%p (young=%d)", i, (void*)cur, is_young(cur));
    i++;
    cur = cont_next(cur);
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
  
  value cur = minor_todo_head;
  
  while (Is_block(cur)) {
    /* We must ALWAYS save the next pointer from the minor heap location
       before doing anything else, because promotion can change the memory */
    value next_in_minor = Val_long(0);
    if (is_young(cur) && is_valid_cont(cur)) {
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
      
      /* The forwarded continuation is in major heap, add to major todo ONCE */
      if (Is_block(forwarded) && is_valid_cont(forwarded) && 
          Field(forwarded, 0) != Val_ptr(NULL)) {
        caml_cont_ll_insert_todo(forwarded);
      }
      
      /* Move to next using the saved minor heap pointer */
      if (Is_block(next_in_minor) && is_young(next_in_minor)) {
        /* Check if next was also forwarded */
        if (Hd_val(next_in_minor) == 0) {
          cur = Field(next_in_minor, 0);  /* Follow its forwarding pointer */
        } else {
          cur = next_in_minor;
        }
      } else {
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
    
    /* Check if continuation was used (field 0 is NULL pointer) */
    if (Field(cur, 0) == Val_ptr(NULL)) {
      caml_gc_log("  cont_ll_minor: Used cont %p, skipping", (void*)cur);
      /* Move to next using saved pointer */
      if (Is_block(next_in_minor) && is_young(next_in_minor) && Hd_val(next_in_minor) == 0) {
        cur = Field(next_in_minor, 0);
      } else {
        cur = next_in_minor;
      }
      continue;
    }
    
    /* Still in minor heap and not forwarded - this is unreachable!
       Promote it and add to toclean list.
       
       We use the oldify function to promote the continuation and its stack.
       This will also handle any objects reachable from the stack. */
    caml_gc_log("  cont_ll_minor: Unreachable cont %p, promoting to toclean", (void*)cur);
    
    /* Check if the continuation has a valid field 0 (stack pointer).
       If it's uninitialized (Val_long(0)) or invalid, skip it.
       Unhandled effect continuations might be in this state. */
    value stack_field = Field(cur, 0);
    if (Is_long(stack_field)) {
      /* Field 0 is an integer (likely 0) - continuation was never initialized.
         This can happen with unhandled effects. Skip it. */
      caml_gc_log("  cont_ll_minor: Continuation %p has uninitialized stack field (0x%lx), skipping",
                  (void*)cur, (unsigned long)stack_field);
      /* Move to next */
      if (Is_block(next_in_minor) && is_young(next_in_minor)) {
        if (Hd_val(next_in_minor) == 0) {
          cur = Field(next_in_minor, 0);
        } else {
          cur = next_in_minor;
        }
      } else {
        cur = next_in_minor;
      }
      continue;
    }
    
    /* Promote the continuation using oldify_fn.
       This will allocate in major heap, set up forwarding, and recursively
       promote the linked list chain. */
    volatile value promoted_cont = Val_long(0);
    oldify_fn(oldify_state, cur, &promoted_cont);
    
    /* After oldify, if the continuation was promoted, add to toclean */
    if (Is_block(promoted_cont) && !is_young(promoted_cont)) {
      caml_gc_log("  cont_ll_minor: Promoted unreachable cont to %p, adding to toclean", 
                  (void*)promoted_cont);
      caml_cont_ll_insert_toclean(promoted_cont);
    }
    
    /* Move to next using the saved minor heap pointer.
       The next continuation may have been promoted recursively, so check. */
    if (Is_block(next_in_minor) && is_young(next_in_minor)) {
      if (Hd_val(next_in_minor) == 0) {
        /* Next was forwarded (promoted), follow forwarding pointer */
        cur = Field(next_in_minor, 0);
        /* But since it was promoted as part of the chain, we should
           actually add it to major todo and stop following the chain
           to avoid duplicates */
        if (Is_block(cur) && is_valid_cont(cur) && Field(cur, 0) != Val_ptr(NULL)) {
          caml_gc_log("  cont_ll_minor: Next in chain was promoted to %p, adding to major todo", (void*)cur);
          caml_cont_ll_insert_todo(cur);
        }
        /* Stop following this chain since it was all promoted together */
        break;
      } else {
        /* Next is still in minor heap, continue processing */
        cur = next_in_minor;
      }
    } else {
      /* End of list */
      cur = next_in_minor;
    }
  }
  
  /* Clear the minor todo list since all continuations have been processed */
  minor_todo_head = Val_long(0);
  
  caml_gc_log("cont_ll_minor: Finished processing minor todo list");
}
