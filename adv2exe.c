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

//#define DEBUG

/* interpreter state structure */
typedef struct {
    jmp_buf errorTarget;
    uint8_t *dataBase;
    uint8_t *codeBase;
    uint8_t *stringBase;
    VMVALUE *stack;
    VMVALUE *stackTop;
    uint8_t *pc;
    VMVALUE *fp;
    VMVALUE *sp;
    VMVALUE tos;
} Interpreter;

/* stack manipulation macros */
#define Reserve(i, n)   do {                                    \
                            if ((i)->sp - (n) < (i)->stack)     \
                                StackOverflow(i);               \
                            else                                \
                                (i)->sp -= (n);                 \
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
static void DoTrap(Interpreter *i, int op);
static void StackOverflow(Interpreter *i);
static void Abort(Interpreter *i, const char *fmt, ...);
#ifdef DEBUG
static void ShowStack(Interpreter *i);
#endif

/* Execute - execute the main code */
int Execute(uint8_t *main)
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
    i->stack = (VMVALUE *)((uint8_t *)i + sizeof(Interpreter));
    i->stackTop = (VMVALUE *)((uint8_t *)i->stack + MAXSTACK);

    /* initialize */    
    i->pc = main;
    i->sp = i->fp = i->stackTop;
    
    /* dummy arguments for testing */
    CPush(i, 30);
    CPush(i, 20);
    CPush(i, 10);

    if (setjmp(i->errorTarget))
        return VMFALSE;

    for (;;) {
#ifdef DEBUG
        ShowStack(i);
        DecodeInstruction(i->pc, i->pc);
#endif
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
            i->tos = tmp + i->tos * sizeof (VMVALUE);
            break;
        case OP_CALL:
            ++i->pc; // skip over the argument count
            tmp = i->tos;
            i->tos = (VMVALUE)i->pc;
            i->pc = (uint8_t *)tmp;
            break;
        case OP_FRAME:
            cnt = VMCODEBYTE(i->pc++);
            tmp = (VMVALUE)i->fp;
            i->fp = i->sp;
            Reserve(i, cnt);
            i->sp[0] = i->tos;
            i->sp[1] = tmp;
            break;
        case OP_RETURN:
            i->pc = (uint8_t *)Top(i);
            i->sp = i->fp;
            Drop(i, i->pc[-1]);
            i->fp = (VMVALUE *)i->fp[-1];
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
        case OP_PADDR:
            break;
        case OP_SEND:
            break;
        default:
            Abort(i, "undefined opcode 0x%02x", VMCODEBYTE(i->pc - 1));
            break;
        }
    }
}

#if 0

/* getp - get the value of an object property */
int getp(int obj,int prop)
{
    int p;

    for (; obj; obj = getofield(obj,O_CLASS))
        if ((p = findprop(obj,prop)) != 0)
            return (getofield(obj,p));
    return (NIL);
}

/* setp - set the value of an object property */
int setp(int obj,int prop,int val)
{
    int p;

    for (; obj; obj = getofield(obj,O_CLASS))
        if ((p = findprop(obj,prop)) != 0)
            return (putofield(obj,p,val));
    return (NIL);
}

/* findprop - find a property */
int findprop(int obj,int prop)
{
    int n,i,p;

    n = getofield(obj,O_NPROPERTIES);
    for (i = p = 0; i < n; i++, p += 4)
        if ((getofield(obj,O_PROPERTIES+p) & 0x7FFF) == prop)
            return (O_PROPERTIES+p+2);
    return (NIL);
}

static void opSEND(void)
{
    register int p2;
    *--sp = getboperand();
    *--sp = pc;
    *--sp = (int)(top - fp);
    fp = sp;
    p2 = ((p2 = fp[fp[2]+3]) ? getofield(p2,O_CLASS) : fp[fp[2]+2]);
    if (p2 && (p2 = getp(p2,fp[fp[2]+1])))
        pc = getafield(p2,A_CODE);
    else {
        *sp = NIL;
        opRETURN();
    }
}

#endif

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
        printf("%s", (char *)i->tos);
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

#ifdef DEBUG
static void ShowStack(Interpreter *i)
{
    VMVALUE *p;
    if (i->sp < i->stackTop) {
        printf(" %d", i->tos);
        for (p = i->sp; p < i->stackTop; ++p) {
            if (p == i->fp)
                printf(" <fp>");
            printf(" %d", *p);
        }
        printf("\n");
    }
}
#endif
