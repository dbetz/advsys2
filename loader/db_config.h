/* db_config.h - system configuration for a simple virtual machine
 *
 * Copyright (c) 2011 by David Michael Betz.  All rights reserved.
 *
 */

#ifndef __DB_CONFIG_H__
#define __DB_CONFIG_H__

#include <stdio.h>
#include "db_system.h"

/* configuration variables

MAXCODE		the size of the bytecode staging buffer used by the compiler

*/

/* support for windows */
#if defined(WIN32)
#define WORD_SIZE_32
#define FAR_DATA
#define FLASH_SPACE		const
#define MAXCODE         (32 * 1024)
#define NEED_STRCASECMP

/* support for posix (cygwin, linux, macosx) */
#else
#define WORD_SIZE_32
#define FAR_DATA
#define FLASH_SPACE		const
#define MAXCODE         (32 * 1024)

#endif

/* for all propeller platforms */
#define HUB_BASE        0x00000000
#define HUB_SIZE        (32 * 1024)
#define COG_BASE        0x10000000
#define RAM_BASE        0x20000000
#define FLASH_BASE      0x30000000

#ifdef NO_STDINT
typedef long int32_t;
typedef unsigned long uint32_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef signed char int8_t;
typedef unsigned char uint8_t;
#else
#include <stdint.h>
#endif

#ifdef WORD_SIZE_16
typedef int16_t VMVALUE;
typedef uint16_t VMUVALUE;
#endif

#ifdef WORD_SIZE_32
typedef int32_t VMVALUE;
typedef uint32_t VMUVALUE;
#endif

#define RCFAST      0x00
#define RCSLOW      0x01
#define XINPUT      0x22
#define XTAL1       0x2a
#define XTAL2       0x32
#define XTAL3       0x3a
#define PLL1X       0x41
#define PLL2X       0x42
#define PLL4X       0x43
#define PLL8X       0x44
#define PLL16X      0x45

#ifdef NEED_STRCASECMP
int strcasecmp(const char *s1, const char *s2);
#endif

#define ALIGN_MASK				(sizeof(VMVALUE) - 1)
#define ROUND_TO_WORDS(x)       (((x) + ALIGN_MASK) & ~ALIGN_MASK)

#define VMCODEBYTE(p)           *(p)
#define VMINTRINSIC(i)          Intrinsics[i]

/* memory section */
typedef struct Section Section;
struct Section {
    VMUVALUE base;      // base address
    VMUVALUE size;      // maximum size
    VMUVALUE offset;    // next available offset
    FILE *fp;           // image or scratch file pointer
    Section *next;      // next section
    char name[1];       // section name
};

typedef struct BoardConfig BoardConfig;
struct BoardConfig {
    uint32_t clkfreq;
    uint8_t clkmode;
    uint32_t baudrate;
    uint8_t rxpin;
    uint8_t txpin;
    uint8_t tvpin;
    char *cacheDriver;
    VMUVALUE cacheSize;
    VMUVALUE cacheParam1;
    VMUVALUE cacheParam2;
    char *defaultTextSection;
    char *defaultDataSection;
    int sectionCount;
    Section *sections;
    BoardConfig *next;
    char name[1];
};

void ParseConfigurationFile(System *sys, const char *path);
BoardConfig *GetBoardConfig(const char *name);
Section *GetSection(BoardConfig *config, const char *name);

#endif
