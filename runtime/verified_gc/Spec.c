#include "internal/Spec.h"
#include "internal/alias.h"

#include "internal/LowStar_Prims.h"

typedef Prims_list__Spec_Graph3_edge *edge_list;

static Prims_int
length__uint64_t___uint64_t(Prims_list__Spec_Graph3_edge *uu___) {
  if (uu___->tag == Prims_Nil)
    return (krml_checked_int_t)0;
  else if (uu___->tag == Prims_Cons) {
    Prims_list__Spec_Graph3_edge *tl1 = uu___->tl;
    return Prims_op_Addition((krml_checked_int_t)1,
                             length__uint64_t___uint64_t(tl1));
  } else {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n", __FILE__, __LINE__,
                      "unreachable (pattern matches are exhaustive in F*)");
    KRML_HOST_EXIT(255U);
  }
}

static Prims_int length__uint64_t___uint64_t0(Prims_list__Spec_Graph3_edge *s) {
  return length__uint64_t___uint64_t(s);
}

static Prims_list__uint64_t *empty__uint64_t() {
  KRML_CHECK_SIZE(sizeof(Prims_list__uint64_t), (uint32_t)1U);
  Prims_list__uint64_t *buf = KRML_HOST_MALLOC(sizeof(Prims_list__uint64_t));
  buf[0U] = ((Prims_list__uint64_t){.tag = Prims_Nil});
  return buf;
}

static uint64_t fst__uint64_t_uint64_t(Spec_Graph3_edge x) { return x.fst; }

static Spec_Graph3_edge
hd__uint64_t___uint64_t(Prims_list__Spec_Graph3_edge *uu___) {
  if (uu___->tag == Prims_Cons)
    return uu___->hd;
  else {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n", __FILE__, __LINE__,
                      "unreachable (pattern matches are exhaustive in F*)");
    KRML_HOST_EXIT(255U);
  }
}

static Prims_list__Spec_Graph3_edge *
tail__uint64_t___uint64_t(Prims_list__Spec_Graph3_edge *uu___) {
  if (uu___->tag == Prims_Cons)
    return uu___->tl;
  else {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n", __FILE__, __LINE__,
                      "unreachable (pattern matches are exhaustive in F*)");
    KRML_HOST_EXIT(255U);
  }
}

static Prims_list__Spec_Graph3_edge *(*tl__uint64_t___uint64_t)(
    Prims_list__Spec_Graph3_edge *x0) = tail__uint64_t___uint64_t;

static Spec_Graph3_edge
index__uint64_t___uint64_t(Prims_list__Spec_Graph3_edge *l, Prims_int i) {
  if (i == (krml_checked_int_t)0)
    return hd__uint64_t___uint64_t(l);
  else
    return index__uint64_t___uint64_t(
        tl__uint64_t___uint64_t(l),
        Prims_op_Subtraction(i, (krml_checked_int_t)1));
}

static Spec_Graph3_edge
index__uint64_t___uint64_t0(Prims_list__Spec_Graph3_edge *s, Prims_int i) {
  return index__uint64_t___uint64_t(s, i);
}

static Spec_Graph3_edge
head__uint64_t___uint64_t(Prims_list__Spec_Graph3_edge *s) {
  return index__uint64_t___uint64_t0(s, (krml_checked_int_t)0);
}

static uint64_t snd__uint64_t_uint64_t(Spec_Graph3_edge x) { return x.snd; }

static Prims_list__Spec_Graph3_edge *
tl__uint64_t___uint64_t0(Prims_list__Spec_Graph3_edge *s) {
  return tl__uint64_t___uint64_t(s);
}

static Prims_list__Spec_Graph3_edge *
cons__uint64_t___uint64_t(Spec_Graph3_edge x, Prims_list__Spec_Graph3_edge *s) {
  KRML_CHECK_SIZE(sizeof(Prims_list__Spec_Graph3_edge), (uint32_t)1U);
  Prims_list__Spec_Graph3_edge *buf =
      KRML_HOST_MALLOC(sizeof(Prims_list__Spec_Graph3_edge));
  buf[0U] =
      ((Prims_list__Spec_Graph3_edge){.tag = Prims_Cons, .hd = x, .tl = s});
  return buf;
}

static Spec_Graph3_edge
hd__uint64_t___uint64_t0(Prims_list__Spec_Graph3_edge *s) {
  return hd__uint64_t___uint64_t(s);
}

