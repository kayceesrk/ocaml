(* TEST *)

let _ =
  let key_array =
    Array.init 128 (fun i -> Domain.TLS.new_key (fun _ -> i))
  in
  assert (Domain.TLS.get (key_array.(42)) = 42);
  let d = Domain.spawn (fun _ ->
    assert (Domain.TLS.get (key_array.(63)) = 63))
  in
  Domain.join d;
  print_endline "OK"
