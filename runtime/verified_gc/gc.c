#include <assert.h>
#include "internal/FStar.h"
#include "internal/Spec.h"
#include "gc.h"

#define CAML_INTERNALS
#include "../caml/mlvalues.h"
#include "../caml/roots.h"
#include "../caml/misc.h"


/* uint64_t roots[MAXIMUM_NO_OF_ROOTS] = {}; */
/* uint64_t num_of_roots = 0; */
uint64_t *stack = NULL;
uint64_t *stack_top = NULL;

//For debugging
uint64_t hs = 0; //Heap Start
uint64_t he = 0; //Heap End

Prims_int Impl_GC_closure_infix_op_Bang(uint64_t x) {
  return FStar_UInt64_v(x);
}

uint64_t Impl_GC_closure_infix_getColor(uint64_t h) {
  uint64_t c_ = h >> (uint32_t)8U;
  return c_ & (uint64_t)3U;
}

uint64_t Impl_GC_closure_infix_makeHeader(uint64_t wz, uint64_t c,
                                          uint64_t tg) {
  return Spec_GC_closure_infix_makeHeader(wz, c, tg);
}

uint64_t Impl_GC_closure_infix_read_word_from_byte_buffer(uint8_t *g,
                                                          uint64_t byte_index) {
  uint32_t x1 = FStar_UInt32_uint_to_t(FStar_UInt64_v(byte_index));
  return load64_le(g + x1);
}

void Impl_GC_closure_infix_write_word_to_byte_buffer(uint8_t *g,
                                                     uint64_t byte_index,
                                                     uint64_t v) {
  uint32_t x1 = FStar_UInt32_uint_to_t(FStar_UInt64_v(byte_index));
  store64_le(g + x1, v);
}

bool Impl_GC_closure_infix_isPointer(uint64_t v_id, uint8_t *g) {
  uint32_t x1 = FStar_UInt32_uint_to_t(FStar_UInt64_v(v_id));
  return (load64_le(g + x1) & (uint64_t)1U) == (uint64_t)0U;
}

uint64_t Impl_GC_closure_infix_wosize_of_block(uint64_t v_id, uint8_t *g) {
  uint32_t x1 = FStar_UInt32_uint_to_t(FStar_UInt64_v(v_id));
  uint64_t index = load64_le(g + x1);
  uint64_t wz = Spec_GC_closure_infix_getWosize(index);
  return wz;
}

uint64_t Impl_GC_closure_infix_color_of_block(uint64_t v_id, uint8_t *g) {
  uint32_t x1 = FStar_UInt32_uint_to_t(FStar_UInt64_v(v_id));
  uint64_t index = load64_le(g + x1);
  uint64_t cl = Spec_GC_closure_infix_getColor(index);
  return cl;
}

uint64_t Impl_GC_closure_infix_tag_of_block(uint64_t v_id, uint8_t *g) {
  uint32_t x1 = FStar_UInt32_uint_to_t(FStar_UInt64_v(v_id));
  uint64_t index = load64_le(g + x1);
  uint64_t tg = Spec_GC_closure_infix_getTag(index);
  return tg;
}

void Impl_GC_closure_infix_colorHeader1(uint8_t *g, uint64_t v_id, uint64_t c) {
  uint64_t wz = Impl_GC_closure_infix_wosize_of_block(v_id, g);
  uint64_t tg = Impl_GC_closure_infix_tag_of_block(v_id, g);
  uint64_t h_val = Spec_GC_closure_infix_makeHeader(wz, c, tg);
  uint32_t x1 = FStar_UInt32_uint_to_t(FStar_UInt64_v(v_id));
  store64_le(g + x1, h_val);
}

