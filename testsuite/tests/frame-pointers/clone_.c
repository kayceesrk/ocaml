/* Stubs for clone.ml: walk the frame-pointer chains of a continuation. */
#define CAML_INTERNALS
#define NATIVE_CODE /* for caml_rewrite_exception_stack; native-only test */
#include <string.h>
#include "caml/alloc.h"
#include "caml/fail.h"
#include "caml/fiber.h"
#include "caml/memory.h"
#include "caml/mlvalues.h"

/* The frame record of amd64 and arm64. */
struct frame { struct frame* prev; void* retaddr; };

static int in_stack(struct stack_info* s, void* p)
{
  return Stack_base(s) <= (value*)p && (value*)p < Stack_high(s);
}

/* For each fiber of [k], follow its frame records from the saved sp while
   they stay in the fiber's stack. Returns (number of links followed,
   number of fibers other than the tail whose last record does not point
   into the parent fiber). Only addresses inside [k]'s own stacks are
   dereferenced. */
value clone_check_frames(value k)
{
  CAMLparam1(k);
  CAMLlocal1(res);
  long links = 0, bad = 0;
  struct stack_info* tail = caml_continuation_borrow(k);
  struct stack_info* s = Stack_parent(tail);
  while (1) {
    struct frame* fp = (struct frame*)s->sp;
    while (in_stack(s, fp->prev)) { fp = fp->prev; links++; }
    if (s == tail) break;
    if (in_stack(Stack_parent(s), fp->prev)) links++; else bad++;
    s = Stack_parent(s);
  }
  caml_continuation_unborrow(k, tail);
  res = caml_alloc_small(2, 0);
  Field(res, 0) = Val_long(links);
  Field(res, 1) = Val_long(bad);
  CAMLreturn(res);
}

/* Control: copy [k]'s chain as [caml_continuation_clone] does, but without
   relocating the frame pointers. */
value clone_without_frame_pointers(value k)
{
  CAMLparam1(k);
  CAMLlocal1(r);
  r = caml_alloc_1(Cont_tag, Val_ptr(NULL));
  struct stack_info* tail = caml_continuation_borrow(k);
  struct stack_info *s = Stack_parent(tail), *head = NULL, *last = NULL;
  while (1) {
    uintnat used = Stack_high(s) - (value*)s->sp;
    struct stack_info* c =
      caml_alloc_stack_noexc((Stack_high(s) - Stack_base(s)) & ~1,
                             Stack_handle_value(s), Stack_handle_exception(s),
                             Stack_handle_effect(s), s->id);
    if (c == NULL) caml_fatal_error("clone_without_frame_pointers");
    memcpy(Stack_high(c) - used, Stack_high(s) - used, used * sizeof(value));
    c->sp = Stack_high(c) - used;
    c->exception_ptr = s->exception_ptr;
    caml_rewrite_exception_stack(s, (value**)&c->exception_ptr, c);
    if (last == NULL) head = c; else Stack_parent(last) = c;
    last = c;
    if (s == tail) break;
    s = Stack_parent(s);
  }
  Stack_parent(last) = head;
  caml_continuation_unborrow(k, tail);
  Field(r, 0) = Val_ptr(last);
  CAMLreturn(r);
}

value clone_stub(value k)
{
  return caml_continuation_clone(k);
}
