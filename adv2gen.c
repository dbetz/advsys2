/* adv2gen.c - code generation functions
 *
 * Copyright (c) 2018 by David Michael Betz.  All rights reserved.
 *
 */

#include <string.h>
#include "adv2compiler.h"

/* local function prototypes */
static void code_statement(ParseContext *c, ParseTreeNode *expr);
static void code_expr(ParseContext *c, ParseTreeNode *expr, PVAL *pv);
static void code_if(ParseContext *c, ParseTreeNode *expr);
static void code_while(ParseContext *c, ParseTreeNode *expr);
static void code_dowhile(ParseContext *c, ParseTreeNode *expr);
static void code_for(ParseContext *c, ParseTreeNode *expr);
static void code_return(ParseContext *c, ParseTreeNode *expr);
static void code_breakorcontinue(ParseContext *c, ParseTreeNode *expr, int isBreak);
static void code_block(ParseContext *c, ParseTreeNode *expr);
static void code_exprstatement(ParseContext *c, ParseTreeNode *expr);
static void code_print(ParseContext *c, ParseTreeNode *expr);
static void code_symbolref(ParseContext *c, ParseTreeNode *expr, PVAL *pv);
static void code_shortcircuit(ParseContext *c, int op, ParseTreeNode *expr, PVAL *pv);
static void code_arrayref(ParseContext *c, ParseTreeNode *expr, PVAL *pv);
static void code_call(ParseContext *c, ParseTreeNode *expr, PVAL *pv);
static void code_send(ParseContext *c, ParseTreeNode *expr, PVAL *pv);
static void code_propertyref(ParseContext *c, ParseTreeNode *expr, PVAL *pv);
static void code_lvalue(ParseContext *c, ParseTreeNode *expr, PVAL *pv);
static void code_rvalue(ParseContext *c, ParseTreeNode *expr);
static void rvalue(ParseContext *c, PVAL *pv);
static void chklvalue(ParseContext *c, PVAL *pv);
static void PushBlock(ParseContext *c, Block *block, BlockType type);
static void PopBlock(ParseContext *c);

static int codeaddr(ParseContext *c);
static int putcword(ParseContext *c, VMWORD v);
static int putclong(ParseContext *c, VMVALUE v);
static void fixupbranch(ParseContext *c, VMUVALUE chn, VMUVALUE val);

/* code_functiondef - generate code for a function definition */
uint8_t *code_functiondef(ParseContext *c, ParseTreeNode *expr, int *pLength)
{
    c->codeBase = c->codeFree;
    putcbyte(c, OP_FRAME);
    putcbyte(c, expr->u.functionDef.locals.count + 1);
    code_statement(c, expr->u.functionDef.body);
    putcbyte(c, OP_RETURN);
    *pLength = c->codeFree - c->codeBase;
    return c->codeBase;
}

/* code_lvalue - generate code for an l-value expression */
static void code_lvalue(ParseContext *c, ParseTreeNode *expr, PVAL *pv)
{
    code_expr(c, expr, pv);
    chklvalue(c, pv);
}

/* code_rvalue - generate code for an r-value expression */
static void code_rvalue(ParseContext *c, ParseTreeNode *expr)
{
    PVAL pv;
    code_expr(c, expr, &pv);
    rvalue(c, &pv);
}

/* code_statement - generate code for a statement parse tree */
static void code_statement(ParseContext *c, ParseTreeNode *expr)
{
    switch (expr->nodeType) {
    case NodeTypeIf:
        code_if(c, expr);
        break;
    case NodeTypeWhile:
        code_while(c, expr);
        break;
    case NodeTypeDoWhile:
        code_dowhile(c, expr);
        break;
    case NodeTypeFor:
        code_for(c, expr);
        break;
    case NodeTypeReturn:
        code_return(c, expr);
        break;
    case NodeTypeBreak:
        code_breakorcontinue(c, expr, VMTRUE);
        break;
    case NodeTypeContinue:
        code_breakorcontinue(c, expr, VMFALSE);
        break;
    case NodeTypeBlock:
        code_block(c, expr);
        break;
    case NodeTypeExpr:
        code_exprstatement(c, expr);
        break;
    case NodeTypeEmpty:
        /* nothing to generate */
        break;
    case NodeTypePrint:
        code_print(c, expr);
        break;
    }
}

