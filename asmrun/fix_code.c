#include <stddef.h>
#include <caml/mlvalues.h>

unsigned int *caml_profile_counts = NULL;
code_t profile_pc = NULL;
code_t caml_start_code = NULL;
