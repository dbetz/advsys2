/* adv2exe.c - bytecode interpreter for a simple virtual machine
 *
 * Copyright (c) 2018 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include "adv2vm.h"
#include "adv2vmdebug.h"

/* interpreter state structure */
typedef struct {
    jmp_buf errorTarget;
    uint8_t *dataBase;
    uint8_t *dataTop;
    uint8_t *codeBase;
    uint8_t *codeTop;
    uint8_t *stringBase;
    uint8_t *stringTop;
    VMVALUE *stack;
    VMVALUE *stackTop;
    uint8_t *pc;
    VMVALUE *fp;
    VMVALUE *sp;
    VMVALUE tos;
    VMVALUE *efp;
    int device;
} Interpreter;

/* stack manipulation macros */
#define Reserve(i, n)   do {                                    \
                            if ((i)->sp - (n) < (i)->stack)     \
                                StackOverflow(i);               \
                            else                                \
                                (i)->sp -= (n);                 \
                        } while (0)
#define Check(i, n)     do {                                    \
                            if ((i)->sp - (n) < (i)->stack)     \
                                StackOverflow(i);               \
                        } while (0)
#define CPush(i, v)     do {                                    \
                            if ((i)->sp <= (i)->stack)          \
                                StackOverflow(i);               \
                            else                                \
                                Push(i, v);                     \
                        } while (0)
#define Push(i, v)      (*--(i)->sp = (v))
#define Pop(i)          (*(i)->sp++)
#define Top(i)          (*(i)->sp)
#define Drop(i, n)      ((i)->sp += (n))
#define Ptr2Off(i, p)   (VMVALUE)(((uint8_t *)(p) - (i)->dataBase))
#define Off2Ptr(i, o)   ((i)->dataBase + (o))

/* prototypes for local functions */
static int GetPropertyAddr(Interpreter *i, VMVALUE object, VMVALUE property, VMVALUE **pPtr);
static void DoSend(Interpreter *i);
static void Throw(Interpreter *i, VMVALUE value);
static void DoTrap(Interpreter *i, int op);
static void StackOverflow(Interpreter *i);
static void Abort(Interpreter *i, const char *fmt, ...);
static void ShowStack(Interpreter *i);

