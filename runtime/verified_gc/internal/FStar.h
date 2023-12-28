#ifndef __internal_FStar_H
#define __internal_FStar_H

#include "alias.h"
#include <stdint.h>
extern Prims_int FStar_UInt32_v(uint32_t x);

extern uint32_t FStar_UInt32_uint_to_t(Prims_int x);

extern Prims_int FStar_UInt64_v(uint64_t x);

extern uint64_t FStar_UInt64_uint_to_t(Prims_int x);

#define __internal_FStar_H_DEFINED
#endif
