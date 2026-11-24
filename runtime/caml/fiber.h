/**************************************************************************/
/*                                                                        */
/*                                 OCaml                                  */
/*                                                                        */
/*      KC Sivaramakrishnan, Indian Institute of Technology, Madras       */
/*                   Tom Kelly, OCaml Labs Consultancy                    */
/*                Stephen Dolan, University of Cambridge                  */
/*                                                                        */
/*   Copyright 2021 Indian Institute of Technology, Madras                */
/*   Copyright 2021 OCaml Labs Consultancy                                */
/*   Copyright 2019 University of Cambridge                               */
/*                                                                        */
/*   All rights reserved.  This file is distributed under the terms of    */
/*   the GNU Lesser General Public License version 2.1, with the          */
/*   special exception on linking described in the file LICENSE.          */
/*                                                                        */
/**************************************************************************/

#ifndef CAML_FIBER_H
#define CAML_FIBER_H

#ifdef CAML_INTERNALS

#include "misc.h"
#include "mlvalues.h"
#include "roots.h"

struct stack_info;

/* stack_handler describes the state for using fibers as part of effects */
struct stack_handler {
  value handle_value;
  value handle_exn;
  value handle_effect;
  struct stack_info* parent; /* parent OCaml stack if any */
};

/* stack_info describes the OCaml stack. It is used for:
 *  - storing information about the OCaml stack allowing it to be switched
 *  - accessing the stack_handler for the stack to handle effects
 *  - handling a freelist of OCaml stacks in a stack_cache
 */
struct stack_info {
#ifdef NATIVE_CODE
  void* sp;            /* stack pointer of the OCaml stack when suspended */
  void* exception_ptr; /* exception pointer of OCaml stack when suspended */
#else
  value* sp;
  value* exception_ptr;
#endif
  struct stack_handler* handler; /* effect handling state for the fiber */

  /* [cache_bucket] is an index into the [Caml_state->stack_cache] array if
   * this size is pooled. If unpooled, it is [-1].
   *
   * Stacks may be unpooled if either the stack size is not 2**N multiple of
   * [caml_fiber_wsz] or the stack is bigger than pooled sizes. */
  int16_t cache_bucket;
  /* Whether this stack has been counted towards the pace of the major GC
     (see [caml_continuation_charge_stacks]). Packed with [cache_bucket] so
     that the structure keeps its size. */
  uint16_t major_charged;
  size_t size; /* only used when USE_MMAP_MAP_STACK is defined */
  uintnat magic;
  int64_t id;
};

#define Stack_base(stk) ((value*)(stk + 1))
#define Stack_threshold_ptr(stk) \
  (Stack_base(stk) + Stack_threshold / sizeof(value))
#define Stack_high(stk) (value*)stk->handler

#define Stack_handle_value(stk) (stk)->handler->handle_value
#define Stack_handle_exception(stk) (stk)->handler->handle_exn
#define Stack_handle_effect(stk) (stk)->handler->handle_effect
#define Stack_parent(stk) (stk)->handler->parent

/* Stack layout for native code. Stack grows downwards.
 *
 * +------------------------+
 * |  struct stack_handler  |
 * +------------------------+ <--- Stack_high
 * |    caml_runstack /     |
 * |   caml_start_program   |
 * +------------------------+
 * |                        |
 * .      OCaml frames      . <--- sp
 * |                        |
 * +------------------------+ <--- Stack_threshold
 * |                        |
 * .        Red Zone        .
 * |                        |
 * +------------------------+ <--- Stack_base
 * |   struct stack_info    |
 * +------------------------+ <--- Caml_state->current_stack
 */

/* Some ABI reserve space at the bottom of every C stack frame. */

#if defined(TARGET_amd64) && (defined(_WIN32) || defined(__CYGWIN__))
/* Win64 ABI shadow store for argument registers */
  #define Reserved_space_c_stack_link 4 * 8
#elif defined(TARGET_s390x)
  #define Reserved_space_c_stack_link 160
