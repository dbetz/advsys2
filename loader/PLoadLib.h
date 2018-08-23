#ifndef __PLOADLIB_H__
#define __PLOADLIB_H__

#include <stdint.h>

#define SHUTDOWN_CMD            0
#define DOWNLOAD_RUN_BINARY     1
#define DOWNLOAD_EEPROM         2
#define DOWNLOAD_RUN_EEPROM     3

int pload(char* file, char* port, int type);
int ploadbuf(uint8_t* dlbuf, int count, char* port, int type);

#endif