/* code_if - generate code for an 'if' statement */
static void code_if(ParseContext *c, ParseTreeNode *expr)
{
    int nxt, end;
    code_rvalue(c, expr->u.ifStatement.test);
    putcbyte(c, OP_BRF);
    nxt = putcword(c, 0);
    end = 0;
    code_statement(c, expr->u.ifStatement.thenStatement);
    if (expr->u.ifStatement.elseStatement) {
        putcbyte(c, OP_BR);
        end = putcword(c, end);
        fixupbranch(c, nxt, codeaddr(c));
        code_statement(c, expr->u.ifStatement.elseStatement);
        fixupbranch(c, end, codeaddr(c));
    }
    else {
        fixupbranch(c, nxt, codeaddr(c));
        fixupbranch(c, end, codeaddr(c));
    }
}


/* code_while - generate code for an 'while' statement */
static void code_while(ParseContext *c, ParseTreeNode *expr)
{
    Block block;
    int inst;
    PushBlock(c, &block, BLOCK_WHILE);
    block.cont = block.nxt = codeaddr(c);
    block.contDefined = VMTRUE;
    code_statement(c, expr->u.whileStatement.test);
    putcbyte(c, OP_BRF);
    block.end = putcword(c, 0);
    code_statement(c, expr->u.whileStatement.body);
    inst = putcbyte(c, OP_BR);
    putcword(c, block.nxt - inst - 1 - sizeof(VMWORD));
    fixupbranch(c, block.end, codeaddr(c));
    PopBlock(c);
}

/* code_dowhile - generate code for an 'do/while' statement */
static void code_dowhile(ParseContext *c, ParseTreeNode *expr)
{
    Block block;
    int inst;
    PushBlock(c, &block, BLOCK_DO);
    block.cont = 0;
    block.contDefined = VMFALSE;
    block.nxt = codeaddr(c);
    block.end = 0;
    code_statement(c, expr->u.doWhileStatement.body);
    fixupbranch(c, block.cont, codeaddr(c));
    code_statement(c, expr->u.doWhileStatement.test);
    inst = putcbyte(c, OP_BRT);
    putcword(c, block.nxt - inst - 1 - sizeof(VMWORD));
    fixupbranch(c, block.end, codeaddr(c));
    PopBlock(c);
}

/* code_for - generate code for an 'for' statement */
static void code_for(ParseContext *c, ParseTreeNode *expr)
{
    Block block;
    int inst;
    PushBlock(c, &block, BLOCK_FOR);
    if (expr->u.forStatement.init) {
        code_rvalue(c, expr->u.forStatement.init);
        putcbyte(c, OP_DROP);
    }
    block.nxt = codeaddr(c);
    if (expr->u.forStatement.incr)
        block.contDefined = VMFALSE;
    else {
        block.cont = codeaddr(c);
        block.contDefined = VMTRUE;
    }
    block.end = 0;
    if (expr->u.forStatement.test) {
        code_rvalue(c, expr->u.forStatement.test);
        putcbyte(c, OP_BRF);
        block.end = putcword(c, 0);
    }
    code_statement(c, expr->u.forStatement.body);
    if (expr->u.forStatement.incr) {
        fixupbranch(c, block.cont, codeaddr(c));
        code_rvalue(c, expr->u.forStatement.incr);
        putcbyte(c, OP_DROP);
    }
    inst = putcbyte(c, OP_BR);
    putcword(c, block.nxt - inst - 1 - sizeof(VMWORD));
    fixupbranch(c, block.end, codeaddr(c));
    PopBlock(c);
}

/* code_return - generate code for an 'return' statement */
static void code_return(ParseContext *c, ParseTreeNode *expr)
{
    if (expr->u.returnStatement.value)
        code_rvalue(c, expr->u.returnStatement.value);
    else {
        putcbyte(c, OP_SLIT);
        putcbyte(c, 0);
    }
    putcbyte(c, OP_RETURN);
}

/* code_breakorcontinue - generate code for an 'break' statement */
static void code_breakorcontinue(ParseContext *c, ParseTreeNode *expr, int isBreak)
{
    Block *block;
    int inst;
    for (block = c->block; block != NULL; block = block->next) {
        switch (block->type) {
        case BLOCK_FOR:
        case BLOCK_WHILE:
        case BLOCK_DO:
            inst = putcbyte(c, OP_BR);
            if (isBreak)
                block->end = putcword(c, block->end);
            else {
                if (block->contDefined)
                    putcword(c, block->cont - inst - 1 - sizeof(VMWORD));
                else
                    block->cont = putcword(c, block->cont);
            }
            return;
        default:
            break;
        }
    }
}