#elif defined(TARGET_power)
/* ELF ABI: 4 reserved words at bottom of C stack */
  #define Reserved_space_c_stack_link 4 * 8
#else
  #define Reserved_space_c_stack_link 0
#endif

/* This structure is used for storing the OCaml return pointer when
 * transitioning from an OCaml stack to a C stack at a C call. When an OCaml
 * stack is reallocated, this linked list is walked to update the OCaml stack
 * pointers. It is also used for DWARF backtraces. */
struct c_stack_link {
#if Reserved_space_c_stack_link > 0
  char reserved[Reserved_space_c_stack_link];
#endif
  /* The reference to the OCaml stack */
  struct stack_info* stack;
  /* OCaml return address */
  void* sp;
  struct c_stack_link* prev;
};

/* `gc_regs` and `gc_regs_buckets`.

   When entering certain runtime functions, the OCaml runtime saves
   all registers into a `gc_regs` "bucket", a value array allocated on
   the C heap. This is notably used by the garbage collector to know
   which registers contain local roots.

   `Caml_state->gc_regs` points to the bucket currently in use, or
   NULL if no runtime function saving all registers is currently being
   called.

   `Caml_state->gc_regs_buckets` is a domain-local cache of buckets
   that are not currently in use. It has a linked list structure
   (the first element of each bucket is a pointer to the next
   available bucket or 0). It is guaranteed to be non-empty, to
   contain at least one free bucket, whenever we are running OCaml
   code on the domain. This invariant is maintained by calling
   [caml_maybe_expand_stack] before calling OCaml code from C code,
   which allocates a new bucket if the list is empty.

   When OCaml code needs to save all registers, it pops the next
   bucket from `gc_regs_bucket`. It is pushed back on return.

   When C code passes control to an OCaml callback, the current
   `Caml_state->gc_regs` value is saved to the top of the OCaml stack
   (see the `caml_start_program` logic, which is also used by
   `caml_callback` functions). In general we can thus have several
   buckets storing registers, one for each nested call to runtime
   functions saving all registers, with the currently-active one in
   `Caml_state` and the rest at the beginning of each OCaml stack
   fragment created from C.
*/