void Impl_GC_closure_infix_push_to_stack(uint8_t *g, uint64_t *st,
                                         uint64_t *st_len, uint64_t elem) {
  uint64_t i = *st_len;
  uint64_t f_elem = Spec_GC_closure_infix_f_address(elem);

  assert (f_elem >= hs && f_elem <= he // the object must be in the heap
          || Wosize_val(f_elem) == 0); // or it must be a block with size 0

  st[FStar_UInt32_uint_to_t(FStar_UInt64_v(i))] = f_elem;
  st_len[0U] = *st_len + (uint64_t)1U;
  uint64_t i1 = *st_len;
  Impl_GC_closure_infix_colorHeader1(g, elem, Spec_GC_closure_infix_gray);
}

uint64_t Impl_GC_closure_infix_read_succ_impl(uint8_t *g, uint64_t h_index,
                                              uint64_t i) {
  uint64_t succ_index = h_index + i * (uint64_t)8U;
  uint32_t x1 = FStar_UInt32_uint_to_t(FStar_UInt64_v(succ_index));
  uint64_t succ = load64_le(g + x1);
  return succ;
}

/* Adding comments after extraction for making reading easy
   succ_index = h_index + i * mword
   succ = read g succ_index <---- Infix object first field
   hdr_succ = hd_address succ <--- hdr address of infix object
   original_closure_parent = succ - wosize(hdr_succ) - mword <--- locate the
   first field of the parent closure object hdr_parent = hd_address
   original_closure_parent <--- The header address of original parent is
   returned to check the color of the object is white or not*/
uint64_t Impl_GC_closure_infix_parent_closure_of_infix_object_impl(
    uint8_t *g, uint64_t h_index, uint64_t i) {
  uint64_t succ = Impl_GC_closure_infix_read_succ_impl(g, h_index, i);
  uint64_t h_addr_succ = Spec_GC_closure_infix_hd_address(succ);
  uint32_t x1 = FStar_UInt32_uint_to_t(FStar_UInt64_v(h_addr_succ));
  uint64_t h_addr_succ_val = load64_le(g + x1);
  uint64_t wosize = Spec_GC_closure_infix_getWosize(h_addr_succ_val);
  uint64_t parent_succ = succ - wosize * (uint64_t)8U;
  uint64_t h_addr_parent = Spec_GC_closure_infix_hd_address(parent_succ);
  return h_addr_parent;
}

void Impl_GC_closure_infix_darken_helper_impl(uint8_t *g, uint64_t *st,
                                              uint64_t *st_len,
                                              uint64_t hdr_id) {
  if (Impl_GC_closure_infix_color_of_block(hdr_id, g) ==
      Spec_GC_closure_infix_white)
    Impl_GC_closure_infix_push_to_stack(g, st, st_len, hdr_id);
}

void Impl_GC_closure_infix_darken_body(uint8_t *g, uint64_t *st,
                                       uint64_t *st_len, uint64_t h_index,
                                       uint64_t wz, uint64_t i) {
  uint64_t succ_index = h_index + i * (uint64_t)8U;
  uint32_t x1 = FStar_UInt32_uint_to_t(FStar_UInt64_v(succ_index));
  uint64_t succ = load64_le(g + x1);
  if (Impl_GC_closure_infix_isPointer(succ_index, g)) {
    uint64_t h_addr_succ = Spec_GC_closure_infix_hd_address(succ);
    if (Impl_GC_closure_infix_tag_of_block(h_addr_succ, g) == (uint64_t)249U) {
      uint64_t parent_hdr =
          Impl_GC_closure_infix_parent_closure_of_infix_object_impl(g, h_index,
                                                                    i);
      Impl_GC_closure_infix_darken_helper_impl(g, st, st_len, parent_hdr);
    } else
      Impl_GC_closure_infix_darken_helper_impl(g, st, st_len, h_addr_succ);
  }
}

void Impl_GC_closure_infix_darken(uint8_t *g, uint64_t *st, uint64_t *st_len,
                                  uint64_t h_index, uint64_t wz) {
  for (uint32_t i = (uint32_t)1U;
       i < FStar_UInt32_uint_to_t(FStar_UInt64_v(wz + (uint64_t)1U)); i++)
    Impl_GC_closure_infix_darken_body(
        g, st, st_len, h_index, wz, FStar_UInt64_uint_to_t(FStar_UInt32_v(i)));
}