static Prims_list__Spec_Graph3_edge *
slice___uint64_t___uint64_t(Prims_list__Spec_Graph3_edge *s, Prims_int i,
                            Prims_int j) {
  if (Prims_op_GreaterThan(i, (krml_checked_int_t)0))
    return slice___uint64_t___uint64_t(
        tl__uint64_t___uint64_t0(s),
        Prims_op_Subtraction(i, (krml_checked_int_t)1),
        Prims_op_Subtraction(j, (krml_checked_int_t)1));
  else if (j == (krml_checked_int_t)0) {
    KRML_CHECK_SIZE(sizeof(Prims_list__Spec_Graph3_edge), (uint32_t)1U);
    Prims_list__Spec_Graph3_edge *buf =
        KRML_HOST_MALLOC(sizeof(Prims_list__Spec_Graph3_edge));
    buf[0U] = ((Prims_list__Spec_Graph3_edge){.tag = Prims_Nil});
    return buf;
  } else
    return cons__uint64_t___uint64_t(
        hd__uint64_t___uint64_t0(s),
        slice___uint64_t___uint64_t(
            tl__uint64_t___uint64_t0(s), i,
            Prims_op_Subtraction(j, (krml_checked_int_t)1)));
}

static Prims_list__Spec_Graph3_edge *(*slice__uint64_t___uint64_t)(
    Prims_list__Spec_Graph3_edge *x0, Prims_int x1,
    Prims_int x2) = slice___uint64_t___uint64_t;

static Prims_list__Spec_Graph3_edge *
tail__uint64_t___uint64_t0(Prims_list__Spec_Graph3_edge *s) {
  return slice__uint64_t___uint64_t(s, (krml_checked_int_t)1,
                                    length__uint64_t___uint64_t0(s));
}

static Prims_list__uint64_t *append__uint64_t(Prims_list__uint64_t *x,
                                              Prims_list__uint64_t *y) {
  if (x->tag == Prims_Nil)
    return y;
  else if (x->tag == Prims_Cons) {
    Prims_list__uint64_t *tl1 = x->tl;
    uint64_t a1 = x->hd;
    KRML_CHECK_SIZE(sizeof(Prims_list__uint64_t), (uint32_t)1U);
    Prims_list__uint64_t *buf = KRML_HOST_MALLOC(sizeof(Prims_list__uint64_t));
    buf[0U] = ((Prims_list__uint64_t){
        .tag = Prims_Cons, .hd = a1, .tl = append__uint64_t(tl1, y)});
    return buf;
  } else {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n", __FILE__, __LINE__,
                      "unreachable (pattern matches are exhaustive in F*)");
    KRML_HOST_EXIT(255U);
  }
}

static Prims_list__uint64_t *append__uint64_t0(Prims_list__uint64_t *s1,
                                               Prims_list__uint64_t *s2) {
  return append__uint64_t(s1, s2);
}

static Prims_list__uint64_t *cons__uint64_t(uint64_t x,
                                            Prims_list__uint64_t *s) {
  KRML_CHECK_SIZE(sizeof(Prims_list__uint64_t), (uint32_t)1U);
  Prims_list__uint64_t *buf = KRML_HOST_MALLOC(sizeof(Prims_list__uint64_t));
  buf[0U] = ((Prims_list__uint64_t){.tag = Prims_Cons, .hd = x, .tl = s});
  return buf;
}

static Prims_list__uint64_t *create__uint64_t(Prims_int len, uint64_t v) {
  if (len == (krml_checked_int_t)0) {
    KRML_CHECK_SIZE(sizeof(Prims_list__uint64_t), (uint32_t)1U);
    Prims_list__uint64_t *buf = KRML_HOST_MALLOC(sizeof(Prims_list__uint64_t));
    buf[0U] = ((Prims_list__uint64_t){.tag = Prims_Nil});
    return buf;
  } else
    return cons__uint64_t(
        v,
        create__uint64_t(Prims_op_Subtraction(len, (krml_checked_int_t)1), v));
}

static Prims_list__uint64_t *cons__uint64_t0(uint64_t x,
                                             Prims_list__uint64_t *s) {
  return append__uint64_t0(create__uint64_t((krml_checked_int_t)1, x), s);
}