/* Overview of the stack switching primitives for effects
 *
 * For an understanding of effect handlers in OCaml please see:
 *  Retrofitting Effect Handlers onto OCaml, KC Sivaramakrishnan, et al.
 *  PLDI 2021
 *
 * A continuation object represents a suspended computation as a chain of fibers
 * (stack segments). It contains a pointer to the tail of this fiber chain
 * (cont_tail), whose parent pointer cycles back to the fiber at which the
 * effect was performed (cont_head).
 *
 * Stack pointers are tagged as integers to avoid being followed by the GC. In
 * the code, the tagged pointer can be referred to as a 'fiber':
 *     fiber := Val_ptr(stack)
 *
 *  Native code
 *  -----------
 *
 * In native compilation the stack switching primitives Prunstack,
 * Pperform, Preperform and Presume make use of corresponding functions
 * implemented in the assembly files for an architecture (such as
 * runtime/amd64.S).
 *
 * caml_runstack new_stack function argument
 *  caml_runstack launches a function (with an argument) in a new OCaml
 *  stack. It switches execution from the parent OCaml stack to the fresh
 *  stack and installs an exception handler. On return the new OCaml stack
 *  is freed, the stack is restored to the parent OCaml stack and the
 *  handle_value/handle_exn function is executed on the parent OCaml stack.
 *
 * caml_perform effect continuation
 *  caml_perform captures the current OCaml stack in the continuation object
 *  provided and raises the effect by switching to the parent OCaml stack and
 *  then executing the handle_effect function. Should there be no parent OCaml
 *  stack then the Effect.Unhandled exception is raised.
 *
 * caml_reperform effect continuation
 *  caml_reperform is used to walk up the parent OCaml stacks to execute the
 *  next effect handler installed in the chain. This function is implemented
 *  by setting up the required registers then jumping into caml_perform which
 *  does the switch to the parent and execution of the handle_effect function.
 *
 * caml_resume new_fiber function argument
 *  caml_resume resumes execution on new_fiber by making the current stack
 *  the parent of the new_fiber and then switching to the stack for new_fiber.
 *  The function with argument is then executed on the new stack. Care is taken
 *  to check if the new_fiber argument has already been resumed and so is null.
 *
 *
 *  Bytecode
 *  --------
 *
 * In bytecode compilation the primitives are mapped to effect instructions and
 * some changes are made to the bytecode interpreter on every function return
 * and exception raise. In particular:
 *
 *  Presume | Prunstack -> RESUME (& RESUMETERM if a tail call)
 *   RESUME checks that the stack is valid (a NULL stack indicates a
 *   continuation that has already been resumed). The stacks are then switched
 *   with the old stack becoming the parent of the new stack. Care is taken
 *   to setup the exception handler for the new stack. Execution continues
 *   on the new OCaml stack with the passed function and argument.
 *
 *  Pperform -> PERFORM
 *   PERFORM captures the current stack in a continuation object it allocates.
 *   The parent stack is then switched to and the handle_effect function for
 *   the parent stack is executed. If no parent stack exists then the
 *   Effect.Unhandled exception is raised.
 *
 *  Preperform -> REPERFORMTERM
 *   REPERFORMTERM is used to walk up the parent OCaml stacks to execute the
 *   next effect handler installed in the chain. The instruction takes care to
 *   switch back to the continuation stack to raise the Effect.Unhandled
 *   exception in in the case no parent is left. Otherwise the instruction
 *   switches to the parent stack and executes the handle_effect function for
 *   that parent stack.
 *
 *  Special return handling:
 *   There is special handling on every function return (see do_return of
 *   interp.c). This handling allows the completion of a child stack to be
 *   detected. On completion of a child stack, the child stack is freed and
 *   control returns to the parent stack to execute the handle_value function.
 *
 *  Special exception handling:
 *   When an exception is raised (see raise_notrace of interp.c), the trap
 *   offset is checked. If there are no more exceptions in this stack and a
 *   parent stack exists, then the child stack is freed and the
 *   handle_exception function is executed on the parent stack.
 *
 *  Layout of continuation objects:
 *  ------------------------------
 *
 *  A continuation object captures a suspended computation as a chain of fibers
 *  (stack segments). The continuation maintains pointers to the tail of this
 *  chain, with fibers linked via their Stack_parent pointers:
 *
 *  Continuation:                    Fibers:
 *  +-----------+                     +------------------+
 *  |   --------+----+                |    cont_head     |<------+
 *  +-----------+    |                +------------------+       |
 *                   |                         |                 |
 *                   |                  | Stack_parent |         |
 *                   |                         |                 |
 *                   |                         v                 |
 *                   |                +------------------+       |
 *                   |                |      Fiber       |       |
 *                   |                +------------------+       |
 *                   |                         |                 |
 *                   |                  | Stack_parent |         |
 *                   |                         |                 |
 *                   |                         v                 |
 *                   +--------------> +------------------+       |
 *                                    |    cont_tail     |       |
 *                                    +------------------+       |
 *                                             |                 |
 *                                      | Stack_parent |         |
 *                                             |                 |
 *                                             +-----------------+
 *
 * [cont_head] is the fiber that performed the effect. The handler at
 * [cont_tail] handled the effect. When the continuation is resumed, the current
 * stack becomes the parent of [cont_tail] and execution continues from
 * [cont_head].
 */

/* The table of global identifiers. Val_unit initially and replaced with a block
   during bytecode startup. */
extern value caml_global_data;

#define Trap_pc(tp) (((code_t *)(tp))[0])
#define Trap_link(tp) ((tp)[1])

struct stack_info** caml_alloc_stack_cache (void);
CAMLextern struct stack_info* caml_alloc_main_stack (uintnat init_wsize);

