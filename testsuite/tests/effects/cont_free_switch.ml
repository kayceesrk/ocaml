(* TEST
 modules = "cont_free_switch_.c";
 ocamlrunparam += ",Xcache_stacks_per_class=0";
 multicore;
 {
   bytecode;
 }{
   native;
 }
*)

(* A scheduler that switches many times between a few fibers keeps the
   table of young continuations small (it is compacted when it fills), and
   still reclaims the fibers it drops. *)

open Effect
open Effect.Deep

external table_size : unit -> int = "cont_free_table_size" [@@noalloc]

type _ t += Yield : unit t

let stacks () = (Gc.quick_stat ()).Gc.live_stacks_words

(* Round robin over [n] fibers for [switches] yields, then drop half of
   the suspended fibers and run the others to the end. *)
let schedule n switches =
  let q : (unit, unit) continuation Queue.t = Queue.create () in
  let left = ref switches and finished = ref 0 in
  let rec fiber () = if !left > 0 then (decr left; perform Yield; fiber ()) in
  for _ = 1 to n do
    match fiber () with
    | () -> incr finished
    | effect Yield, k -> Queue.add k q
  done;
  while !left > 0 do continue (Queue.pop q) () done;
  for _ = 1 to n / 2 do ignore (Queue.pop q) done;
  left := 0;
  while not (Queue.is_empty q) do continue (Queue.pop q) () done;
  !finished

(* In a fresh domain, whose table starts with 256 entries. *)
let () =
  let before = stacks () in
  let finished, size = Domain.join (Domain.spawn (fun () ->
    let f = schedule 50 200_000 in
    f, table_size ())) in
  Gc.full_major ();
  Gc.full_major ();
  Printf.printf "finished %d, table %d entries, dropped reclaimed %b\n"
    finished size (stacks () - before < 1000)
