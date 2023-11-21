

type dls_state = Obj.t array

val unique_value : Obj.t

val get_dls

external get_dls_state : unit -> dls_state = "%tls_get"

external set_dls_state : dls_state -> unit =
  "caml_domain_dls_set" [@@noalloc]

type 'a key = int * (unit -> 'a)

type key_initializer =
  KI: 'a key * ('a -> 'a) -> key_initializer

val add_parent_key : _ key -> unit

val set : 'a key -> 'a -> unit

val get : 'a key -> 'a

val get_initial_keys : unit -> (int * Obj.t) list

val set_initial_keys : (int * Obj.t) list) -> unit
