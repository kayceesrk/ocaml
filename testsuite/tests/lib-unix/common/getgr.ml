(* TEST
 include unix;
 hasunix;
 not target-windows;
 { bytecode; }{ native; }
*)

(* A missing group raises Not_found, including on hosts whose NSS module
   reports it with an error code such as ENOENT (#15378). *)
let () =
  match Unix.getgrnam "ocaml-test-no-such-group" with
  | exception Not_found -> ()
  | _ -> assert false

(* An existing group is still found, by gid and by name. *)
let () =
  match Unix.getgrgid (Unix.getgid ()) with
  | exception Not_found -> ()
  | g -> assert ((Unix.getgrnam g.Unix.gr_name).Unix.gr_gid = g.Unix.gr_gid)
