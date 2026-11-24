(* TEST
 multicore;
 {
   bytecode;
 }{
   native;
 }
*)

(* [Effect.Deep.get_callstack] on one domain reads the stack of a
   continuation without resuming it. A [continue] of that continuation on
   another domain at the same time must succeed. The reader loops on
   [get_callstack] while the owner continues; the interleavings are not
   forced. *)

open Effect
open Effect.Deep

type _ t += E : unit t

let gen () : (unit, int) continuation =
  let r : (unit, int) continuation option ref = ref None in
  let rec deep n = if n = 0 then (perform E; 0) else 1 + deep (n - 1) in
  ignore (match_with (fun () -> deep 2000) ()
    { retc = (fun x -> x);
      exnc = raise;
      effc = fun (type a) (e : a t) ->
        match e with
        | E -> Some (fun (k : (a, int) continuation) -> r := Some k; 0)
        | _ -> None });
  Option.get !r

let trial () =
  let k = gen () in
  let stop = Atomic.make false and started = Atomic.make false in
  let reader = Domain.spawn (fun () ->
    Atomic.set started true;
    try
      while not (Atomic.get stop) do
        ignore (Sys.opaque_identity (Deep.get_callstack k 50))
      done
    with Continuation_already_resumed -> ()) in
  while not (Atomic.get started) do Domain.cpu_relax () done;
  for _ = 1 to 2000 do Domain.cpu_relax () done;
  let result =
    match continue k () with
    | n -> string_of_int n
    | exception Continuation_already_resumed -> "Continuation_already_resumed"
  in
  Atomic.set stop true;
  Domain.join reader;
  result

let () =
  let failures = ref 0 in
  for _ = 1 to 50 do if trial () <> "2000" then incr failures done;
  Printf.printf "continue during get_callstack: %d failures of 50\n" !failures