/* Execute - execute the main code */
int Execute(ImageHdr *image, int debug)
{
    Interpreter *i;
    VMVALUE tmp, *p;
    VMWORD tmpw;
    int8_t tmpb;
    int cnt;

    /* allocate the interpreter state */
    if (!(i = (Interpreter *)malloc(sizeof(Interpreter) + MAXSTACK)))
        return VMFALSE;

	/* setup the new image */
	i->dataBase = (uint8_t *)image + image->dataOffset;
	i->dataTop = i->dataBase + image->dataSize;
	i->codeBase = (uint8_t *)image + image->codeOffset;
	i->codeTop = i->codeBase + image->codeSize;
	i->stringBase = (uint8_t *)image + image->stringOffset;
	i->stringTop = i->stringBase + image->stringSize;
    i->stack = (VMVALUE *)((uint8_t *)i + sizeof(Interpreter));
    i->stackTop = (VMVALUE *)((uint8_t *)i->stack + MAXSTACK);

    /* initialize */    
    i->pc = i->codeBase + image->mainFunction;
    i->sp = i->fp = i->stackTop;
    i->efp = NULL;
    
    /* set the default i/o device */
    i->device = -1;
    
    /* put the address of a HALT on the top of the stack */
    /* codeBase[0] is zero to act as the second byte of a fake CALL instruction */
    /* codeBase[1] is a HALT instruction */
    i->tos = Ptr2Off(i, i->codeBase + 1);
    
    if (setjmp(i->errorTarget))
        return VMFALSE;

    for (;;) {
        if (debug) {
            ShowStack(i);
            DecodeInstruction(i->codeBase, i->pc);
        }
        switch (VMCODEBYTE(i->pc++)) {
        case OP_HALT:
            return VMTRUE;
        case OP_BRT:
            for (tmpw = 0, cnt = sizeof(VMWORD); --cnt >= 0; )
                tmpw = (tmpw << 8) | VMCODEBYTE(i->pc++);
            if (i->tos)
                i->pc += tmpw;
            i->tos = Pop(i);
            break;
        case OP_BRTSC:
            for (tmpw = 0, cnt = sizeof(VMWORD); --cnt >= 0; )
                tmpw = (tmpw << 8) | VMCODEBYTE(i->pc++);
            if (i->tos)
                i->pc += tmpw;
            else
                i->tos = Pop(i);
            break;
        case OP_BRF:
            for (tmpw = 0, cnt = sizeof(VMWORD); --cnt >= 0; )
                tmpw = (tmpw << 8) | VMCODEBYTE(i->pc++);
            if (!i->tos)
                i->pc += tmpw;
            i->tos = Pop(i);
            break;
        case OP_BRFSC:
            for (tmpw = 0, cnt = sizeof(VMWORD); --cnt >= 0; )
                tmpw = (tmpw << 8) | VMCODEBYTE(i->pc++);
            if (!i->tos)
                i->pc += tmpw;
            else
                i->tos = Pop(i);
            break;
        case OP_BR:
            for (tmpw = 0, cnt = sizeof(VMWORD); --cnt >= 0; )
                tmpw = (tmpw << 8) | VMCODEBYTE(i->pc++);
            i->pc += tmpw;
            break;
        case OP_NOT:
            i->tos = (i->tos ? VMFALSE : VMTRUE);
            break;
        case OP_NEG:
            i->tos = -i->tos;
            break;
        case OP_ADD:
            tmp = Pop(i);
            i->tos = tmp + i->tos;
            break;
        case OP_SUB:
            tmp = Pop(i);
            i->tos = tmp - i->tos;
            break;
        case OP_MUL:
            tmp = Pop(i);
            i->tos = tmp * i->tos;
            break;
        case OP_DIV:
            tmp = Pop(i);
            i->tos = (i->tos == 0 ? 0 : tmp / i->tos);
            break;
        case OP_REM:
            tmp = Pop(i);
            i->tos = (i->tos == 0 ? 0 : tmp % i->tos);
            break;
        case OP_BNOT:
            i->tos = ~i->tos;
            break;
        case OP_BAND:
            tmp = Pop(i);
            i->tos = tmp & i->tos;
            break;
        case OP_BOR:
            tmp = Pop(i);
            i->tos = tmp | i->tos;
            break;
        case OP_BXOR:
            tmp = Pop(i);
            i->tos = tmp ^ i->tos;
            break;
        case OP_SHL:
            tmp = Pop(i);
            i->tos = tmp << i->tos;
            break;
        case OP_SHR:
            tmp = Pop(i);
            i->tos = tmp >> i->tos;
            break;
        case OP_LT:
            tmp = Pop(i);
            i->tos = (tmp < i->tos ? VMTRUE : VMFALSE);
            break;
        case OP_LE:
            tmp = Pop(i);
            i->tos = (tmp <= i->tos ? VMTRUE : VMFALSE);
            break;
        case OP_EQ:
            tmp = Pop(i);
            i->tos = (tmp == i->tos ? VMTRUE : VMFALSE);
            break;
        case OP_NE:
            tmp = Pop(i);
            i->tos = (tmp != i->tos ? VMTRUE : VMFALSE);
            break;
        case OP_GE:
            tmp = Pop(i);
            i->tos = (tmp >= i->tos ? VMTRUE : VMFALSE);
            break;
        case OP_GT:
            tmp = Pop(i);
            i->tos = (tmp > i->tos ? VMTRUE : VMFALSE);
            break;
        case OP_LIT:
            for (tmp = 0, cnt = sizeof(VMVALUE); --cnt >= 0; )
                tmp = (tmp << 8) | VMCODEBYTE(i->pc++);
            CPush(i, i->tos);
            i->tos = tmp;
            break;
        case OP_SLIT:
            tmpb = (int8_t)VMCODEBYTE(i->pc++);
            CPush(i, i->tos);
            i->tos = tmpb;
            break;
        case OP_LOAD:
            i->tos = *(VMVALUE *)(i->dataBase + i->tos);
            break;
        case OP_LOADB:
            i->tos = *(uint8_t *)(i->dataBase + i->tos);
            break;
        case OP_STORE:
            tmp = Pop(i);
            *(VMVALUE *)(i->dataBase + tmp) = i->tos;
            break;
        case OP_STOREB:
            tmp = Pop(i);
            *(uint8_t *)(i->dataBase + tmp) = i->tos;
            break;
        case OP_LADDR:
            tmpb = (int8_t)VMCODEBYTE(i->pc++);
            CPush(i, i->tos);
            i->tos = Ptr2Off(i, &i->fp[(int)tmpb]);
            break;
        case OP_INDEX:
            tmp = Pop(i);
            i->tos = tmp + i->tos * sizeof (VMVALUE);
            break;
        case OP_BINDEX:
            tmp = Pop(i);
            i->tos = tmp + i->tos;
            break;
        case OP_CALL:
            ++i->pc; // skip over the argument count
            tmp = i->tos;
            i->tos = Ptr2Off(i, i->pc);
            i->pc = i->codeBase + tmp;
            break;
        case OP_FRAME:
            cnt = VMCODEBYTE(i->pc++);
            tmp = Ptr2Off(i, i->fp);
            i->fp = i->sp;
            Reserve(i, cnt);
            *i->sp = tmp;
            break;
        case OP_RETURNZ:
            CPush(i, i->tos);
            i->tos = 0;
            // fall through
        case OP_RETURN:
            i->pc = Off2Ptr(i, Top(i));
            tmp = i->sp[1];
            i->sp = i->fp;
            Drop(i, i->pc[-1]);
            i->fp = (VMVALUE *)Off2Ptr(i, tmp);
            break;
        case OP_DROP:
            i->tos = Pop(i);
            break;
        case OP_DUP:
            CPush(i, i->tos);
            break;
        case OP_TUCK:
            CPush(i, 0);
            i->sp[0] = i->sp[1];
            i->sp[1] = i->tos;
            break;
        case OP_SWAP:
            tmp = i->tos;
            i->tos = *i->sp;
            *i->sp = tmp;
            break;
        case OP_TRAP:
            DoTrap(i, VMCODEBYTE(i->pc++));
            break;
        case OP_SEND:
            DoSend(i);
            break;
        case OP_PADDR:
            if (!GetPropertyAddr(i, Pop(i), i->tos, &p))
                Throw(i, 1);
            i->tos = Ptr2Off(i, p);
            break;
        case OP_CLASS:
            i->tos = ((ObjectHdr *)(i->dataBase + i->tos))->class;
            break;
        case OP_TRY:
            for (tmpw = 0, cnt = sizeof(VMWORD); --cnt >= 0; )
                tmpw = (tmpw << 8) | VMCODEBYTE(i->pc++);
            Check(i, 4);
            Push(i, i->tos);
            Push(i, Ptr2Off(i, i->pc + tmpw));
            Push(i, Ptr2Off(i, i->fp));
            Push(i, Ptr2Off(i, i->efp));
            i->efp = i->sp;
            break;
        case OP_TRYEXIT:
            tmp = Pop(i);
            i->fp = (VMVALUE *)Off2Ptr(i, Pop(i));
            Drop(i, 1);
            i->tos = Pop(i);
            i->efp = (VMVALUE *)Off2Ptr(i, tmp);
            break;
        case OP_THROW:
            Throw(i, i->tos);
            break;
        case OP_NATIVE:
            ++i->pc;
            break;
        default:
            Abort(i, "undefined opcode 0x%02x", VMCODEBYTE(i->pc - 1));
            break;
        }
    }
}