void Impl_GC_closure_infix_darken1(uint8_t *g, uint64_t *st, uint64_t *st_len,
                                   uint64_t h_index, uint64_t wz, uint64_t j) {
  for (uint32_t i = FStar_UInt32_uint_to_t(FStar_UInt64_v(j));
       i < FStar_UInt32_uint_to_t(FStar_UInt64_v(wz + (uint64_t)1U)); i++)
    Impl_GC_closure_infix_darken_body(
        g, st, st_len, h_index, wz, FStar_UInt64_uint_to_t(FStar_UInt32_v(i)));
}

/* Function used to extract the content of the closure info field from the
 * address of an object*/
uint64_t Impl_GC_closure_infix_closinfo_val_impl(uint8_t *g, uint64_t f_addr) {
  uint64_t hdr_f_addr = Spec_GC_closure_infix_hd_address(f_addr);
  uint64_t wz = Impl_GC_closure_infix_wosize_of_block(hdr_f_addr, g);
  uint64_t offst1 = (uint64_t)8U;
  uint64_t s1 = f_addr + offst1;
  uint32_t x1 = FStar_UInt32_uint_to_t(FStar_UInt64_v(s1));
  uint64_t clos_info = load64_le(g + x1);
  return clos_info;
}

uint64_t Impl_GC_closure_infix_start_env_clos_info(uint8_t *g,
                                                   uint64_t f_addr) {
  uint64_t clos_info = Impl_GC_closure_infix_closinfo_val_impl(g, f_addr);
  uint64_t start_env = Spec_GC_closure_infix_extract_start_env_bits_(clos_info);
  uint64_t hdr_f_addr = Spec_GC_closure_infix_hd_address(f_addr);
  uint64_t wz = Impl_GC_closure_infix_wosize_of_block(hdr_f_addr, g);
  return start_env;
}

void Impl_GC_closure_infix_darken_wrapper_impl(uint8_t *g, uint64_t *st,
                                               uint64_t *st_len, uint64_t h_x,
                                               uint64_t wz) {
  if (Impl_GC_closure_infix_tag_of_block(h_x, g) == (uint64_t)247U) {
    uint64_t x = Spec_GC_closure_infix_f_address(h_x);
    uint64_t start_env = Impl_GC_closure_infix_start_env_clos_info(g, x);
    Impl_GC_closure_infix_darken1(g, st, st_len, h_x, wz, start_env);
  } else
    Impl_GC_closure_infix_darken1(g, st, st_len, h_x, wz, (uint64_t)1U);
}

void Impl_GC_closure_infix_mark_heap_body1_impl(uint8_t *g, uint64_t *st,
                                                uint64_t *st_len) {
  st_len[0U] = *st_len - (uint64_t)1U;
  uint64_t x = st[FStar_UInt32_uint_to_t(FStar_UInt64_v(*st_len))];
  uint64_t h_x = Spec_GC_closure_infix_hd_address(x);
  Impl_GC_closure_infix_colorHeader1(g, h_x, Spec_GC_closure_infix_black);
  uint64_t wz = Impl_GC_closure_infix_wosize_of_block(h_x, g);
  uint64_t tg = Impl_GC_closure_infix_tag_of_block(h_x, g);
  if (tg < (uint64_t)251U)
    Impl_GC_closure_infix_darken_wrapper_impl(g, st, st_len, h_x, wz);
}

void Impl_GC_closure_infix_mark_heap(uint8_t *g, uint64_t *st,
                                     uint64_t *st_len) {
  while (*st_len > (uint64_t)0U)
    Impl_GC_closure_infix_mark_heap_body1_impl(g, st, st_len);
}