/* code_block - generate code for an {} block statement */
static void code_block(ParseContext *c, ParseTreeNode *expr)
{
    NodeListEntry *entry = expr->u.blockStatement.statements;
    while (entry) {
        code_statement(c, entry->node);
        entry = entry->next;
    }
}

/* code_exprstatement - generate code for an expression statement */
static void code_exprstatement(ParseContext *c, ParseTreeNode *expr)
{
    code_rvalue(c, expr->u.exprStatement.expr);
    putcbyte(c, OP_DROP);
}

/* CallHandler - compile a call to a runtime print function */
static void CallHandler(ParseContext *c, int trap, ParseTreeNode *expr)
{
    /* compile the argument */
    if (expr)
        code_rvalue(c, expr);
    
    /* compile the function symbol reference */
    putcbyte(c, OP_TRAP);
    putcbyte(c, trap);
}

/* code_print - code a print statement */
static void code_print(ParseContext *c, ParseTreeNode *expr)
{
    PrintOp *op = expr->u.printStatement.ops;
    while (op) {
        CallHandler(c, op->trap, op->expr);
        op = op->next;
    }
}

/* code_expr - generate code for an expression parse tree */
static void code_expr(ParseContext *c, ParseTreeNode *expr, PVAL *pv)
{
    VMVALUE ival;
    PVAL pv2;
    
    switch (expr->nodeType) {
    case NodeTypeGlobalSymbolRef:
        code_symbolref(c, expr, pv);
        break;
    case NodeTypeLocalSymbolRef:
        putcbyte(c, OP_LADDR);
        putcbyte(c, -expr->u.localSymbolRef.symbol->offset - 1);
        *pv = VT_LVALUE;
        break;
    case NodeTypeArgumentRef:
        putcbyte(c, OP_LADDR);
        putcbyte(c, expr->u.localSymbolRef.symbol->offset);
        *pv = VT_LVALUE;
        break;
    case NodeTypeStringLit:
        putcbyte(c, OP_SADDR);
        putclong(c, expr->u.stringLit.string->offset);
        *pv = VT_RVALUE;
        break;
    case NodeTypeIntegerLit:
        ival = expr->u.integerLit.value;
        if (ival >= -128 && ival <= 127) {
            putcbyte(c, OP_SLIT);
            putcbyte(c, ival);
        }
        else {
            putcbyte(c, OP_LIT);
            putclong(c, ival);
        }
        *pv = VT_RVALUE;
        break;
    case NodeTypeFunctionLit:
        putcbyte(c, OP_LIT);
        putclong(c, expr->u.functionLit.offset);
        *pv = VT_RVALUE;
        break;
    case NodeTypePreincrementOp:
        code_lvalue(c, expr->u.incrementOp.expr, &pv2);
        putcbyte(c, OP_DUP);
        putcbyte(c, OP_LOAD);
        putcbyte(c, OP_SLIT);
        putcbyte(c, expr->u.incrementOp.increment);
        putcbyte(c, OP_ADD);
        putcbyte(c, OP_STORE);
        *pv = VT_RVALUE;
        break;
    case NodeTypePostincrementOp:
        code_lvalue(c, expr->u.incrementOp.expr, &pv2);
        putcbyte(c, OP_DUP);
        putcbyte(c, OP_LOAD);
        putcbyte(c, OP_TUCK);
        putcbyte(c, OP_SLIT);
        putcbyte(c, expr->u.incrementOp.increment);
        putcbyte(c, OP_ADD);
        putcbyte(c, OP_STORE);
        putcbyte(c, OP_DROP);
        *pv = VT_RVALUE;
        break;
    case NodeTypeUnaryOp:
        code_rvalue(c, expr->u.unaryOp.expr);
        putcbyte(c, expr->u.unaryOp.op);
        *pv = VT_RVALUE;
        break;
    case NodeTypeBinaryOp:
        code_rvalue(c, expr->u.binaryOp.left);
        code_rvalue(c, expr->u.binaryOp.right);
        putcbyte(c, expr->u.binaryOp.op);
        *pv = VT_RVALUE;
        break;
    case NodeTypeAssignmentOp:
        if (expr->u.binaryOp.op == OP_EQ) {
            code_lvalue(c, expr->u.binaryOp.left, &pv2);
            code_rvalue(c, expr->u.binaryOp.right);
            putcbyte(c, OP_STORE);
        }
        else {
            code_lvalue(c, expr->u.binaryOp.left, &pv2);
            putcbyte(c, OP_DUP);
            putcbyte(c, OP_LOAD);
            code_rvalue(c, expr->u.binaryOp.right);
            putcbyte(c, expr->u.binaryOp.op);
            putcbyte(c, OP_STORE);
        }
        *pv = VT_RVALUE;
        break;
    case NodeTypeArrayRef:
        code_arrayref(c, expr, pv);
        break;
    case NodeTypeFunctionCall:
        code_call(c, expr, pv);
        break;
    case NodeTypeSend:
        code_send(c, expr, pv);
        break;
    case NodeTypePropertyRef:
        code_propertyref(c, expr, pv);
        break;
    case NodeTypeDisjunction:
        code_shortcircuit(c, OP_BRTSC, expr, pv);
        break;
    case NodeTypeConjunction:
        code_shortcircuit(c, OP_BRFSC, expr, pv);
        break;
    }
}

