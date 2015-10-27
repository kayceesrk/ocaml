/***********************************************************************/
/*                                                                     */
/*                                OCaml                                */
/*                                                                     */
/*            Xavier Leroy, projet Cristal, INRIA Rocquencourt         */
/*                                                                     */
/*  Copyright 2001 Institut National de Recherche en Informatique et   */
/*  en Automatique.  All rights reserved.  This file is distributed    */
/*  under the terms of the GNU Library General Public License, with    */
/*  the special exception on linking described in file ../LICENSE.     */
/*                                                                     */
/***********************************************************************/

#ifndef CAML_BACKTRACE_H
#define CAML_BACKTRACE_H

#include "mlvalues.h"

CAMLextern int caml_backtrace_active;
CAMLextern int caml_backtrace_pos;
CAMLextern code_t * caml_backtrace_buffer;
CAMLextern value caml_backtrace_last_exn;
CAMLextern char * caml_cds_file;

CAMLprim value caml_record_backtrace(value vflag);
#ifndef NATIVE_CODE
extern void caml_stash_backtrace(value exn, code_t pc, value * sp, int reraise);
#endif
CAMLextern void caml_print_exception_backtrace(void);

struct loc_info {
  int loc_valid;
  int loc_is_raise;
  char * loc_filename;
  int loc_lnum;
  int loc_startchr;
  int loc_endchr;
};

void extract_location_info(code_t pc, int get_containing,
                           /*out*/ struct loc_info * li);
char* read_debug_info_with_error (void);
void caml_update_stack_profile (mlsize_t wosize);

#endif /* CAML_BACKTRACE_H */