void caml_scan_stack(
  scanning_action f, scanning_action_flags fflags, void* fdata,
  struct stack_info* stack, value* v_gc_regs);

struct stack_info* caml_alloc_stack_noexc(mlsize_t wosize, value hval,
                                          value hexn, value heff, int64_t id);
/* try to grow the stack until at least required_size words are available.
   returns nonzero on success */
CAMLextern int caml_try_realloc_stack (asize_t required_wsize);
CAMLextern uintnat caml_get_init_stack_wsize(void);
void caml_change_max_stack_size (uintnat new_max_wsize);
void caml_maybe_expand_stack(void);
CAMLextern void caml_free_stack(struct stack_info* stk);
CAMLextern void caml_free_stack_cache(struct stack_info** cache);

/* gc_regs_buckets is allocated on-demand by [maybe_expand_stack]. */
CAMLextern void caml_free_gc_regs_buckets(value *gc_regs_buckets);

#ifdef NATIVE_CODE
void caml_get_stack_sp_pc (struct stack_info* stack,
                           char** sp /* out */, uintnat* pc /* out */);
void
caml_rewrite_exception_stack(struct stack_info *old_stack,
                             value** exn_ptr, struct stack_info *new_stack);
#endif

/* Take the stack of a continuation, to resume it. For the runtime's own
   callers. A taken continuation counts as resumed: it cannot be put back.
   To read a continuation's stack without resuming it, borrow it
   ([caml_continuation_borrow], below); to copy it, use
   [caml_continuation_clone]. */
value caml_continuation_use (value cont);

/* Borrowing. [caml_get_continuation_callstack] reads the stack of a
   continuation without taking it: [caml_continuation_borrow] swaps the
   field to [Val_cont_borrowed], and [caml_continuation_unborrow] puts the
   stack back. A borrow neither allocates nor polls, so no GC can see
   [Val_cont_borrowed]. [caml_continuation_use_noexc] waits until the
   borrow ends, so that a concurrent [continue] does not fail.
   [caml_continuation_borrow] raises [Continuation_already_resumed] if the
   continuation has been taken. */
#define Val_cont_borrowed ((value) 3)
struct stack_info* caml_continuation_borrow(value cont);
void caml_continuation_unborrow(value cont, struct stack_info* stk);

CAMLnoret CAMLextern void caml_raise_continuation_already_resumed (void);

CAMLnoret CAMLextern void caml_raise_unhandled_effect (value effect);

value caml_make_unhandled_effect_exn (value effect);

CAMLextern uintnat caml_live_stacks_memory(void);
CAMLextern uintnat caml_current_stack_size(void);

/* Reclaiming the stacks of unreachable continuations.

   A continuation has one field, which holds [Val_ptr(cont_tail)], or
   [Val_ptr(NULL)] once the continuation has been taken.

   [perform] records each continuation that it allocates in the domain's
   table [Caml_state->young_conts_*]. If the top entry has been taken
   ([Val_ptr(NULL)]; a borrowed continuation, see below, has not), the new
   continuation replaces it; otherwise it is pushed. The word below the
   table's base holds a sentinel entry that is never taken, so this needs
   no bounds check. The table is allocated with the domain, with 256
   entries plus a small reserve (2.5 KB on 64-bit), so that no effect
   operation has to check for it. When [perform] reaches [limit], the
   end of the main part, it interrupts the domain (requesting no GC work)
   and continues in the reserve. The next allocation polls before running
   any OCaml code (see [caml_alloc_small_dispatch]), and the poll compacts
   the table ([caml_compact_young_conts]): it keeps the entries that have
   not been taken and doubles the table if they fill more than a quarter
   of it. Every [perform] allocates its continuation first, so the
   reserve does not fill in practice; if it does, [perform] does not
   record the continuation. Any other reset of [young_limit] re-arms the
   interrupt while the table is in its reserve ([caml_reset_young_limit]),
   and if the table cannot grow out of its reserve, the poll requests a
   minor GC, which empties it. The table's size thus follows the number of continuations
   suspended at once, not the number of effects per minor cycle, and
   filling it requests no GC work (except a minor GC if the table cannot
   grow out of its reserve; and its poll can start GC work that was within
   one allocation of its trigger). The minor GC empties it.

   A borrowed continuation (see [caml_continuation_borrow]) is not taken,
   so it stays recorded. Borrowing is the way to get temporary access to a
   continuation's stack. [caml_continuation_clone] records the copy it
   makes. The free claims a continuation with a CAS, so its stacks are
   freed at most once.

   A continuation that was never taken and is found to be unreachable,
   either in the table after a minor collection or by the sweeper, has its
   stack chain freed. No OCaml code runs. Only [perform] leaves a
   continuation untaken: the continuations made by [callback.c] are always
   taken by [restore_stack_parent], and [perform] takes its continuation
   itself when the effect is unhandled. */

