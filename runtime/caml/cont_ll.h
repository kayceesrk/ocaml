/* Prototypes for continuation singly-linked list helpers */
#ifndef CAML_CONT_LL_H
#define CAML_CONT_LL_H

#include "mlvalues.h"

/* ========================================================================== */
/* Major GC                                                                   */
/* ========================================================================== */

CAMLextern void caml_cont_insert_major_todo(value cont);
CAMLextern void caml_cont_insert_major_toclean(value cont);
CAMLextern void caml_cont_print_major(const char *tag);
CAMLextern int caml_cont_mark_and_shift_toclean(void);
CAMLextern void caml_cont_discontinue_toclean(void);

/* ========================================================================== */
/* Minor GC                                                                   */
/* ========================================================================== */

CAMLextern void caml_cont_insert_minor_todo(value cont);
CAMLextern void caml_cont_process_minor_todo(
  void (*oldify_fn)(void*, value, volatile value*),
  void* oldify_state,
  caml_domain_state* domain_state);
CAMLextern void caml_cont_print_minor(const char *tag);

#endif /* CAML_CONT_LL_H */
