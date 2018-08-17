/* adv2vmdebug.c - debug routines
 *
 * Copyright (c) 2018 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "adv2vmdebug.h"
#include "adv2image.h"

/* instruction output formats */
#define FMT_NONE        0
#define FMT_BYTE        1
#define FMT_SBYTE       2
#define FMT_LONG        3
#define FMT_BR          4

typedef struct {
    int code;
    char *name;
    int fmt;
} OTDEF;

OTDEF OpcodeTable[] = {
{ OP_HALT,      "HALT",     FMT_NONE    },
{ OP_BRT,       "BRT",      FMT_BR      },
{ OP_BRTSC,     "BRTSC",    FMT_BR      },
{ OP_BRF,       "BRF",      FMT_BR      },
{ OP_BRFSC,     "BRFSC",    FMT_BR      },
{ OP_BR,        "BR",       FMT_BR      },
{ OP_NOT,       "NOT",      FMT_NONE    },
{ OP_NEG,       "NEG",      FMT_NONE    },
{ OP_ADD,       "ADD",      FMT_NONE    },
{ OP_SUB,       "SUB",      FMT_NONE    },
{ OP_MUL,       "MUL",      FMT_NONE    },
{ OP_DIV,       "DIV",      FMT_NONE    },
{ OP_REM,       "REM",      FMT_NONE    },
{ OP_BNOT,      "BNOT",     FMT_NONE    },
{ OP_BAND,      "BAND",     FMT_NONE    },
{ OP_BOR,       "BOR",      FMT_NONE    },
{ OP_BXOR,      "BXOR",     FMT_NONE    },
{ OP_SHL,       "SHL",      FMT_NONE    },
{ OP_SHR,       "SHR",      FMT_NONE    },
{ OP_LT,        "LT",       FMT_NONE    },
{ OP_LE,        "LE",       FMT_NONE    },
{ OP_EQ,        "EQ",       FMT_NONE    },
{ OP_NE,        "NE",       FMT_NONE    },
{ OP_GE,        "GE",       FMT_NONE    },
{ OP_GT,        "GT",       FMT_NONE    },
{ OP_LIT,       "LIT",      FMT_LONG    },
{ OP_SLIT,      "SLIT",     FMT_SBYTE   },
{ OP_LOAD,      "LOAD",     FMT_NONE    },
{ OP_LOADB,     "LOADB",    FMT_NONE    },
{ OP_STORE,     "STORE",    FMT_NONE    },
{ OP_STOREB,    "STOREB",   FMT_NONE    },
{ OP_LADDR,     "LADDR",    FMT_SBYTE   },
{ OP_INDEX,     "INDEX",    FMT_NONE    },
{ OP_CALL,      "CALL",     FMT_BYTE    },
{ OP_FRAME,     "FRAME",    FMT_BYTE    },
{ OP_RETURN,    "RETURN",   FMT_NONE    },
{ OP_DROP,      "DROP",     FMT_NONE    },
{ OP_DUP,       "DUP",      FMT_NONE    },
{ OP_TUCK,      "TUCK",     FMT_NONE    },
{ OP_NATIVE,    "NATIVE",   FMT_LONG    },
{ OP_TRAP,      "TRAP",     FMT_BYTE    },
{ OP_SEND,      "SEND",     FMT_BYTE    },
{ OP_DADDR,     "DADDR",    FMT_LONG    },
{ OP_PADDR,     "PADDR",    FMT_NONE    },
{ OP_CLASS,     "CLASS",    FMT_NONE    },
{ OP_THROW,     "THROW",    FMT_NONE    },
{ 0,            NULL,       0           }
};

/* DecodeFunction - decode the instructions in a function code object */
void DecodeFunction(const uint8_t *base, const uint8_t *code, int len)
{
    const uint8_t *lc = code;
    const uint8_t *end = code + len;
    while (lc < end)
        lc += DecodeInstruction(base, lc);
}

/* DecodeInstruction - decode a single bytecode instruction */
int DecodeInstruction(const uint8_t *base, const uint8_t *lc)
{
    uint8_t opcode, bytes[sizeof(VMVALUE)];
    const OTDEF *op;
    VMWORD offset;
    int8_t sbyte;
    int n, i;

    /* get the opcode */
    opcode = VMCODEBYTE(lc);

    /* show the address */
    printf("%04x %02x ", lc - base, opcode);
    n = 1;

    /* display the operands */
    for (op = OpcodeTable; op->name; ++op)
        if (opcode == op->code) {
            switch (op->fmt) {
            case FMT_NONE:
                for (i = 0; i < sizeof(VMVALUE); ++i)
                    printf("   ");
                printf("%s\n", op->name);
                break;
            case FMT_BYTE:
                bytes[0] = VMCODEBYTE(lc + 1);
                printf("%02x ", bytes[0]);
                for (i = 1; i < sizeof(VMVALUE); ++i)
                    printf("   ");
                printf("%s %02x\n", op->name, bytes[0]);
                n += 1;
                break;
            case FMT_SBYTE:
                sbyte = (int8_t)VMCODEBYTE(lc + 1);
                printf("%02x ", (uint8_t)sbyte);
                for (i = 1; i < sizeof(VMVALUE); ++i)
                    printf("   ");
                printf("%s %d\n", op->name, sbyte);
                n += 1;
                break;
            case FMT_LONG:
                for (i = 0; i < sizeof(VMVALUE); ++i) {
                    bytes[i] = VMCODEBYTE(lc + i + 1);
                    printf("%02x ", bytes[i]);
                }
                printf("%s ", op->name);
                for (i = 0; i < sizeof(VMVALUE); ++i)
                    printf("%02x", bytes[i]);
                printf("\n");
                n += sizeof(VMVALUE);
                break;
            case FMT_BR:
                offset = 0;
                for (i = 0; i < sizeof(VMWORD); ++i) {
                    bytes[i] = VMCODEBYTE(lc + i + 1);
                    offset = (offset << 8) | bytes[i];
                    printf("%02x ", bytes[i]);
                }
                for (i = sizeof(VMWORD); i < sizeof(VMVALUE); ++i)
                    printf("   ");
                printf("%s ", op->name);
                for (i = 0; i < sizeof(VMWORD); ++i)
                    printf("%02x", bytes[i]);
                printf(" # %04x\n", (lc + 1 + sizeof(VMWORD) + offset) - base);
                n += sizeof(VMWORD);
                break;
            }
            return n;
        }
            
    /* unknown opcode */
    printf("      <UNKNOWN>\n");
    return 1;
}

