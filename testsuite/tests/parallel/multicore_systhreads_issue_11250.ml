(* TEST
* hassysthreads
include systhreads
** bytecode
** native
*)

let () =
  let max_iterations = 8192 in
  for _i=0 to max_iterations do
    Printf.printf ".%!";
    let dom1 = Domain.spawn (fun () -> ()) in
    let _dom2 = Domain.spawn (fun () -> Thread.delay 0.005) in (* domain not joined *)
    let dom3 = Domain.spawn (fun () -> ()) in
    Domain.join dom1;
    Domain.join dom3;
  done