/* code_symbolref - generate code for a symbol reference */
static void code_symbolref(ParseContext *c, ParseTreeNode *expr, PVAL *pv)
{
    Symbol *symbol = expr->u.symbolRef.symbol;
    switch (symbol->storageClass) {
    case SC_VARIABLE:
        putcbyte(c, OP_DADDR);
        putclong(c, AddSymbolRef(c, symbol, FT_CODE, codeaddr(c)));
        *pv = VT_LVALUE;
        break;
    case SC_OBJECT:
        putcbyte(c, OP_DADDR);
        putclong(c, AddSymbolRef(c, symbol, FT_CODE, codeaddr(c)));
        *pv = VT_RVALUE;
        break;
    case SC_FUNCTION:
        putcbyte(c, OP_CADDR);
        putclong(c, AddSymbolRef(c, symbol, FT_CODE, codeaddr(c)));
        *pv = VT_RVALUE;
        break;
    default:
        break;
    }
}

/* code_shortcircuit - generate code for a conjunction or disjunction of boolean expressions */
static void code_shortcircuit(ParseContext *c, int op, ParseTreeNode *expr, PVAL *pv)
{
    NodeListEntry *entry = expr->u.exprList.exprs;
    int end = 0;

    code_rvalue(c, entry->node);
    entry = entry->next;

    do {
        putcbyte(c, op);
        end = putcword(c, end);
        code_rvalue(c, entry->node);
    } while ((entry = entry->next) != NULL);

    fixupbranch(c, end, codeaddr(c));

    *pv = VT_RVALUE;
}

/* code_arrayref - code an array reference */
static void code_arrayref(ParseContext *c, ParseTreeNode *expr, PVAL *pv)
{
    /* code the array reference */
    code_rvalue(c, expr->u.arrayRef.array);

    /* code the index */
    code_rvalue(c, expr->u.arrayRef.index);

    /* code the index operation */
    putcbyte(c, OP_INDEX);

    *pv = VT_LVALUE;
}

/* code_arguments - code function arguments (in reverse order) */
static void code_arguments(ParseContext *c, NodeListEntry *args)
{
    if (args->next)
        code_arguments(c, args->next);
    code_rvalue(c, args->node);
}

/* code_call - code a function call */
static void code_call(ParseContext *c, ParseTreeNode *expr, PVAL *pv)
{
    /* code each argument expression */
    code_arguments(c, expr->u.functionCall.args);

    /* call the function */
    code_rvalue(c, expr->u.functionCall.fcn);
    putcbyte(c, OP_CALL);
    putcbyte(c, expr->u.functionCall.argc);

    /* we've got an rvalue now */
    *pv = VT_RVALUE;
}

/* code_send - code a message send */
static void code_send(ParseContext *c, ParseTreeNode *expr, PVAL *pv)
{
    /* code each argument expression */
    code_arguments(c, expr->u.send.args);

    /* code the object from which to start searching for the method */
    if (expr->u.send.class) {
        code_rvalue(c, expr->u.send.class);
        putcbyte(c, OP_CLASS);
    }
    else {
        putcbyte(c, OP_SLIT);
        putcbyte(c, NIL);
    }
    
    /* code the object receiving the message */
    code_rvalue(c, expr->u.send.object);
    
    /* code the selector */
    code_rvalue(c, expr->u.send.selector);

    /* code the send operation */
    putcbyte(c, OP_SEND);
    putcbyte(c, expr->u.send.argc + 2);

    /* we've got an rvalue now */
    *pv = VT_RVALUE;
}

