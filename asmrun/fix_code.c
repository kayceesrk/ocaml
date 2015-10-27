#include <stddef.h>
#include <caml/mlvalues.h>

unsigned int *caml_profile_stack_counts = NULL;
long caml_profile_stack_depth = 0;
code_t *caml_profile_stack_info = NULL;
unsigned int *caml_profile_counts = NULL;
code_t profile_pc = NULL;
code_t caml_start_code = NULL;
void caml_update_stack_profile (mlsize_t wosize) {
}
