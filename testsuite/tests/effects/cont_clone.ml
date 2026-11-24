(* TEST
 modules = "cont_clone_.c";
 ocamlrunparam += ",Xcache_stacks_per_class=0";
 {
   bytecode;
 }{
   native;
 }
*)

(* [caml_continuation_clone] copies a continuation without resuming it:
   both copies resume, once each, a dropped copy is reclaimed, a chain of
   fibers is copied with its handlers, and a resumed continuation cannot
   be cloned. The stack cache is off, so a freed stack is released. *)

open Effect
open Effect.Deep

external clone : ('a, 'b) continuation -> ('a, 'b) continuation
  = "cont_clone_stub"

type _ t += E : int t | F : unit t

let stacks () = (Gc.quick_stat ()).Gc.live_stacks_words

(* [E] is performed [depth] frames down, below an exception handler and,
   if [chain], across the handler of [F] (another fiber). *)
let gen ~chain depth : (int, int) continuation =
  let r : (int, int) continuation option ref = ref None in
  let rec deep n = if n = 0 then perform E else 1 + deep (n - 1) in
  let body () = try deep depth with Not_found -> -1 in
  let body () =
    if not chain then body ()
    else try body () with effect F, k -> continue k ()
  in
  ignore (match body () with
    | x -> x
    | effect E, k -> r := Some k; 0);
  Option.get !r

let () =
  List.iter (fun chain ->
    let k = gen ~chain 2000 in
    let k' = clone k in
    let a = continue k 1 and b = continue k' 2 in
    let c = discontinue (clone (gen ~chain 10)) Not_found in
    Printf.printf "chain %b: resumed %d %d, discontinued %d\n" chain a b c)
    [false; true];
  Gc.full_major ();
  let before = stacks () in
  (let k = gen ~chain:true 2000 in
   ignore (Sys.opaque_identity (clone k));
   ignore (continue k 0));
  Gc.full_major ();
  Gc.full_major ();
  Printf.printf "dropped copy: reclaimed %b\n" (stacks () - before < 1000);
  let k = gen ~chain:false 10 in
  ignore (continue k 0);
  match clone k with
  | _ -> print_endline "clone of a resumed continuation: no exception"
  | exception Continuation_already_resumed ->
    print_endline "clone of a resumed continuation: raised"