static Prims_list__uint64_t *successors_fn2(uint64_t i,
                                            Prims_list__Spec_Graph3_edge *e) {
  if (length__uint64_t___uint64_t0(e) == (krml_checked_int_t)0)
    return empty__uint64_t();
  else {
    uint64_t f = fst__uint64_t_uint64_t(head__uint64_t___uint64_t(e));
    uint64_t s = snd__uint64_t_uint64_t(head__uint64_t___uint64_t(e));
    Prims_list__uint64_t *sl_ =
        successors_fn2(i, tail__uint64_t___uint64_t0(e));
    if (f == i) {
      Prims_list__uint64_t *sl = cons__uint64_t0(s, sl_);
      return sl;
    } else
      return sl_;
  }
}

Prims_list__uint64_t *Spec_Graph3_successors(Spec_Graph3_graph_state g,
                                             uint64_t i) {
  Prims_list__Spec_Graph3_edge *e = g.edge_set;
  return successors_fn2(i, e);
}

static Prims_int length__uint64_t(Prims_list__uint64_t *uu___) {
  if (uu___->tag == Prims_Nil)
    return (krml_checked_int_t)0;
  else if (uu___->tag == Prims_Cons) {
    Prims_list__uint64_t *tl1 = uu___->tl;
    return Prims_op_Addition((krml_checked_int_t)1, length__uint64_t(tl1));
  } else {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n", __FILE__, __LINE__,
                      "unreachable (pattern matches are exhaustive in F*)");
    KRML_HOST_EXIT(255U);
  }
}

static Prims_int length__uint64_t0(Prims_list__uint64_t *s) {
  return length__uint64_t(s);
}

bool Spec_Graph3_is_emptySet(Prims_list__uint64_t *s) {
  if (length__uint64_t0(s) == (krml_checked_int_t)0)
    return true;
  else
    return false;
}

static uint64_t hd__uint64_t(Prims_list__uint64_t *uu___) {
  if (uu___->tag == Prims_Cons)
    return uu___->hd;
  else {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n", __FILE__, __LINE__,
                      "unreachable (pattern matches are exhaustive in F*)");
    KRML_HOST_EXIT(255U);
  }
}

static Prims_list__uint64_t *tail__uint64_t(Prims_list__uint64_t *uu___) {
  if (uu___->tag == Prims_Cons)
    return uu___->tl;
  else {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n", __FILE__, __LINE__,
                      "unreachable (pattern matches are exhaustive in F*)");
    KRML_HOST_EXIT(255U);
  }
}

static Prims_list__uint64_t *(*tl__uint64_t)(Prims_list__uint64_t *x0) =
    tail__uint64_t;

static uint64_t index__uint64_t(Prims_list__uint64_t *l, Prims_int i) {
  if (i == (krml_checked_int_t)0)
    return hd__uint64_t(l);
  else
    return index__uint64_t(tl__uint64_t(l),
                           Prims_op_Subtraction(i, (krml_checked_int_t)1));
}

static uint64_t index__uint64_t0(Prims_list__uint64_t *s, Prims_int i) {
  return index__uint64_t(s, i);
}

uint64_t Spec_Graph3_get_last_elem(Spec_Graph3_graph_state g,
                                   Prims_list__uint64_t *s) {
  return index__uint64_t0(
      s, Prims_op_Subtraction(length__uint64_t0(s), (krml_checked_int_t)1));
}

static Prims_list__uint64_t *tl__uint64_t0(Prims_list__uint64_t *s) {
  return tl__uint64_t(s);
}

static uint64_t hd__uint64_t0(Prims_list__uint64_t *s) {
  return hd__uint64_t(s);
}

static Prims_list__uint64_t *slice___uint64_t(Prims_list__uint64_t *s,
                                              Prims_int i, Prims_int j) {
  if (Prims_op_GreaterThan(i, (krml_checked_int_t)0))
    return slice___uint64_t(tl__uint64_t0(s),
                            Prims_op_Subtraction(i, (krml_checked_int_t)1),
                            Prims_op_Subtraction(j, (krml_checked_int_t)1));
  else if (j == (krml_checked_int_t)0) {
    KRML_CHECK_SIZE(sizeof(Prims_list__uint64_t), (uint32_t)1U);
    Prims_list__uint64_t *buf = KRML_HOST_MALLOC(sizeof(Prims_list__uint64_t));
    buf[0U] = ((Prims_list__uint64_t){.tag = Prims_Nil});
    return buf;
  } else
    return cons__uint64_t(
        hd__uint64_t0(s),
        slice___uint64_t(tl__uint64_t0(s), i,
                         Prims_op_Subtraction(j, (krml_checked_int_t)1)));
}

