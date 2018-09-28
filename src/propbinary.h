#ifndef __PROPBINARY_H__
#define __PROPBINARY_H__

#include <stdint.h>

uint8_t *BuildBinary(uint8_t *template, int templateSize, uint8_t *image, int imageSize, int *pBinarySize);
void DumpSpinBinary(uint8_t *binary);

#endif

