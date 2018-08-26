/* adv2image.h - compiled image file definitions
 *
 * Copyright (c) 2018 by David Michael Betz.  All rights reserved.
 *
 */

#ifndef __ADV2IMAGE_H__
#define __ADV2IMAGE_H__

#include "adv2types.h"

/* end of a list */
#define NIL         0

/* flag indicating that a property is shared with its child objects */
#define P_SHARED    0x80000000

/* image header */
typedef struct {
    VMVALUE dataOffset;
    VMVALUE dataSize;
    VMVALUE codeOffset;
    VMVALUE codeSize;
    VMVALUE stringOffset;
    VMVALUE stringSize;
    VMVALUE mainFunction;
} ImageHdr;

/* property structure */
typedef struct {
    VMVALUE tag;
    VMVALUE value;
} Property;

/* object header */
typedef struct {
    VMVALUE class;
    VMVALUE nProperties;
    /* properties follow */
} ObjectHdr;

/* stack frame format:

sp -> saved fp
      local n
      local n-1
      ...
      local 2
      local 1
fp -> arg 1
      arg 2
      ...
      arg m-1
      arg m
*/

/* opcodes */
#define OP_HALT         0x00    /* halt */
#define OP_BRT          0x01    /* branch on true */
#define OP_BRTSC        0x02    /* branch on true (for short circuit booleans) */
#define OP_BRF          0x03    /* branch on false */
#define OP_BRFSC        0x04    /* branch on false (for short circuit booleans) */
#define OP_BR           0x05    /* branch unconditionally */
#define OP_NOT          0x06    /* logical negate top of stack */
#define OP_NEG          0x07    /* negate */
#define OP_ADD          0x08    /* add two numeric expressions */
#define OP_SUB          0x09    /* subtract two numeric expressions */
#define OP_MUL          0x0a    /* multiply two numeric expressions */
#define OP_DIV          0x0b    /* divide two numeric expressions */
#define OP_REM          0x0c    /* remainder of two numeric expressions */
#define OP_BNOT         0x0d    /* bitwise not of two numeric expressions */
#define OP_BAND         0x0e    /* bitwise and of two numeric expressions */
#define OP_BOR          0x0f    /* bitwise or of two numeric expressions */
#define OP_BXOR         0x10    /* bitwise exclusive or */
#define OP_SHL          0x11    /* shift left */
#define OP_SHR          0x12    /* shift right */
#define OP_LT           0x13    /* less than */
#define OP_LE           0x14    /* less than or equal to */
#define OP_EQ           0x15    /* equal to */
#define OP_NE           0x16    /* not equal to */
#define OP_GE           0x17    /* greater than or equal to */
#define OP_GT           0x18    /* greater than */
#define OP_LIT          0x19    /* load a literal */
#define OP_SLIT         0x1a    /* load a short literal (-128 to 127) */
#define OP_LOAD         0x1b    /* load a long from memory */
#define OP_LOADB        0x1c    /* load a byte from memory */
#define OP_STORE        0x1d    /* store a long into memory */
#define OP_STOREB       0x1e    /* store a byte into memory */
#define OP_LADDR        0x1f    /* load the address of a local variable */
#define OP_INDEX        0x20    /* index into a vector of longs */
#define OP_CALL         0x21    /* call a function */
#define OP_FRAME        0x22    /* create a stack frame */
#define OP_RETURN       0x23    /* remove a stack frame and return from a function call */
#define OP_RETURNZ      0x24    /* remove a stack frame and return from a function call */
#define OP_DROP         0x25    /* drop the top element of the stack */
#define OP_DUP          0x26    /* duplicate the top element of the stack */
#define OP_TUCK         0x27    /* a b -> b a b */
#define OP_SWAP         0x28    /* swap the top two elements on the stack */
#define OP_TRAP         0x29    /* trap to handler */
#define OP_SEND         0x2a    /* send a message to an object */
#define OP_DADDR        0x2b    /* load the address of an object in data space */
#define OP_PADDR        0x2c    /* load the address of an object property */
#define OP_CLASS        0x2d    /* load the class of an object */
#define OP_TRY          0x2e    /* enter try code */
#define OP_TRYEXIT      0x2f    /* exit try code */
#define OP_THROW        0x30    /* throw an exception */

/* VM trap codes */
enum {
    TRAP_GetChar      = 0,
    TRAP_PutChar      = 1,
    TRAP_PrintStr     = 2,
    TRAP_PrintInt     = 3,
    TRAP_PrintTab     = 4,
    TRAP_PrintNL      = 5,
    TRAP_PrintFlush   = 6,
};

/* prototypes */
void InitImage(ImageHdr *image);

#endif
