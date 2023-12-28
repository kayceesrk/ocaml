#ifndef __Spec_H
#define __Spec_H

#include <stdint.h>
typedef struct Prims_list__uint64_t_s Prims_list__uint64_t;

#define Prims_Nil 0
#define Prims_Cons 1

typedef uint8_t Prims_list__uint64_t_tags;

typedef struct Prims_list__uint64_t_s {
  Prims_list__uint64_t_tags tag;
  uint64_t hd;
  Prims_list__uint64_t *tl;
} Prims_list__uint64_t;

typedef struct Spec_Graph3_edge_s {
  uint64_t fst;
  uint64_t snd;
} Spec_Graph3_edge;

typedef struct Prims_list__Spec_Graph3_edge_s Prims_list__Spec_Graph3_edge;

typedef struct Prims_list__Spec_Graph3_edge_s {
  Prims_list__uint64_t_tags tag;
  Spec_Graph3_edge hd;
  Prims_list__Spec_Graph3_edge *tl;
} Prims_list__Spec_Graph3_edge;

typedef struct Spec_Graph3_graph_state_s {
  Prims_list__uint64_t *vertices;
  Prims_list__Spec_Graph3_edge *edge_set;
} Spec_Graph3_graph_state;

#define __Spec_H_DEFINED
#endif
