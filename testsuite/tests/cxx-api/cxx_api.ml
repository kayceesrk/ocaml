(* TEST
 has-cxx;
 hassysthreads;
 readonly_files = "stubs.cpp cxx.sh";
 include runtime_events;
 include systhreads;
 {
   setup-ocamlopt.byte-build-env;
   script = "${cxx} ${cppflags} ${cflags} \
     -I ${ocamlsrcdir}/runtime \
     -I ${ocamlsrcdir}/otherlibs/systhreads \
     -I ${ocamlsrcdir}/otherlibs/unix \
     ${outputobj}${test_build_directory}/stubs.${objext} \
     -c ${test_source_directory}/stubs.cpp";
   script;
   all_modules = "stubs.${objext} cxx_api.ml";
   ocamlopt.byte;
   output = "${test_build_directory}/program-output";
   stdout = "${output}";
   run;
   check-program-output;
 }
 {
   setup-ocamlc.byte-build-env;
   script = "${cxx} ${cppflags} ${cflags} \
     -I ${ocamlsrcdir}/runtime \
     -I ${ocamlsrcdir}/otherlibs/systhreads \
     -I ${ocamlsrcdir}/otherlibs/unix \
     ${outputobj}${test_build_directory}/stubs.${objext} \
     -c ${test_source_directory}/stubs.cpp";
   script;
   all_modules = "stubs.${objext} cxx_api.ml";
   flags = "-output-complete-exe  -cclib -lunixbyt -cclib -lthreads -cclib -lcamlruntime_eventsbyt";
   ocamlc.byte;
   output = "${test_build_directory}/program-output";
   stdout = "${output}";
   run;
   check-program-output;
 }
*)

external test_cxx : unit -> string = "test_cxx"

let () = print_string (test_cxx ())

(* [caml_continuation_clone] called from C++. *)
external clone :
  ('a, 'b) Effect.Deep.continuation -> ('a, 'b) Effect.Deep.continuation
  = "test_cxx_clone"

type _ Effect.t += E : int Effect.t

let () =
  let open Effect.Deep in
  let r : (int, int) continuation option ref = ref None in
  ignore (match Effect.perform E + 1 with
    | x -> x
    | effect E, k -> r := Some k; 0);
  let k = Option.get !r in
  let k' = clone k in
  Printf.printf "clone from C++: %d %d\n" (continue k 1) (continue k' 2)
