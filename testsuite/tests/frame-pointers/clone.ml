(* TEST
 modules = "clone_.c";
 ocamlrunparam += ",Xcache_stacks_per_class=0";
 frame_pointers;
 {
   arch_amd64;
   native;
 }{
   arch_arm64;
   native;
 }
*)

(* Restricted to amd64 and arm64, whose frame record is two words with the
   saved frame pointer first, as [clone_.c] walks it. RISC-V saves a CFA
   (the runtime handles it, the walker here does not) and Power has a back
   chain. *)

(* The copy made by [caml_continuation_clone] has its saved frame pointers
   relocated into its own stacks: after the original has run to completion
   and its stacks have been freed (the stack cache is off), every frame
   record of the copy links within the copy. Control: the same copy
   without the relocation links into the freed stacks. *)

open Effect
open Effect.Deep

external check_frames : ('a, 'b) continuation -> int * int
  = "clone_check_frames"
external clone : ('a, 'b) continuation -> ('a, 'b) continuation
  = "clone_stub"
external clone_without_frame_pointers :
  ('a, 'b) continuation -> ('a, 'b) continuation
  = "clone_without_frame_pointers"

type _ t += E : int t | F : unit t | G : unit t

(* Three fibers of about 100 frames each: [E] crosses handlers of [G] and
   [F], which forward it. *)
let gen3 () : (int, int) continuation =
  let r : (int, int) continuation option ref = ref None in
  let rec deep n f = if n = 0 then f () else 1 + deep (n - 1) f in
  ignore (match
    deep 100 (fun () ->
      try
        deep 100 (fun () ->
          try deep 100 (fun () -> perform E)
          with effect F, k -> continue k () | effect G, k -> continue k ())
      with effect F, k -> continue k () | effect G, k -> continue k ())
  with
  | x -> x
  | effect E, k -> r := Some k; 0);
  Option.get !r

let () =
  let k = gen3 () in
  let k' = clone k in
  Printf.printf "original: %d\n" (continue k 1);
  Gc.full_major ();
  let links, bad = check_frames k' in
  Printf.printf "clone: %b links followed, %d leave the copy\n"
    (links >= 300) bad;
  Printf.printf "clone resumed: %d\n" (continue k' 2);
  let k = gen3 () in
  let kc = clone_without_frame_pointers k in
  ignore (continue k 1);
  Gc.full_major ();
  let links, bad = check_frames kc in
  Printf.printf "control: %d links followed, %b leave the copy\n" links (bad > 0);
  (* A clone of a clone, made before either copy resumes; the original and
     the first copy then run to completion. With the stack cache on, the
     freed stacks can be reused by later copies. *)
  List.iter (fun cache ->
    Gc.Tweak.set "cache_stacks_per_class" cache;
    let k = gen3 () in
    let k1 = clone k in
    let k2 = clone k1 in
    ignore (continue k 1);
    ignore (continue k1 2);
    Gc.full_major ();
    let k3 = gen3 () in
    let k4 = clone k3 in
    let links, bad = check_frames k2 in
    Printf.printf "clone of a clone (cache %d): %b links followed, %d leave\n"
      cache (links >= 300) bad;
    Printf.printf "  resumed: %d %d %d\n" (continue k2 3) (continue k4 4)
      (continue k3 5))
    [0; 128]