static Prims_list__uint64_t *(*slice__uint64_t)(
    Prims_list__uint64_t *x0, Prims_int x1, Prims_int x2) = slice___uint64_t;

Prims_list__uint64_t *Spec_Graph3_get_first(Spec_Graph3_graph_state g,
                                            Prims_list__uint64_t *s) {
  Prims_list__uint64_t *s_ = slice__uint64_t(
      s, (krml_checked_int_t)0,
      Prims_op_Subtraction(length__uint64_t0(s), (krml_checked_int_t)1));
  if (Prims_op_GreaterThan(length__uint64_t0(s_), (krml_checked_int_t)0))
    return s_;
  else
    return s_;
}

static uint64_t head__uint64_t(Prims_list__uint64_t *s) {
  return index__uint64_t0(s, (krml_checked_int_t)0);
}

static Prims_list__uint64_t *tail__uint64_t0(Prims_list__uint64_t *s) {
  return slice__uint64_t(s, (krml_checked_int_t)1, length__uint64_t0(s));
}

static Prims_int count__uint64_t(uint64_t x, Prims_list__uint64_t *s) {
  if (length__uint64_t0(s) == (krml_checked_int_t)0)
    return (krml_checked_int_t)0;
  else if (head__uint64_t(s) == x)
    return Prims_op_Addition((krml_checked_int_t)1,
                             count__uint64_t(x, tail__uint64_t0(s)));
  else
    return count__uint64_t(x, tail__uint64_t0(s));
}

static bool mem__uint64_t(uint64_t x, Prims_list__uint64_t *l) {
  return Prims_op_GreaterThan(count__uint64_t(x, l), (krml_checked_int_t)0);
}

static Prims_list__uint64_t *snoc__uint64_t(Prims_list__uint64_t *s,
                                            uint64_t x) {
  return append__uint64_t0(s, create__uint64_t((krml_checked_int_t)1, x));
}

static Prims_list__uint64_t *
insert_to_vertex_set_snoc(uint64_t x, Prims_list__uint64_t *s) {
  if (mem__uint64_t(x, s))
    return s;
  else if (length__uint64_t0(s) == (krml_checked_int_t)0) {
    Prims_list__uint64_t *s_ = create__uint64_t((krml_checked_int_t)1, x);
    return s_;
  } else {
    Prims_list__uint64_t *s_ = snoc__uint64_t(s, x);
    return s_;
  }
}

Prims_list__uint64_t *
Spec_Graph3_insert_to_vertex_set(Spec_Graph3_graph_state g, uint64_t x,
                                 Prims_list__uint64_t *s) {
  return insert_to_vertex_set_snoc(x, s);
}

static Prims_list__uint64_t *union_vertex_sets_snoc(Spec_Graph3_graph_state g,
                                                    Prims_list__uint64_t *l1,
                                                    Prims_list__uint64_t *l2) {
  if (length__uint64_t0(l1) == (krml_checked_int_t)0)
    return l2;
  else if (mem__uint64_t(head__uint64_t(l1), l2))
    return union_vertex_sets_snoc(g, tail__uint64_t0(l1), l2);
  else {
    Prims_list__uint64_t *u =
        union_vertex_sets_snoc(g, tail__uint64_t0(l1), l2);
    if (length__uint64_t0(u) == (krml_checked_int_t)0)
      return create__uint64_t((krml_checked_int_t)1, head__uint64_t(l1));
    else
      return snoc__uint64_t(union_vertex_sets_snoc(g, tail__uint64_t0(l1), l2),
                            head__uint64_t(l1));
  }
}

Prims_list__uint64_t *Spec_Graph3_union_vertex_sets(Spec_Graph3_graph_state g,
                                                    Prims_list__uint64_t *l1,
                                                    Prims_list__uint64_t *l2) {
  return union_vertex_sets_snoc(g, l1, l2);
}

