/* Prototypes for continuation singly-linked list helpers */
#ifndef CAML_CONT_LL_H
#define CAML_CONT_LL_H

#include "mlvalues.h"

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

#endif /* CAML_CONT_LL_H */
