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

#define CAML_CONFIG_H_NO_TYPEDEFS
#include "config.h"

type t =
#define DOMAIN_STATE(type, name) | Domain_##name
#include "domain_state.tbl"
#undef DOMAIN_STATE

let idx_of_field =
  let curr = 0 in
#define DOMAIN_STATE(type, name) \
  let idx__##name = curr in \
  let curr = curr + 1 in
#include "domain_state.tbl"
#undef DOMAIN_STATE
  let _ = curr in
  function
#define DOMAIN_STATE(type, name) \
  | Domain_##name -> idx__##name
#include "domain_state.tbl"
#undef DOMAIN_STATE
