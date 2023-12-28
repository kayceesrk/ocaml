#ifndef __internal_Spec_H
#define __internal_Spec_H

#include "../Spec.h"
#include "LowStar_Prims.h"
#include "alias.h"
typedef Prims_list__uint64_t *Spec_Graph3_vertex_list;

Prims_list__uint64_t *Spec_Graph3_successors(Spec_Graph3_graph_state g,
                                             uint64_t i);

bool Spec_Graph3_is_emptySet(Prims_list__uint64_t *s);

uint64_t Spec_Graph3_get_last_elem(Spec_Graph3_graph_state g,
                                   Prims_list__uint64_t *s);

Prims_list__uint64_t *Spec_Graph3_get_first(Spec_Graph3_graph_state g,
                                            Prims_list__uint64_t *s);

Prims_list__uint64_t *
Spec_Graph3_insert_to_vertex_set(Spec_Graph3_graph_state g, uint64_t x,
                                 Prims_list__uint64_t *s);

Prims_list__uint64_t *Spec_Graph3_union_vertex_sets(Spec_Graph3_graph_state g,
                                                    Prims_list__uint64_t *l1,
                                                    Prims_list__uint64_t *l2);

Prims_list__uint64_t *Spec_Graph3_successor_push_itr1(Prims_list__uint64_t *l,
                                                      Prims_list__uint64_t *vl,
                                                      Prims_list__uint64_t *st,
                                                      Prims_int j);

Prims_list__uint64_t *
Spec_Graph3_remove_all_instances_of_vertex_from_vertex_set(
    Prims_list__uint64_t *l, Prims_list__uint64_t *vl);

extern uint64_t Spec_GC_closure_infix_blue;

extern uint64_t Spec_GC_closure_infix_white;

extern uint64_t Spec_GC_closure_infix_gray;

extern uint64_t Spec_GC_closure_infix_black;

uint64_t Spec_GC_closure_infix_getWosize(uint64_t h);

uint64_t Spec_GC_closure_infix_getColor(uint64_t h);

uint64_t Spec_GC_closure_infix_getTag(uint64_t h);

uint64_t Spec_GC_closure_infix_makeHeader(uint64_t wz, uint64_t c, uint64_t tg);

uint64_t Spec_GC_closure_infix_extract_start_env_bits_(uint64_t clos_info);

uint64_t Spec_GC_closure_infix_hd_address(uint64_t st_index);

uint64_t Spec_GC_closure_infix_f_address(uint64_t h_index);

#define __internal_Spec_H_DEFINED
#endif