/* Pacing. Fiber stack memory is counted towards the pace of the GCs as
   the out-of-heap memory of custom blocks is:
   - minor GC: a stack newly obtained from the system (not from the stack
     cache), when a fiber starts ([caml_alloc_stack]), grows
     ([caml_try_realloc_stack]) or is cloned;
   - major GC: when a continuation that has not been taken is promoted, the
     stacks of its chain, each once in its lifetime: a stack is marked when
     counted; the mark is cleared when a stack is allocated, fresh or from
     the cache (not when it is freed); a grown stack keeps it and counts
     only its growth.
   [caml_continuation_charge_stacks] returns the bytes of the not yet
   counted stacks of the chain of a continuation that has not been taken,
   and marks them counted. */
uintnat caml_continuation_charge_stacks(struct stack_info* cont_tail);

/* Free the stack chain of a continuation that has not been taken and is
   unreachable. */
void caml_free_continuation_stacks(struct stack_info* cont_tail);

/* Called on a GARBAGE [Cont_tag] block of the major heap. */
void caml_finalise_dead_continuation(value cont);

/* Called by the minor GC after its end barrier: free the stacks of this
   domain's dead young continuations and empty the table. */
void caml_free_dead_young_continuations(caml_domain_state* domain);

/* Allocate the table of young continuations of a new domain (returns 0 on
   failure), and free the (empty) table of a terminating one (no effect if
   already freed). */
int caml_init_young_conts(caml_domain_state* domain);
void caml_free_young_conts(caml_domain_state* domain);

/* Drop the taken entries of this domain's table of young continuations,
   keeping the others in order, and double the table if it is still more
   than a quarter full; if it is still in its reserve, request a minor GC.
   Called when [caml_perform] reached the threshold (at the
   next poll, [caml_poll_gc_work]), and from C where the runtime records a
   continuation. Allocates nothing on the OCaml heap. */
void caml_compact_young_conts(caml_domain_state* domain);

/* Test hook, for the testsuite only (internal, not an API): the current
   capacity of the table (0 if not allocated). */
CAMLextern uintnat caml_young_conts_size(caml_domain_state* domain);


#endif /* CAML_INTERNALS */

#include "mlvalues.h"

#ifdef __cplusplus
extern "C" {
#endif

/* [caml_continuation_clone(k)] returns a new continuation that resumes a
   copy of the computation suspended in [k], without resuming [k]: both can
   then be resumed, once each. This is for libraries that implement
   multi-shot continuations; [Effect] itself stays one-shot. Raises
   [Effect.Continuation_already_resumed] if [k] has been resumed, and
   otherwise [Out_of_memory] if the copy cannot be allocated. Both continuations are
   reclaimed like any other when they become unreachable. While the stacks
   are copied, a [continue] of [k] on another domain waits, and so does a
   stop-the-world section: cloning a large continuation delays both. In
   frame-pointer builds, the copy's chain of frame records (on Power, its
   back chain) is relocated into the copy; on Power, the copies of r31
   that generated code spills in its frames are not. */
CAMLextern value caml_continuation_clone(value k);

#ifdef __cplusplus
}
#endif

#endif /* CAML_FIBER_H */