/* code_propertyref - code a property reference */
static void code_propertyref(ParseContext *c, ParseTreeNode *expr, PVAL *pv)
{
    code_rvalue(c, expr->u.propertyRef.object);
    code_rvalue(c, expr->u.propertyRef.property);
    putcbyte(c, OP_PADDR);
    *pv = VT_LVALUE;
}

/* rvalue - get the rvalue of a partial expression */
static void rvalue(ParseContext *c, PVAL *pv)
{
    if (*pv == VT_LVALUE) {
        putcbyte(c, OP_LOAD);
        *pv = VT_RVALUE;
    }
}

/* chklvalue - make sure we've got an lvalue */
static void chklvalue(ParseContext *c, PVAL *pv)
{
    if (*pv == VT_RVALUE)
        ParseError(c,"expecting an lvalue");
}

/* PushBlock - push a block on the block stack */
static void PushBlock(ParseContext *c, Block *block, BlockType type)
{
    memset(block, 0, sizeof(Block));
    block->type = type;
    block->next = c->block;
    c->block = block;
}

/* PopBlock - pop a block off the block stack */
static void PopBlock(ParseContext *c)
{
    c->block = c->block->next;
}

static VMWORD rd_cword(ParseContext *c, VMUVALUE off);
static void wr_cword(ParseContext *c, VMUVALUE off, VMWORD v);
void wr_clong(ParseContext *c, VMUVALUE off, VMVALUE v);

/* codeaddr - get the current code address (actually, offset) */
static int codeaddr(ParseContext *c)
{
    return (int)(c->codeFree - c->codeBuf);
}

/* putcbyte - put a code byte into the code buffer */
int putcbyte(ParseContext *c, int b)
{
    int addr = codeaddr(c);
    if (c->codeFree >= c->codeTop)
        Abort(c, "insufficient memory");
    *c->codeFree++ = b;
    return addr;
}

/* putcword - put a code word into the code buffer */
static int putcword(ParseContext *c, VMWORD v)
{
    int addr = codeaddr(c);
    if (c->codeFree + sizeof(VMWORD) > c->codeTop)
        Abort(c, "insufficient memory");
    wr_cword(c, addr, v);
    c->codeFree += sizeof(VMWORD);
    return addr;
}

/* putclong - put a code word into the code buffer */
static int putclong(ParseContext *c, VMVALUE v)
{
    int addr = codeaddr(c);
    if (c->codeFree + sizeof(VMVALUE) > c->codeTop)
        Abort(c, "insufficient memory");
    wr_clong(c, addr, v);
    c->codeFree += sizeof(VMVALUE);
    return addr;
}

/* fixupbranch - fixup a branch reference chain */
static void fixupbranch(ParseContext *c, VMUVALUE chn, VMUVALUE val)
{
    while (chn != 0) {
        int nxt = rd_cword(c, chn);
        VMWORD off = val - (chn + sizeof(VMWORD)); /* this assumes all 1+sizeof(VMWORD) byte branch instructions */
        wr_cword(c, chn, off);
        chn = nxt;
    }
}

/* rd_cword - get a code word from the code buffer */
static VMWORD rd_cword(ParseContext *c, VMUVALUE off)
{
    int cnt = sizeof(VMWORD);
    VMWORD v = 0;
    while (--cnt >= 0)
        v = (v << 8) | c->codeBuf[off++];
    return v;
}

/* wr_cword - put a code word into the code buffer */
static void wr_cword(ParseContext *c, VMUVALUE off, VMWORD v)
{
    uint8_t *p = &c->codeBuf[off] + sizeof(VMWORD);
    int cnt = sizeof(VMWORD);
    while (--cnt >= 0) {
        *--p = v;
        v >>= 8;
    }
}

/* wr_clong - put a code word into the code buffer */
void wr_clong(ParseContext *c, VMUVALUE off, VMVALUE v)
{
    uint8_t *p = &c->codeBuf[off] + sizeof(VMVALUE);
    int cnt = sizeof(VMVALUE);
    while (--cnt >= 0) {
        *--p = v;
        v >>= 8;
    }
}
