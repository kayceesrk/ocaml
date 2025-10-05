/* Prototypes for continuation doubly-linked list helpers */
#ifndef CAML_CONT_DLL_H
#define CAML_CONT_DLL_H

#include "mlvalues.h"

CAMLextern void caml_cont_dll_init(void);
CAMLextern void caml_cont_dll_insert_todo(value cont);
CAMLextern void caml_cont_dll_insert_live(value cont);
CAMLextern value caml_cont_dll_delete(value cont);
CAMLextern void caml_cont_dll_scan_todo_c(void (*cb)(value, void*), void *data);
CAMLextern void caml_cont_dll_scan_todo_ocaml(value f);
CAMLextern value caml_cont_dll_get_todo_head(void);
CAMLextern value caml_cont_dll_get_live_head(void);
CAMLextern void caml_cont_dll_print(const char *tag);

#endif /* CAML_CONT_DLL_H */
