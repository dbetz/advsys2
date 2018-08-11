/* adv2types.h - type definitions for a simple virtual machine
 *
 * Copyright (c) 2018 by David Michael Betz.  All rights reserved.
 *
 */

#ifndef __ADV2TYPES_H__
#define __ADV2TYPES_H__

#include <stdint.h>

#define VMTRUE      1
#define VMFALSE     0

typedef int16_t VMWORD;
typedef int32_t VMVALUE;
typedef uint32_t VMUVALUE;

#define VMCODEBYTE(p)   *(uint8_t *)(p)

#endif
