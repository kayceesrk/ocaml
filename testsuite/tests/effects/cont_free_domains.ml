(* TEST
 multicore;
 {
   bytecode;
 }{
   native;
 }
*)

(* Continuations made on a domain that then terminates, resumed on other
   domains while those collect and compact: a resumed continuation runs on
   its stacks, which are never freed under it; the dropped ones are
   reclaimed. *)

open Effect
open Effect.Deep

type _ t += Y : unit t

(* Resuming performs again: that continuation is dropped where it runs. *)
let gen () : (unit, unit) continuation option =
  let r : (unit, unit) continuation option ref = ref None in
  match_with (fun () -> perform Y; perform Y) ()
    { retc = (fun () -> ());
      exnc = raise;
      effc = fun (type a) (e : a t) ->
        match e with
        | Y -> Some (fun (k : (a, _) continuation) -> r := Some k)
        | _ -> None };
  !r

let stacks () = (Gc.quick_stat ()).Gc.live_stacks_words

let rounds = 20 and per_round = 2000
let resumed = Atomic.make 0

let resume ks lo hi collect = Domain.spawn (fun () ->
  for i = lo to hi - 1 do
    if i mod 2 = 0 then begin
      continue (Option.get ks.(i)) ();
      Atomic.incr resumed;
      if i mod 100 = 0 then Gc.minor ()
    end
  done;
  collect ())

let () =
  let before = stacks () in
  for _ = 1 to rounds do
    let ks = Domain.join (Domain.spawn (fun () ->
      let ks = Array.init per_round (fun _ -> gen ()) in
      Gc.minor ();
      ks)) in
    let half = per_round / 2 in
    let d1 = resume ks 0 half Gc.full_major in
    let d2 = resume ks half per_round Gc.compact in
    Domain.join d1;
    Domain.join d2
  done;
  Gc.full_major ();
  Gc.full_major ();
  Printf.printf "resumed %d of %d\n" (Atomic.get resumed)
    (rounds * per_round / 2);
  (* Without reclamation, the dropped continuations hold about 40000 stacks
     of at least 64 words. *)
  Printf.printf "reclaimed: %b\n" (stacks () - before < 200_000)
