/**************************************************************************/
/*                                                                        */
/*                                 OCaml                                  */
/*                                                                        */
/*          Xavier Leroy and Damien Doligez, INRIA Rocquencourt           */
/*                                                                        */
/*   Copyright 1996 Institut National de Recherche en Informatique et     */
/*     en Automatique.                                                    */
/*                                                                        */
/*   All rights reserved.  This file is distributed under the terms of    */
/*   the GNU Lesser General Public License version 2.1, with the          */
/*   special exception on linking described in the file LICENSE.          */
/*                                                                        */
/**************************************************************************/

#include <errno.h>
#include <limits.h>
#include "caml/alloc.h"
#include "caml/backtrace_prim.h"
#include "caml/fix_code.h"
#include "caml/memory.h"
#include "caml/mlvalues.h"
#include "caml/stacks.h"

#ifndef NATIVE_CODE

unsigned int *caml_profile_counts = NULL;
unsigned int *caml_profile_stack_counts = NULL;
long caml_profile_stack_depth = 0;
code_t *caml_profile_stack_info = NULL;
char* prof_file = NULL;
CAMLextern code_t *caml_profile_stack_info;
CAMLextern unsigned int *caml_profile_stack_counts;
CAMLextern long caml_profile_stack_depth;

CAMLprim value caml_output_profile (value unit) {
  CAMLparam1 (unit);
  if (prof_file != NULL) {
    int i;
    FILE* fp = fopen (prof_file, "w");
    struct caml_loc_info li = {0};

    if (!caml_debug_info_available()) {
      fprintf(stderr, "(Cannot output profile: Debug information not available)\n");
      CAMLreturn (Val_unit);
    }

    /* Write out the profile */
    if (caml_profile_stack_depth)
      fprintf (fp, "__STACK__\n");
    else
      fprintf (fp, "__FLAT__\n");

    for (i = 0; i < caml_code_size; i++) {
      if (caml_profile_counts[i] != 0 ||
          (caml_profile_stack_counts &&
           caml_profile_stack_counts[i] != 0)) {
        caml_extract_containing_location_info (caml_start_code + i, 1, &li);
        if (li.loc_valid) {
          if (caml_profile_stack_depth) {
            fprintf (fp, "%d\t%u\t%u\tfile \"%s\", line %d, characters %d-%d\n",
                    i, caml_profile_counts[i], caml_profile_stack_counts[i],
                    li.loc_filename, li.loc_lnum, li.loc_startchr, li.loc_endchr);
          } else {
            fprintf (fp, "%d\t%u\tfile \"%s\", line %d, characters %d-%d\n",
                    i, caml_profile_counts[i], li.loc_filename, li.loc_lnum,
                    li.loc_startchr, li.loc_endchr);
          }
        }
        else {
          if (caml_profile_stack_depth) {
            fprintf (fp, "%d\t%u\t%u\n", i, caml_profile_counts[i],
                     caml_profile_stack_counts[i]);
          } else {
            fprintf (fp, "%d\t%u\n", i,caml_profile_counts[i]);
          }
        }
      }
    }
    fflush(fp);
    fclose(fp);
  }
  CAMLreturn (Val_unit);
}

/* Initialize the profiler */
void caml_init_profiler () {
  prof_file = getenv("CAML_PROFILE_ALLOC");
  if (prof_file != NULL) {
    int i, base = 10;
    char *endptr, *str;
    errno = 0;
    str = getenv("CAML_PROFILE_STACK");
    if (str != NULL) {
      caml_profile_stack_depth = strtol(str, &endptr, base);

      /* Check for various possible errors */
      if ((errno == ERANGE && (caml_profile_stack_depth == LONG_MAX
                              || caml_profile_stack_depth == LONG_MIN))
          || (errno != 0 && caml_profile_stack_depth == 0)) {
        fprintf (stderr, "Error processing CAML_PROFILE_STACK\n");
        perror("strtol");
        exit(EXIT_FAILURE);
      }

      if (endptr == str) {
        fprintf(stderr, "CAML_PROFILE_STACK: No digits were found\n");
        exit(EXIT_FAILURE);
      }

      if (caml_profile_stack_depth < 1) {
        fprintf(stderr, "CAML_PROFILE_STACK: depth must be greater than 0\n");
        exit(EXIT_FAILURE);
      }
      /* Successfully parsed stack depth */
    }

    caml_profile_counts =
      (unsigned int *) caml_stat_alloc (caml_code_size * sizeof(unsigned int));
    if (caml_profile_stack_depth) {
      caml_profile_stack_counts =
        (unsigned int *) caml_stat_alloc (caml_code_size * sizeof(unsigned int));
      caml_profile_stack_info =
        (code_t*) caml_stat_alloc (caml_profile_stack_depth * sizeof(code_t));
    }
    for (i = 0; i < caml_code_size; i++) {
      caml_profile_counts[i] = 0;
      if (caml_profile_stack_depth)
        caml_profile_stack_counts[i] = 0;
    }
  }
}

static int unseen_frame_pointer (int next_idx, code_t p) {
  int i;
  for (i = next_idx - 1; i >= 0; i--) {
    if (caml_profile_stack_info[i] == p)
      return 0;
  }
  return 1;
}

void caml_update_stack_profile (mlsize_t wosize)
{
  value* sp = caml_extern_sp;
  value* trsp = caml_trapsp;
  long i;
  int next_idx = 0;

  for (i = 0; i < caml_profile_stack_depth; i++) {
    code_t p = caml_next_frame_pointer (&sp, &trsp);
    if (p == NULL) {
      return;
    }
    /* Count only once for recursive calls. */
    if (unseen_frame_pointer (next_idx, p)) {
      caml_profile_stack_counts[p - caml_start_code] += wosize;
      caml_profile_stack_info[next_idx++] = p;
    }
  }
}


#else

CAMLprim value caml_output_profile (value unit) {
  CAMLparam1 (unit);
  CAMLreturn (Val_unit);
};

void caml_init_profiler () {};

void caml_update_stack_profile (mlsize_t wosize) {};

#endif
