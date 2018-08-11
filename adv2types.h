/* db_types.h - type definitions for a simple virtual machine
 *
 * Copyright (c) 2014 by David Michael Betz.  All rights reserved.
 *
 */

#ifndef __DB_TYPES_H__
#define __DB_TYPES_H__

/**********/
/* Common */
/**********/

#define VMTRUE      1
#define VMFALSE     0

/* system heap size (includes compiler heap and image buffer) */
#define HEAPSIZE            5000

/* size of image buffer (allocated from system heap) */
#define IMAGESIZE           2500

/* edit buffer size (separate from the system heap) */
#define EDITBUFSIZE         1500

/* minimum stack size in bytes */
#define MIN_STACK_SIZE      128

/*****************/
/* MAC and LINUX */
/*****************/

#if defined(MAC) || defined(LINUX)

#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef int16_t VMWORD;
typedef int32_t VMVALUE;
typedef uint32_t VMUVALUE;

#define ALIGN_MASK              3

#define VMCODEBYTE(p)           *(uint8_t *)(p)
#define VMINTRINSIC(i)          Intrinsics[i]

#define ANSI_FILE_IO

#endif  // MAC

/*********/
/* PIC32 */
/*********/

#if defined(PIC32)

#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef int16_t VMWORD;
typedef int32_t VMVALUE;
typedef uint32_t VMUVALUE;

#define ALIGN_MASK              3

#define VMCODEBYTE(p)           *(uint8_t *)(p)
#define VMINTRINSIC(i)          Intrinsics[i]

#define LINE_EDIT
#define ECHO_INPUT

#endif  // MAC

/*****************/
/* PROPELLER_GCC */
/*****************/

#ifdef PROPELLER_GCC

#include <string.h>
#include <stdint.h>

typedef int16_t VMWORD;
typedef int32_t VMVALUE;
typedef uint32_t VMUVALUE;

#define ALIGN_MASK              3

#define VMCODEBYTE(p)           *(uint8_t *)(p)
#define VMINTRINSIC(i)          Intrinsics[i]

#define PROPELLER
#define ANSI_FILE_IO

#endif  // PROPELLER_GCC

/****************/
/* ANSI_FILE_IO */
/****************/

#ifdef ANSI_FILE_IO

#if 0

#include <stdio.h>
#include <dirent.h>

typedef FILE VMFILE;

#define VM_fopen	fopen
#define VM_fclose	fclose
#define VM_fgets	fgets
#define VM_fputs	fputs

struct VMDIR {
    DIR *dirp;
};

struct VMDIRENT {
    char name[FILENAME_MAX];
};

#endif

#endif // ANSI_FILE_IO

#endif
