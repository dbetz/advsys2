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

/* prototypes for local functions */
static VMVALUE GetPropertyAddr(Interpreter *i, VMVALUE object, VMVALUE property);
static void DoSend(Interpreter *i);
static void DoTrap(Interpreter *i, int op);
static void StackOverflow(Interpreter *i);
static void Abort(Interpreter *i, const char *fmt, ...);
static void ShowStack(Interpreter *i);

/* Execute - execute the main code */
int Execute(ImageHdr *image, int debug)
{
    Interpreter *i;
    VMVALUE tmp;
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
    
    /* put the address of a HALT on the top of the stack */
    /* codeBase[0] is zero to act as the second byte of a fake CALL instruction */
    /* codeBase[1] is a HALT instruction */
    i->tos = (VMVALUE)i->codeBase + 1;
    
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
            i->tos = *(VMVALUE *)i->tos;
            break;
        case OP_LOADB:
            i->tos = *(uint8_t *)i->tos;
            break;
        case OP_STORE:
            tmp = Pop(i);
            *(VMVALUE *)tmp = i->tos;
            break;
        case OP_STOREB:
            tmp = Pop(i);
            *(uint8_t *)tmp = i->tos;
            break;
        case OP_LADDR:
            tmpb = (int8_t)VMCODEBYTE(i->pc++);
            CPush(i, i->tos);
            i->tos = (VMVALUE)&i->fp[(int)tmpb];
            break;
        case OP_INDEX:
            tmp = Pop(i);
            i->tos = (VMVALUE)(i->dataBase + tmp + i->tos * sizeof (VMVALUE));
            break;
        case OP_CALL:
            ++i->pc; // skip over the argument count
            tmp = i->tos;
            i->tos = (VMVALUE)i->pc;
            i->pc = i->codeBase + tmp;
            break;
        case OP_CATCH:
            CPush(i, i->tos);
            // fall through
        case OP_FRAME:
            cnt = VMCODEBYTE(i->pc++);
            tmp = (VMVALUE)i->fp;
            i->fp = i->sp;
            Reserve(i, cnt);
            *i->sp = tmp;
            break;
        case OP_RETURN:
            i->pc = (uint8_t *)i->sp[0];
            tmp = i->sp[1];
            i->sp = i->fp;
            Drop(i, i->pc[-1]);
            i->fp = (VMVALUE *)tmp;
            break;
        case OP_DROP:
            i->tos = Pop(i);
            break;
        case OP_DUP:
            CPush(i, i->tos);
            break;
        case OP_TUCK:
            CPush(i, i->tos);
            tmp = i->sp[0];
            i->sp[0] = i->sp[1];
            i->sp[1] = tmp;
            break;
        case OP_NATIVE:
            for (tmp = 0, cnt = sizeof(VMUVALUE); --cnt >= 0; )
                tmp = (tmp << 8) | VMCODEBYTE(i->pc++);
            break;
        case OP_TRAP:
            DoTrap(i, VMCODEBYTE(i->pc++));
            break;
        case OP_SEND:
            DoSend(i);
            break;
        case OP_DADDR:
            for (tmp = 0, cnt = sizeof(VMVALUE); --cnt >= 0; )
                tmp = (tmp << 8) | VMCODEBYTE(i->pc++);
            CPush(i, i->tos);
            i->tos = (VMVALUE)(i->dataBase + tmp);
            break;
        case OP_PADDR:
            i->tos = GetPropertyAddr(i, *i->sp, i->tos);
            Drop(i, 1);
            break;
        case OP_CLASS:
            i->tos = ((ObjectHdr *)(i->dataBase + i->tos))->class;
            break;
        case OP_TRY:
            for (tmpw = 0, cnt = sizeof(VMWORD); --cnt >= 0; )
                tmpw = (tmpw << 8) | VMCODEBYTE(i->pc++);
            Check(i, 3);
            Push(i, (VMVALUE)(i->pc + tmpw));
            Push(i, (VMVALUE)i->fp);
            Push(i, (VMVALUE)i->efp);
            i->efp = i->sp;
            break;
        case OP_CEXIT:
            break;
        case OP_THROW:
            break;
        default:
            Abort(i, "undefined opcode 0x%02x", VMCODEBYTE(i->pc - 1));
            break;
        }
    }
}

static VMVALUE GetPropertyAddr(Interpreter *i, VMVALUE object, VMVALUE tag)
{
    ObjectHdr *hdr = (ObjectHdr *)(i->dataBase + object);
    while (object) {
        Property *property = (Property *)(hdr + 1);
        int nProperties = hdr->nProperties;
        while (--nProperties >= 0) {
            if ((property->tag & ~P_SHARED) == tag)
                return (VMVALUE)&property->value;
            ++property;
        }
        object = hdr->class;
        hdr = (ObjectHdr *)(i->dataBase + object);
    }
    return NIL;
}

static void DoSend(Interpreter *i)
{
    VMVALUE tmp, p;
    ++i->pc; // skip over the argument count
    tmp = i->tos;
    i->tos = (VMVALUE)i->pc;
    if (!(p = i->sp[1]))
        p = i->sp[0];
    p = GetPropertyAddr(i, p, tmp);
    i->pc = i->codeBase + *(VMVALUE *)p;
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
        printf("%s", (char *)(i->stringBase + i->tos));
        i->tos = *i->sp++;
        break;
    case TRAP_PrintInt:
        printf("%d", i->tos);
        i->tos = *i->sp++;
        break;
    case TRAP_PrintTab:
        putchar('\t');
        break;
    case TRAP_PrintNL:
        putchar('\n');
        break;
    case TRAP_PrintFlush:
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
    uint8_t *p = (uint8_t *)value;
    if (p >= i->dataBase && p < i->dataTop)
        printf("(d:%d)", p - i->dataBase);
    else if (p >= i->codeBase && p < i->codeTop)
        printf("(c:%d-%x)", p - i->codeBase, p - i->codeBase);
    else if (p >= i->stringBase && p < i->stringTop)
        printf("(s:%d)", p - i->stringBase);
    else if ((VMVALUE *)p >= i->stack && (VMVALUE *)p < i->stackTop)
        printf("(%d)", (VMVALUE *)p - i->stack);
    else if ((VMVALUE *)p == i->stackTop)
        printf("(top)");
}

static void ShowStack(Interpreter *i)
{
    VMVALUE *p;
    printf("sp %d, fp %d\n", i->sp - i->stack, i->fp - i->stack);
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
