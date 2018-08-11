/* adv2vmdebug.h - debug definitions
 *
 * Copyright (c) 2018 by David Michael Betz.  All rights reserved.
 *
 */

#ifndef __DB_VMDEBUG_H__
#define __DB_VMDEBUG_H__

#include <stdint.h>

void DecodeFunction(const uint8_t *code, int len);
int DecodeInstruction(const uint8_t *code, const uint8_t *lc);

#endif
