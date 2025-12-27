/* Prototypes for continuation singly-linked list helpers */
#ifndef CAML_CONT_LL_H
#define CAML_CONT_LL_H

#include "mlvalues.h"

/* ========================================================================== */
/* Major GC continuation tracking                                             */
/* ========================================================================== */

CAMLextern void caml_cont_ll_init(void);
CAMLextern void caml_cont_ll_insert_todo(value cont);
CAMLextern void caml_cont_ll_insert_toclean(value cont);
CAMLextern void caml_cont_ll_scan_todo_c(void (*cb)(value, void*), void *data);
CAMLextern void caml_cont_ll_scan_todo_ocaml(value f);
CAMLextern value caml_cont_ll_get_todo_head(void);
CAMLextern value caml_cont_ll_get_toclean_head(void);
CAMLextern void caml_cont_ll_print(const char *tag);
CAMLextern void caml_cont_mark_and_shift_toclean(void);
CAMLextern void caml_discontinue_toclean(void);

/* ========================================================================== */
/* Minor GC continuation tracking                                             */
/* ========================================================================== */

/* Per-domain minor heap continuation todo list.
   This tracks continuations allocated in the minor heap. During minor GC:
   - If continuation is collected (NULL), skip it
   - If continuation is already promoted, add to major GC todo list
   - If continuation is unreachable and not promoted, promote it and add to toclean
*/

/* Initialize the minor continuation list for a domain. Called during domain setup. */
CAMLextern void caml_cont_ll_minor_init(void);

/* Insert a minor heap continuation into the minor todo list.
   This should be called when a continuation is allocated in the minor heap. */
CAMLextern void caml_cont_ll_insert_minor_todo(value cont);

/* Get the head of the minor todo list (for debugging/testing). */
CAMLextern value caml_cont_ll_get_minor_todo_head(void);

/* Clear the minor todo list. Called after minor GC processing. */
CAMLextern void caml_cont_ll_clear_minor_todo(void);

/* Process the minor todo list during minor GC.
   This function should be called after promotion phase of minor GC.
   It handles:
   1. Continuations that were promoted normally - move to major todo list
   2. Unreachable continuations - promote explicitly and add to toclean list
   3. Collected/NULL continuations - skip
   
   Parameters:
   - oldify_fn: The oldify_one function for promoting objects
   - oldify_state: State for the oldify function
   - domain: The current domain state
*/
CAMLextern void caml_cont_ll_process_minor_todo(
  void (*oldify_fn)(void*, value, volatile value*),
  void* oldify_state,
  void* domain);

/* Print the minor todo list for debugging */
CAMLextern void caml_cont_ll_print_minor(const char *tag);

#endif /* CAML_CONT_LL_H */