static Prims_list__uint64_t *push_to_stack_graph1(Prims_list__uint64_t *st,
                                                  uint64_t x) {
  if (length__uint64_t0(st) == (krml_checked_int_t)0) {
    Prims_list__uint64_t *stk_ = create__uint64_t((krml_checked_int_t)1, x);
    return stk_;
  } else {
    Prims_list__uint64_t *st_ = snoc__uint64_t(st, x);
    return st_;
  }
}

static Prims_list__uint64_t *successor_push_body1(Prims_list__uint64_t *l,
                                                  Prims_list__uint64_t *vl,
                                                  Prims_list__uint64_t *st,
                                                  Prims_int j) {
  uint64_t succ = index__uint64_t0(l, j);
  if (!mem__uint64_t(succ, vl) && !mem__uint64_t(succ, st)) {
    Prims_list__uint64_t *st_ = push_to_stack_graph1(st, succ);
    return st_;
  } else
    return st;
}

Prims_list__uint64_t *Spec_Graph3_successor_push_itr1(Prims_list__uint64_t *l,
                                                      Prims_list__uint64_t *vl,
                                                      Prims_list__uint64_t *st,
                                                      Prims_int j) {
  if (Prims_op_GreaterThanOrEqual(j, length__uint64_t0(l)))
    return st;
  else {
    Prims_int j_ = Prims_op_Addition(j, (krml_checked_int_t)1);
    Prims_list__uint64_t *st_ = successor_push_body1(l, vl, st, j);
    Prims_list__uint64_t *st__ =
        Spec_Graph3_successor_push_itr1(l, vl, st_, j_);
    return st__;
  }
}

static Prims_list__uint64_t *
remove_all_instances_of_vertex_from_vertex_set_cons(Prims_list__uint64_t *l,
                                                    Prims_list__uint64_t *vl) {
  if (length__uint64_t0(l) == (krml_checked_int_t)0)
    return empty__uint64_t();
  else if (mem__uint64_t(head__uint64_t(l), vl))
    return remove_all_instances_of_vertex_from_vertex_set_cons(
        tail__uint64_t0(l), vl);
  else {
    Prims_list__uint64_t *u =
        remove_all_instances_of_vertex_from_vertex_set_cons(tail__uint64_t0(l),
                                                            vl);
    if (mem__uint64_t(head__uint64_t(l), u))
      return u;
    else {
      Prims_list__uint64_t *u_ = cons__uint64_t0(head__uint64_t(l), u);
      return u_;
    }
  }
}

Prims_list__uint64_t *
Spec_Graph3_remove_all_instances_of_vertex_from_vertex_set(
    Prims_list__uint64_t *l, Prims_list__uint64_t *vl) {
  return remove_all_instances_of_vertex_from_vertex_set_cons(l, vl);
}
/* #define Caml_white (0 << 8) */
/* #define Caml_gray  (1 << 8) */
/* #define Caml_blue  (2 << 8) */
/* #define Caml_black (3 << 8) */

uint64_t Spec_GC_closure_infix_blue = (uint64_t)2U;
uint64_t Spec_GC_closure_infix_white = (uint64_t)0U;

uint64_t Spec_GC_closure_infix_gray = (uint64_t)1U;

uint64_t Spec_GC_closure_infix_black = (uint64_t)3U;

uint64_t Spec_GC_closure_infix_getWosize(uint64_t h) {
  return h >> (uint32_t)10U;
}

uint64_t Spec_GC_closure_infix_getColor(uint64_t h) {
  uint64_t c_ = h >> (uint32_t)8U;
  return c_ & (uint64_t)3U;
}

uint64_t Spec_GC_closure_infix_getTag(uint64_t h) { return h & (uint64_t)255U; }

uint64_t Spec_GC_closure_infix_makeHeader(uint64_t wz, uint64_t c,
                                          uint64_t tg) {
  uint64_t l1 = wz << (uint32_t)10U;
  uint64_t l2 = c << (uint32_t)8U;
  uint64_t l3 = tg << (uint32_t)0U;
  return l1 + l2 + l3;
}

uint64_t Spec_GC_closure_infix_extract_start_env_bits_(uint64_t clos_info) {
  uint64_t l1 = clos_info << (uint32_t)8U;
  return l1 >> (uint32_t)9U;
}

uint64_t Spec_GC_closure_infix_hd_address(uint64_t st_index) {
  return st_index - (uint64_t)8U;
}

uint64_t Spec_GC_closure_infix_f_address(uint64_t h_index) {
  return h_index + (uint64_t)8U;
}
