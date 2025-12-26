open Effect
open Effect.Deep

(* When the GC encounter an leaked continuation it tries to close all the file file descriptors that fiber is holding *)

type _ Effect.t += Choose : bool Effect.t

let coin_flip () =
  (* Create a temporary file and open a channel to it. *)
  let temp_file_name = Filename.temp_file "leak_test_file" ".tmp" in
  let channel = open_in temp_file_name in
  let _ = Sys.opaque_identity channel in
  print_endline ("Opened file (and did not close it): " ^ temp_file_name);
  if perform Choose then "Heads" else "Tails"

let () =
  match coin_flip () with
  | result -> print_endline result
  | effect Choose, k ->
      (* Gc.minor (); *)
      (* Keep the continuation k alive but don't resume it. *)
      let _ = Sys.opaque_identity [k] in
      print_endline "Continuation ignored and Gc.full_major calling";
      Gc.full_major ();
      print_endline "Major GC Completed."