/* Doubly-linked list helpers for continuation objects
   Each continuation is an OCaml block with tag Cont_tag and at least
   four fields. We use Field(cont,2) as "prev" and Field(cont,3) as "next".

   This module maintains two global lists of continuations:
     - todo list (continuations to be processed)
     - live list (continuations considered live)

   It exposes C helpers to insert/delete a continuation from a list and
   to scan the todo list applying either a C callback or an OCaml closure.
*/

#define CAML_INTERNALS

#include "caml/config.h"
#include "caml/mlvalues.h"
#include "caml/memory.h"
#include "caml/callback.h"
#include "caml/misc.h"
#include "caml/alloc.h"
#include "caml/cont_dll.h"

/* Heads of the two lists. Registered as global roots so the GC follows
   the lists. Initialized to the integer 0 (null). */
static value todo_head = Val_long(0);
static value live_head = Val_long(0);

/* Initialization: register the heads as global roots. Call at startup
   from the runtime init sequence (or from C code that uses these lists). */
CAMLexport void caml_cont_dll_init(void)
{
  caml_register_global_root(&todo_head);
  caml_register_global_root(&live_head);
}

/* Internal helpers for accessing prev/next fields in a continuation block.
   We assume the continuation has at least 4 fields and that fields 2 and
   3 are reserved for prev and next pointers respectively. */
Caml_inline value cont_prev(value cont) { return Field(cont, 2); }
Caml_inline value cont_next(value cont) { return Field(cont, 3); }

/* Insert [cont] at the head of the todo list. If the continuation is
   already linked in any list, unlink it first. */
CAMLexport void caml_cont_dll_insert_todo(value cont)
{
  /* defensive: only operate on blocks */
  if (Is_block(cont)) {
    /* unlink from any list first */
    {
        value prev = cont_prev(cont);
        value next = cont_next(cont);
        if (Is_block(prev) || Is_block(next) || todo_head == cont || live_head == cont) {
        /* call delete logic below by reusing the exported function */
        caml_cont_dll_delete(cont);
        }
    }

    value head = todo_head;
    Field(cont, 2) = Val_long(0); /* prev = NULL */
    Field(cont, 3) = head;        /* next = old head */
    if (Is_block(head)) Field(head, 2) = cont;
    todo_head = cont;
  }
}

/* Insert [cont] at the head of the live list. */
CAMLexport void caml_cont_dll_insert_live(value cont)
{
  if (Is_block(cont)) {
    /* unlink from any list first */
    {
        value prev = cont_prev(cont);
        value next = cont_next(cont);
        if (Is_block(prev) || Is_block(next) || todo_head == cont || live_head == cont) {
        caml_cont_dll_delete(cont);
        }
    }

    value head = live_head;
    Field(cont, 2) = Val_long(0); /* prev = NULL */
    Field(cont, 3) = head;        /* next = old head */
    if (Is_block(head)) Field(head, 2) = cont;
    live_head = cont;
  }
}

/* Unlink [cont] from whichever list it belongs to. Safe to call even if
   the continuation is not linked. */
CAMLprim value caml_cont_dll_delete(value cont)
{
  if (!Is_block(cont)) return Val_unit;

  value prev = cont_prev(cont);
  value next = cont_next(cont);

  if (Is_block(prev)) {
    /* prev->next = next */
    Field(prev, 3) = next;
  } else {
    /* cont was a head in one (or both) lists; update heads if they point to cont */
    if (todo_head == cont) todo_head = next;
    if (live_head == cont) live_head = next;
  }

  if (Is_block(next)) {
    /* next->prev = prev */
    Field(next, 2) = prev;
  }

  /* clear links */
  Field(cont, 2) = Val_long(0);
  Field(cont, 3) = Val_long(0);
  return Val_unit;
}

/* C-level scanning: apply a C callback to each continuation in the todo list.
   The callback receives the continuation value and the user-provided data
   pointer. This is safe with respect to the GC because the list heads are
   registered global roots. We fetch the 'next' pointer before calling the
   callback in case the callback allocates or mutates the list. */
typedef void (*cont_cbu_t)(value cont, void *data);

CAMLexport void caml_cont_dll_scan_todo_c(cont_cbu_t cb, void *data)
{
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
CAMLexport void caml_cont_dll_scan_todo_ocaml(value f)
{
  CAMLparam1(f);
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
CAMLexport value caml_cont_dll_get_todo_head(void) { return todo_head; }
CAMLexport value caml_cont_dll_get_live_head(void) { return live_head; }

/* Debugging: print the two lists using caml_gc_log. [tag] is a short label
   indicating the context of the print (e.g. "before-sweep"). */
CAMLexport void caml_cont_dll_print(const char *tag)
{
  /* print header */
  caml_gc_log("cont_dll[%s]: todo_head=%p, live_head=%p", tag, (void*)todo_head, (void*)live_head);

  /* Walk todo list */
  value cur = todo_head;
  while (Is_block(cur)) {
    value prev = cont_prev(cur);
    value next = cont_next(cur);
    caml_gc_log("  todo: cur=%p prev=%p next=%p", (void*)cur, (void*)prev, (void*)next);
    cur = next;
  }

  /* Walk live list */
  cur = live_head;
  while (Is_block(cur)) {
    value prev = cont_prev(cur);
    value next = cont_next(cur);
    caml_gc_log("  live: cur=%p prev=%p next=%p", (void*)cur, (void*)prev, (void*)next);
    cur = next;
  }
}
