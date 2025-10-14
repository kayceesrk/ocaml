open Effect
open Effect.Deep

(* Effect used to capture a continuation while holding a resource. *)
type _ Effect.t += Hold_fd : int -> unit Effect.t

(* Existential wrapper to store continuations of any input type. *)
type packed_k = Pack : ('a, unit) continuation * int -> packed_k
let captured : packed_k list ref = ref []
let cleanup_triggered = ref false
let leaked_fds : int list ref = ref []

let leak_resource x =
  let fd = x in  (* Dummy file descriptor, just an integer *)
  match perform (Hold_fd fd) with
  | () -> ()
  | effect Hold_fd fd', k ->
      Printf.printf "[example] captured continuation protecting FD %d\n%!" fd';
      captured := Pack (k, fd') :: !captured
  | effect eff, k ->
      (* Unknown effect fallback: continue with a dummy value. *)
      continue k (Obj.magic ())
  | exception exn ->
      Printf.printf "[example] GC discontinued continuation with exception: %s\n%!" 
        (Printexc.to_string exn);
      cleanup_triggered := true;
      leaked_fds := fd :: !leaked_fds

let () =
  Printf.printf "[example] setting up leaked continuation\n%!";
  leak_resource 12;
  leak_resource 13;
  Printf.printf "[example] captured count: %d\n%!" (List.length !captured);

  Printf.printf "[example] dropping program references to the continuations\n%!";
  captured := [];

  Printf.printf "[example] forcing first major GCs to trigger discontinuation\n%!";
  Gc.full_major ();
  Printf.printf "[example] forcing second major GCs to trigger discontinuation\n%!";
  Gc.full_major ();

  (* Force pending actions to complete by triggering a minor GC and doing allocations *)
  Gc.minor ();
  let _ = ref 0 in (* small allocation *)
  let _ = ref 0 in
  
  Printf.printf "[example] cleanup triggered? %b\n%!" !cleanup_triggered;
  
  (* Now perform the deferred cleanup in a safe context *)
  if !cleanup_triggered then begin
    Printf.printf "[example] deferred cleanup: reclaimed %d leaked FDs: [%s]\n%!" 
      (List.length !leaked_fds)
      (String.concat ", " (List.map string_of_int !leaked_fds))
  end;
  
  Printf.printf "[example] done\n%!"

