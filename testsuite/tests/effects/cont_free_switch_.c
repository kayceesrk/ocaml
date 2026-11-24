#define CAML_INTERNALS
#include "caml/mlvalues.h"
#include "caml/fiber.h"
#include "caml/domain_state.h"

value cont_free_table_size(value unit)
{
  return Val_long(caml_young_conts_size(Caml_state));
}
