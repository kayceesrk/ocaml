open Effect
open Effect.Deep

(* This Program is for the GC which Calls Caml_Fatel_Error when it encounters a leaked Continuation *)

type _ Effect.t += Choose : bool Effect.t

let coin_flip () =
  if perform Choose then "Heads" else "Tails"

let () =
  match coin_flip () with
  | result -> print_endline result
  | effect Choose, k ->
      Gc.minor ();
      let _ = Sys.opaque_identity [k] in
      print_endline "Continuation ignored and Gc.full_major calling";
      Gc.full_major ();
      print_endline "After Major GC call, Will Not reach here"