static int GetPropertyAddr(Interpreter *i, VMVALUE object, VMVALUE tag, VMVALUE **pPtr)
{
    ObjectHdr *hdr = (ObjectHdr *)(i->dataBase + object);
    while (object) {
        Property *property = (Property *)(hdr + 1);
        int nProperties = hdr->nProperties;
        while (--nProperties >= 0) {
            if ((property->tag & ~P_SHARED) == tag) {
                *pPtr = (VMVALUE *)&property->value;
                return VMTRUE;
            }
            ++property;
        }
        object = hdr->class;
        hdr = (ObjectHdr *)(i->dataBase + object);
    }
    return VMFALSE;
}

static void DoSend(Interpreter *i)
{
    VMVALUE obj, prop, *p;
    ++i->pc; // skip over the argument count
    prop = i->tos;
    i->tos = Ptr2Off(i, i->pc);
    if (!(obj = i->sp[1]))
        obj = i->sp[0];
    if (GetPropertyAddr(i, obj, prop, &p))
        i->pc = i->codeBase + *p;
    else
        Throw(i, 1);
}

static void Throw(Interpreter *i, VMVALUE value)
{
    VMVALUE tmp;
    if (!i->efp)
        Abort(i, "uncaught throw %d", value);
    i->sp = i->efp;
    tmp = Pop(i);
    i->fp = (VMVALUE *)Off2Ptr(i, Pop(i));
    i->pc = Off2Ptr(i, Pop(i));
    i->efp = (VMVALUE *)Off2Ptr(i, tmp);
    i->tos = value;
}