void Impl_GC7_create_root_stack_and_gray_modified_heap_loop(
    uint8_t *g, uint64_t *st, uint64_t *st_top, uint64_t *h_list,
    uint64_t h_list_length) {
  for (uint32_t i = (uint32_t)0U;
       i < FStar_UInt32_uint_to_t(FStar_UInt64_v(h_list_length)); i++) {
    uint64_t hd = h_list[i];
    Impl_GC_closure_infix_push_to_stack(g, st, st_top, hd);
  }
}

void Impl_GC7_sweep_body_helper(uint8_t *g, uint64_t *h_index) {
  uint64_t h_index_val = *h_index;
  uint64_t c = Impl_GC_closure_infix_color_of_block(h_index_val, g);
  if (c == Spec_GC_closure_infix_white)
    Impl_GC_closure_infix_colorHeader1(g, h_index_val,
                                       Spec_GC_closure_infix_blue);
  else if (c == Spec_GC_closure_infix_black)
    Impl_GC_closure_infix_colorHeader1(g, h_index_val,
                                       Spec_GC_closure_infix_white);
}

void Impl_GC7_sweep1(uint8_t *g, uint64_t *h_index, uint64_t limit) {
  while (*h_index < limit) {
    uint64_t h_index_val = *h_index;
    uint64_t wz = Impl_GC_closure_infix_wosize_of_block(h_index_val, g);
    uint64_t h_index_new = h_index_val + (wz + (uint64_t)1U) * (uint64_t)8U;
    Impl_GC7_sweep_body_helper(g, h_index);
    h_index[0U] = h_index_new;
  }
}

void Impl_GC7_mark_and_sweep_GC1_aux(uint8_t *g, uint64_t *st, uint64_t *st_top,
                                     uint64_t *h_list, uint64_t h_list_length,
                                     uint64_t *h_index, uint64_t limit) {
  //Impl_GC7_create_root_stack_and_gray_modified_heap_loop(g, st, st_top, h_list,
  //                                                       h_list_length);
  Impl_GC_closure_infix_mark_heap(g, st, st_top);
  Impl_GC7_sweep1(g, h_index, limit);
}

// Handwritten
void darken_root(value root, value *root_ptr) {
  if (Is_block(root) && Wosize_val(root) > 0) {
    Impl_GC_closure_infix_push_to_stack(NULL, stack, stack_top,
                                        (uint64_t)Hp_val(root));
  }
}

void Impl_GC7_mark_and_sweep_GC1(uint8_t *g, uint64_t *h_list,
                                 uint64_t h_list_length,
                                 uint64_t free_list_start_ptr,
                                 uint64_t free_list_end_ptr) {
  /* uint64_t *st = KRML_HOST_CALLOC((uint32_t)1024U, sizeof(uint64_t)); */
  // Allocating much more than 1024 since that's not practical and it's only
  // coming here due to modelling memory as an array of this size in proofs
  stack = KRML_HOST_CALLOC((uint32_t)(h_list_length * 10), sizeof(uint64_t));
  stack_top = KRML_HOST_CALLOC((uint32_t)1U, sizeof(uint64_t));
  uint64_t *h_index_buf = KRML_HOST_CALLOC((uint32_t)1U, sizeof(uint64_t));
  *h_index_buf = free_list_start_ptr;
  caml_do_roots(darken_root, 1);

  Impl_GC7_mark_and_sweep_GC1_aux(g, stack, stack_top, h_list, h_list_length,
                                  h_index_buf, free_list_end_ptr);

  KRML_HOST_FREE(stack);
  KRML_HOST_FREE(stack_top);
  KRML_HOST_FREE(h_index_buf);
}

void mark_and_sweep(uint64_t xheap_start, uint64_t heap_end) {
  hs = xheap_start;
  he = heap_end;
  uint64_t locally_maintained_roots[] = {};
  Impl_GC7_mark_and_sweep_GC1(NULL, locally_maintained_roots, 1024 * 1024,
                              xheap_start, heap_end);
}
