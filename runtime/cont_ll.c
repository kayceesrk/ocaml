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
  return Is_block(v) && Tag_val(v) == Cont_tag && Wosize_val(v) == 3;
}

/* Remove collected or malformed continuations from the head of the todo list.
   This prevents later code from following or mutating memory that the GC has
   already reclaimed. Note that we only inspect fields after validating that
   the node really is a continuation block. */
static void prune_todo_prefix(void)
{
  value cur = todo_head;

  while (Is_block(cur)) {
    if (!is_valid_cont(cur)) {
      /* We cannot safely walk past an invalid node; drop the entire list. */
      todo_head = Val_long(0);
      return;
    }

    if (Field(cur, 0) != Val_long(0)) {
      /* Current head is still live; keep it. */
      return;
    }

    /* Continuation already collected/resumed: unlink it. */
    value next = cont_next(cur);
    Field(cur, 2) = Val_long(0);
    todo_head = next;
    cur = next;
  }
}

/* Insert [cont] at the head of the todo list.
   Skip over any already-collected continuations at the head (field 0 == 0)
   and insert before the first non-collected node. */
CAMLexport void caml_cont_ll_insert_todo(value cont)
{ 
  /* Skip over used continuations at the head to find first non-collected node */
  while (Is_block(todo_head) && Field(todo_head, 0) == Val_long(0)) {
    todo_head = Field(todo_head, 2);  /* move to next */
  }

  /* defensive: only operate on blocks */
  if (!Is_block(cont)) return;

  prune_todo_prefix();

  Field(cont, 2) = todo_head;
  todo_head = cont;
}

static void remove_from_list(value *head, value cont)
{
  value prev = Val_long(0);
  value cur = *head;

  while (Is_block(cur)) {
    if (cur == cont) {
      value next = cont_next(cur);
      if (prev == Val_long(0)) {
        *head = next;
      } else if (is_valid_cont(prev)) {
        Field(prev, 2) = next;
      }
      Field(cont, 2) = Val_long(0);
      return;
    }

    if (!is_valid_cont(cur)) {
      /* Encountered an invalid node while searching. Cut the list here to
         avoid following potentially reclaimed memory. */
      if (prev == Val_long(0)) {
        *head = Val_long(0);
      } else if (is_valid_cont(prev)) {
        Field(prev, 2) = Val_long(0);
      }
      return;
    }

    prev = cur;
    cur = cont_next(cur);
  }
}

CAMLexport void caml_cont_ll_remove(value cont)
{
  if (!is_valid_cont(cont)) return;

  remove_from_list(&todo_head, cont);
  remove_from_list(&toclean_head, cont);
}

/* Insert [cont] at the head of the toclean list. */
CAMLexport void caml_cont_ll_insert_toclean(value cont)
{
  if (is_valid_cont(cont)) {
    Field(cont, 2) = toclean_head;        /* next = old head */
    toclean_head = cont;
  }
}

/* C-level scanning: apply a C callback to each continuation in the todo list.
   The callback receives the continuation value and the user-provided data
   pointer. This is safe with respect to the GC because the list heads are
   registered global roots. We fetch the 'next' pointer before calling the
   callback in case the callback allocates or mutates the list. */
typedef void (*cont_cbu_t)(value cont, void *data);

CAMLexport void caml_cont_ll_scan_todo_c(cont_cbu_t cb, void *data)
{
  prune_todo_prefix();

  value cur = todo_head;
  while (Is_block(cur)) {
    value next = cont_next(cur);
    cb(cur, data);
    cur = next;
  }
}

/* OCaml-level scanning: apply an OCaml function [f] to each continuation
   in the todo list. The OCaml function is called with a single argument
   (the continuation). If the OCaml call raises, the exception is propagated
   and scanning stops. */
CAMLexport void caml_cont_ll_scan_todo_ocaml(value f)
{
  CAMLparam1(f);
  prune_todo_prefix();

  value cur = todo_head;
  while (Is_block(cur)) {
    value next = cont_next(cur);
    /* Use caml_callback_exn to call the closure [f] with [cur] */
    caml_callback_exn(f, cur);
    cur = next;
  }
  CAMLreturn0;
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
      caml_gc_log("  todo: INVALID continuation node %p (tag=%d)",
                  (void*)cur, (int)Tag_val(cur));
      break;
    }
    value next = cont_next(cur);
    caml_gc_log("  todo: cur=%p next=%p", (void*)cur, (void*)next);
    cur = next;
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
  prune_todo_prefix();

  value cur = todo_head;
  value prev = Val_long(0);  /* previous node (or 0 if at head) */

  caml_gc_log("cont_ll: Starting mark_and_shift_toclean");

  /* Walk through todo list */
  while (Is_block(cur)) {
    value next = cont_next(cur);

    /* Check if continuation is already collected (field 0 is null) */
    if (Field(cur, 0) == Val_long(0)) {
      caml_gc_log("  Removing Used cont: %p", (void*)cur);
      /* Remove from todo list by skipping this node */
      if (prev == Val_long(0)) {
        todo_head = next;
      } else {
        Field(prev, 2) = next;
      }
    } else {
      if (Is_young(cur)) {
        /* Newly created continuation still in the minor heap; keep it in todo
           until it is promoted so the major GC can track it normally. */
        prev = cur;
        caml_gc_log("  Continuation still young; keeping in todo: %p", (void*)cur);
        cur = next;
        continue;
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
    value next = is_valid_cont(cur) ? cont_next(cur) : Val_long(0);

    if (!Is_young(cur) && is_valid_cont(cur)) {
      caml_darken_cont(cur);
      darkened_any = 1;
    } else {
      caml_gc_log("    Skipping young toclean cont (still minor): %p", (void*)cur);
    }

    cur = next;
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
  
  /* Get the discontinue function from Effect module */
  const value *discontinue_fn = caml_named_value("Effect.discontinue");
  if (discontinue_fn == NULL) {
    caml_gc_log("cont_ll: Effect.discontinue not registered");
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
      caml_gc_log("  INVALID node in todo (Dropping the entire list): %p (tag=%d, wosize=%lu)",
                  (void*)cur, (int)Tag_val(cur), (unsigned long)Wosize_val(cur));
      toclean_head = Val_long(0);
      break;
    }
    toclean_head = cont_next(cur);
    Field(cur, 2) = Val_long(0);

    caml_gc_log("  Discontinuing cont: %p", (void*)cur);

    /* Call Effect.discontinue(cont, exn) */
    value result = caml_callback2_exn(discontinue_closure, cur, exn);

    /* Check if the callback raised an exception */
    if (Is_exception_result(result)) {
      caml_gc_log("  Warning: discontinue raised exception for cont %p", (void*)cur);
      /* Continue processing other continuations even if one fails */
    }
  }

  caml_gc_log("cont_ll: Finished discontinuing toclean list");
  CAMLreturn0;
}

