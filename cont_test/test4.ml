open Effect
open Effect.Deep

type _ Effect.t += Choose : bool Effect.t

let coin_flip () =
  try
    if perform Choose then "Heads" else "Tails"
  with exn ->
    Printf.eprintf "coin_flip raised: %s\n" (Printexc.to_string exn);
    flush stderr;
    raise exn

let nested_live_continuations () =
  let live_ks = ref ([] : (bool, unit) continuation list) in  (* list to keep continuations alive *)
  for i = 1 to 10 do
    match coin_flip () with
    | r -> Printf.printf "Result %d: %s\n" i r
    | effect Choose, k ->
        live_ks := k :: !live_ks;  (* keep continuation alive *)
        Printf.printf "Captured continuation %d\n" i
  done;
  (* All continuations are now in live_ks, so they remain live *)
  Printf.printf "All continuations captured and kept alive. Calling Gc.full_major()...\n";
  Gc.full_major ();
  List.iteri (fun j k ->
    let original_i = List.length !live_ks - j in
    let value_to_pass = (original_i mod 2 = 0) in
    if value_to_pass then
      begin
        Printf.printf "Resuming continuation (originally captured at i=%d) with value %b\n" original_i value_to_pass;
        continue k value_to_pass
      end
    else
      begin
        Printf.printf "Ignoring continuation (originally captured at i=%d) with value %b\n" original_i value_to_pass;
      end
  ) !live_ks;
  Gc.full_major ();
  Printf.printf "Full major GC completed. Continuations should be in the LL.\n"

let () =
  print_endline "Running nested live continuations example...";
  nested_live_continuations ();
  print_endline "Example done."