static void DoTrap(Interpreter *i, int op)
{
    switch (op) {
    case TRAP_GetChar:
        Push(i, i->tos);
        i->tos = getchar();
        break;
    case TRAP_PutChar:
        putchar(i->tos);
        i->tos = Pop(i);
        break;
    case TRAP_PrintStr:
        printf("%s", (char *)(i->dataBase + i->tos));
        i->tos = *i->sp++;
        break;
    case TRAP_PrintInt:
        printf("%d", i->tos);
        i->tos = *i->sp++;
        break;
    case TRAP_PrintNL:
        putchar('\n');
        break;
    case TRAP_SetDevice:
        i->device = i->tos;
        i->tos = Pop(i);
        break;
    default:
        Abort(i, "undefined trap %d", op);
        break;
    }
}

static void StackOverflow(Interpreter *i)
{
    Abort(i, "stack overflow");
}

static void Abort(Interpreter *i, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("error: ");
    vprintf(fmt, ap);
    putchar('\n');
    va_end(ap);
    longjmp(i->errorTarget, 1);
}

static void ShowOffset(Interpreter *i, VMVALUE value)
{
    uint8_t *p = (uint8_t *)Off2Ptr(i, value);
    if (p >= i->dataBase && p < i->dataTop)
        printf("(d:%d)", (int)(p - i->dataBase));
    else if (p >= i->codeBase && p < i->codeTop)
        printf("(c:%d-%x)", (int)(p - i->codeBase), (int)(p - i->codeBase));
    else if (p >= i->stringBase && p < i->stringTop)
        printf("(s:%d)", (int)(p - i->stringBase));
    else if ((VMVALUE *)p >= i->stack && (VMVALUE *)p < i->stackTop)
        printf("(%d)", (int)((VMVALUE *)p - i->stack));
    else if ((VMVALUE *)p == i->stackTop)
        printf("(top)");
}

static void ShowStack(Interpreter *i)
{
    VMVALUE *p;
    printf("sp %d, fp %d\n", (int)(i->sp - i->stack), (int)(i->fp - i->stack));
    printf("%d", i->tos);
    ShowOffset(i, i->tos);
    if (i->sp < i->stackTop) {
        for (p = i->sp; p < i->stackTop; ++p) {
            if (p == i->fp)
                printf(" <fp>");
            printf(" %d", *p);
            ShowOffset(i, *p);
        }
    }
    if (i->fp == i->stackTop)
        printf(" <fp>");
    printf("\n");
}
