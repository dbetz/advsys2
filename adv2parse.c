/* adv2parse.c - parser
 *
 * Copyright (c) 2018 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdlib.h>
#include <string.h>
#include "adv2compiler.h"
#include "adv2debug.h"
#include "adv2vm.h"
#include "adv2vmdebug.h"

/* local function prototypes */
static void ParseDef(ParseContext *c);
static void ParseConstantDef(ParseContext *c, char *name);
static void  ParseFunctionDef(ParseContext *c, char *name);
static void ParseVar(ParseContext *c);
static void ParseObject(ParseContext *c, char *name);
static void ParseProperty(ParseContext *c);
static ParseTreeNode *ParseFunction(ParseContext *c);
static ParseTreeNode *ParseMethod(ParseContext *c);
static ParseTreeNode *ParseFunctionBody(ParseContext *c, ParseTreeNode *node, int offset);
static ParseTreeNode *ParseIf(ParseContext *c);
static ParseTreeNode *ParseWhile(ParseContext *c);
static ParseTreeNode *ParseDoWhile(ParseContext *c);
static ParseTreeNode *ParseFor(ParseContext *c);
static ParseTreeNode *ParseBreak(ParseContext *c);
static ParseTreeNode *ParseContinue(ParseContext *c);
static ParseTreeNode *ParseReturn(ParseContext *c);
static ParseTreeNode *ParseBlock(ParseContext *c);
static ParseTreeNode *ParseTry(ParseContext *c);
static ParseTreeNode *ParseThrow(ParseContext *c);
static ParseTreeNode *ParseExprStatement(ParseContext *c);
static ParseTreeNode *ParseEmpty(ParseContext *c);
static ParseTreeNode *ParsePrint(ParseContext *c);
static VMVALUE ParseIntegerLiteralExpr(ParseContext *c);
static VMVALUE ParseConstantLiteralExpr(ParseContext *c, VMVALUE offset);
static ParseTreeNode *ParseExpr(ParseContext *c);
static ParseTreeNode *ParseExpr1(ParseContext *c);
static ParseTreeNode *ParseExpr2(ParseContext *c);
static ParseTreeNode *ParseExpr3(ParseContext *c);
static ParseTreeNode *ParseExpr4(ParseContext *c);
static ParseTreeNode *ParseExpr5(ParseContext *c);
static ParseTreeNode *ParseExpr6(ParseContext *c);
static ParseTreeNode *ParseExpr7(ParseContext *c);
static ParseTreeNode *ParseExpr8(ParseContext *c);
static ParseTreeNode *ParseExpr9(ParseContext *c);
static ParseTreeNode *ParseExpr10(ParseContext *c);
static ParseTreeNode *ParseExpr11(ParseContext *c);
static ParseTreeNode *ParsePrimary(ParseContext *c);
static ParseTreeNode *GetSymbolRef(ParseContext *c, char *name);
static ParseTreeNode *ParseSimplePrimary(ParseContext *c);
static ParseTreeNode *ParseArrayReference(ParseContext *c, ParseTreeNode *arrayNode);
static ParseTreeNode *ParseCall(ParseContext *c, ParseTreeNode *functionNode);
static ParseTreeNode *ParseSend(ParseContext *c);
static ParseTreeNode *ParsePropertyRef(ParseContext *c, ParseTreeNode *object);
static ParseTreeNode *MakeUnaryOpNode(ParseContext *c, int op, ParseTreeNode *expr);
static ParseTreeNode *MakeBinaryOpNode(ParseContext *c, int op, ParseTreeNode *left, ParseTreeNode *right);
static ParseTreeNode *MakeAssignmentOpNode(ParseContext *c, int op, ParseTreeNode *left, ParseTreeNode *right);
static ParseTreeNode *NewParseTreeNode(ParseContext *c, int type);
static void InitLocalSymbolTable(LocalSymbolTable *table);
static LocalSymbol *AddLocalSymbol(ParseContext *c, LocalSymbolTable *table, const char *name, int offset);
static LocalSymbol *MakeLocalSymbol(ParseContext *c, const char *name, int offset);
static LocalSymbol *FindLocalSymbol(LocalSymbolTable *table, const char *name);
static void AddNodeToList(ParseContext *c, NodeListEntry ***ppNextEntry, ParseTreeNode *node);
static int IsIntegerLit(ParseTreeNode *node, VMVALUE *pValue);

/* ParseDeclarations - parse variable, object, and function declarations */
void ParseDeclarations(ParseContext *c)
{
    char name[MAXTOKEN];
    int tkn;
    
    /* parse declarations */
    while ((tkn = GetToken(c)) != T_EOF) {
        switch (tkn) {
        case T_DEF:
            ParseDef(c);
            break;
        case T_VAR:
            ParseVar(c);
            break;
        case T_OBJECT:
            ParseObject(c, NULL);
            break;
        case T_IDENTIFIER:
            strcpy(name, c->token);
            ParseObject(c, name);
            break;
        case T_PROPERTY:
            ParseProperty(c);
            break;
        case T_EOF:
            return;
        default:
            ParseError(c, "unknown declaration");
            break;
        }
    }
}

/* ParseDef - parse the 'def' statement */
static void ParseDef(ParseContext *c)
{
    int complete = VMTRUE;
    char name[MAXTOKEN];
    int tkn;

    /* get the name being defined */
    FRequire(c, T_IDENTIFIER);
    strcpy(name, c->token);

    /* check for a constant definition */
    if ((tkn = GetToken(c)) == '=')
        ParseConstantDef(c, name);

    /* otherwise, assume a function definition */
    else {
        SaveToken(c, tkn);
        ParseFunctionDef(c, name);
        complete = VMFALSE;
    }
}

/* ParseConstantDef - parse a 'def <name> =' statement */
static void ParseConstantDef(ParseContext *c, char *name)
{
    AddGlobal(c, name, SC_CONSTANT, ParseIntegerLiteralExpr(c));
    FRequire(c, ';');
}

/* ParseFunctionDef - parse a 'def <name> () {}' statement */
static void ParseFunctionDef(ParseContext *c, char *name)
{
    ParseTreeNode *node;
    uint8_t *code;
    int codeLength;
    
    /* enter the function name in the global symbol table */
    AddGlobal(c, name, SC_FUNCTION, (VMVALUE)(c->codeFree - c->codeBuf));
    
    node = ParseFunction(c);
    PrintNode(c, node, 0);
    
    code = code_functiondef(c, node, &codeLength);
    
    DecodeFunction(c->codeBuf, code, codeLength);
}

/* ParseVar - parse the 'var' statement */
static void ParseVar(ParseContext *c)
{
    int tkn;
    do {
        FRequire(c, T_IDENTIFIER);
        if (c->dataFree + sizeof(VMVALUE) > c->dataTop)
            ParseError(c, "insufficient data space");
        AddGlobal(c, c->token, SC_VARIABLE, (VMVALUE)(c->dataFree - c->dataBuf));
        if ((tkn = GetToken(c)) == '=') {
            VMVALUE offset = c->dataFree - c->dataBuf;
            *(VMVALUE *)c->dataFree = ParseConstantLiteralExpr(c, offset);
        }
        else {
            SaveToken(c, tkn);
        }
        c->dataFree += sizeof(VMVALUE);
    } while ((tkn = GetToken(c)) == ',');
    Require(c, tkn, ';');
}

/* ParseObject - parse the 'object' statement */
static void ParseObject(ParseContext *c, char *className)
{
    VMVALUE class;
    char name[MAXTOKEN];
    ParseTreeNode *node;
    ObjectHdr *objectHdr;
    Property *property, *p;
    int tkn;
    
    /* get the name of the object being defined */
    FRequire(c, T_IDENTIFIER);
    strcpy(name, c->token);
    
    /* allocate space for an object header and initialize */
    if (c->dataFree + sizeof(ObjectHdr) > c->dataTop)
        ParseError(c, "insufficient data space");
    c->currentObjectSymbol = AddGlobal(c, name, SC_OBJECT, (VMVALUE)(c->dataFree - c->dataBuf));
    objectHdr = (ObjectHdr *)c->dataFree;
    c->dataFree += sizeof(ObjectHdr);
    objectHdr->nProperties = 0;
    property = (Property *)(objectHdr + 1);
        
    /* copy non-shared properties from the class object */
    if (className) {
        ObjectHdr *classHdr;
        Property *srcProperty;
        VMVALUE nProperties;
        class = FindObject(c, className);
        objectHdr->class = class;
        classHdr = (ObjectHdr *)(c->dataBuf + class);
        srcProperty = (Property *)(classHdr + 1);
        for (nProperties = classHdr->nProperties; --nProperties >= 0; ++srcProperty) {
            if (!(srcProperty->tag & P_SHARED)) {
                if ((uint8_t *)property + sizeof(Property) > c->dataTop)
                    ParseError(c, "insufficient data space");
                *property++ = *srcProperty;
                ++objectHdr->nProperties;
            }
        }
    }
    else {
        objectHdr->class = NIL;
    }
    
    /* parse object properties */
    FRequire(c, '{');
    while ((tkn = GetToken(c)) != '}') {
        VMVALUE tag, flags = 0;
        if (tkn == T_SHARED) {
            flags = P_SHARED;
            tkn = GetToken(c);
        }
        Require(c, tkn, T_IDENTIFIER);
        tag = AddProperty(c, c->token);
        FRequire(c, ':');
        
        /* find a property copied from the class */
        for (p = (Property *)(objectHdr + 1); p < property; ++p) {
            if ((p->tag & ~P_SHARED) == tag) {
                if (p->tag & P_SHARED)
                    ParseError(c, "can't set shared property in object definition");
                break;
            }
        }
        
        /* add a new property if one wasn't found that was copied from the class */
        if (p >= property) {
            if ((uint8_t *)property + sizeof(Property) > c->dataTop)
                ParseError(c, "insufficient data space");
            p = property;
            p->tag = tag | flags;
            ++objectHdr->nProperties;
            ++property;
        }
        
        /* handle methods */
        if ((tkn = GetToken(c)) == T_METHOD) {
            uint8_t *code;
            int codeLength;
            node = ParseMethod(c);
            PrintNode(c, node, 0);
            code = code_functiondef(c, node, &codeLength);
            DecodeFunction(c->codeBuf, code, codeLength);
            p->value = (VMVALUE)(code - c->codeBuf);
        }
        
        /* handle values */
        else {
            VMVALUE offset = (uint8_t *)&p->value - c->dataBuf;
            SaveToken(c, tkn);
            p->value = ParseConstantLiteralExpr(c, offset);
        }

        FRequire(c, ';');
    }
    
    /* move the free pointer past the new object */
    c->dataFree = (uint8_t *)property;
    
    /* not in an object definition anymore */
    c->currentObjectSymbol = NULL;
}

/* ParseProperty - parse the 'property' statement */
static void ParseProperty(ParseContext *c)
{
    int tkn;
    do {
        FRequire(c, T_IDENTIFIER);
        AddProperty(c, c->token);
    } while ((tkn = GetToken(c)) == ',');
    Require(c, tkn, ';');
}

/* ParseFunction - parse a function definition */
static ParseTreeNode *ParseFunction(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeFunctionDef);
    
    c->currentFunction = node;
    InitLocalSymbolTable(&node->u.functionDef.arguments);
    InitLocalSymbolTable(&node->u.functionDef.locals);
    c->trySymbols = NULL;
    c->currentTryDepth = 0;
    c->block = NULL;
    
    return ParseFunctionBody(c, node, 0);
}

/* ParseMethod - parse a method definition */
static ParseTreeNode *ParseMethod(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeFunctionDef);
    
    c->currentFunction = node;
    InitLocalSymbolTable(&node->u.functionDef.arguments);
    InitLocalSymbolTable(&node->u.functionDef.locals);
    c->trySymbols = NULL;
    c->currentTryDepth = 0;
    c->block = NULL;
    
    AddLocalSymbol(c, &node->u.functionDef.arguments, "self", 0);
    AddLocalSymbol(c, &node->u.functionDef.arguments, "(dummy)", 1);
    
    return ParseFunctionBody(c, node, 2);
}

/* ParseFunctionBody - parse a function argument list and body */
static ParseTreeNode *ParseFunctionBody(ParseContext *c, ParseTreeNode *node, int offset)
{
    int localOffset = 0;
    int tkn;

    /* parse the argument list */
    FRequire(c, '(');
    if ((tkn = GetToken(c)) != ')') {
        SaveToken(c, tkn);
        do {
            FRequire(c, T_IDENTIFIER);
            AddLocalSymbol(c, &node->u.functionDef.arguments, c->token, offset++);
        } while ((tkn = GetToken(c)) == ',');
    }
    Require(c, tkn, ')');
    FRequire(c, '{');
    
    while ((tkn = GetToken(c)) == T_VAR) {
        do {
            LocalSymbol *symbol;
            FRequire(c, T_IDENTIFIER);
            symbol = AddLocalSymbol(c, &node->u.functionDef.locals, c->token, localOffset++);
            if ((tkn = GetToken(c)) == '=') {
                symbol->initialValue = ParseExpr(c);
            }
            else {
                SaveToken(c, tkn);
            }
        } while ((tkn = GetToken(c)) == ',');
        Require(c, tkn, ';');
    }
    SaveToken(c, tkn);

    /* parse the function body */
    node->u.functionDef.body = ParseBlock(c);
    
    /* not compiling a function anymore */
    c->currentFunction = NULL;
    
    return node;
}

/* ParseStatement - parse a statement */
ParseTreeNode *ParseStatement(ParseContext *c)
{
    ParseTreeNode *node;
    int tkn;
    
    /* dispatch on the statement keyword */
    switch (tkn = GetToken(c)) {
    case T_IF:
        node = ParseIf(c);
        break;
    case T_WHILE:
        node = ParseWhile(c);
        break;
    case T_DO:
        node = ParseDoWhile(c);
        break;
    case T_FOR:
        node = ParseFor(c);
        break;
    case T_BREAK:
        node = ParseBreak(c);
        break;
    case T_CONTINUE:
        node = ParseContinue(c);
        break;
    case T_RETURN:
        node = ParseReturn(c);
        break;
    case T_TRY:
        node = ParseTry(c);
        break;
    case T_THROW:
        node = ParseThrow(c);
        break;
    case T_PRINT:
        node = ParsePrint(c);
        break;
    case '{':
        node = ParseBlock(c);
        break;
    case ';':
        node = ParseEmpty(c);
        break;
    default:
        SaveToken(c, tkn);
        node = ParseExprStatement(c);
        break;
    }
    
    return node;
}

/* ParseIf - parse an 'if' statement */
static ParseTreeNode *ParseIf(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeIf);
    int tkn;
    
    /* parse the test expression */
    FRequire(c, '(');
    node->u.ifStatement.test = ParseExpr(c);
    FRequire(c, ')');
    
    /* parse the 'then' statement */
    node->u.ifStatement.thenStatement = ParseStatement(c);
    
    /* check for an 'else' statement */
    if ((tkn = GetToken(c)) == T_ELSE)
        node->u.ifStatement.elseStatement = ParseStatement(c);
    else
        SaveToken(c, tkn);
        
    return node;
}

/* ParseWhile - parse a 'while' statement */
static ParseTreeNode *ParseWhile(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeWhile);
    FRequire(c, '(');
    node->u.whileStatement.test = ParseExpr(c);
    FRequire(c, ')');
    node->u.whileStatement.body = ParseStatement(c);
    return node;
}

/* ParseDoWhile - parse a 'do/while' statement */
static ParseTreeNode *ParseDoWhile(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeDoWhile);
    node->u.doWhileStatement.body = ParseStatement(c);
    FRequire(c, T_WHILE);
    FRequire(c, '(');
    node->u.doWhileStatement.test = ParseExpr(c);
    FRequire(c, ')');
    FRequire(c, ';');
    return node;
}

/* ParseFor - parse a 'for' statement */
static ParseTreeNode *ParseFor(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeFor);
    int tkn;
    
    /* parse the init part */
    FRequire(c, '(');
    if ((tkn = GetToken(c)) != ';') {
        SaveToken(c, tkn);
        node->u.forStatement.init = ParseExpr(c);
        FRequire(c, ';');
    }
    
    /* parse the test part */
    if ((tkn = GetToken(c)) != ';') {
        SaveToken(c, tkn);
        node->u.forStatement.test = ParseExpr(c);
        FRequire(c, ';');
    }
    
    /* parse the incr part of the 'for' stateme*/
    if ((tkn = GetToken(c)) != ')') {
        SaveToken(c, tkn);
        node->u.forStatement.incr = ParseExpr(c);
        FRequire(c, ')');
    }
    
    /* parse the body */
    node->u.forStatement.body = ParseStatement(c);
    
    return node;
}

/* ParseBreak - parse a 'break' statement */
static ParseTreeNode *ParseBreak(ParseContext *c)
{
    return NewParseTreeNode(c, NodeTypeBreak);
}

/* ParseContinue - parse a 'continue' statement */
static ParseTreeNode *ParseContinue(ParseContext *c)
{
    return NewParseTreeNode(c, NodeTypeContinue);
}

/* ParseReturn - parse a 'return' statement */
static ParseTreeNode *ParseReturn(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeReturn);
    int tkn;
    if ((tkn = GetToken(c)) != ';') {
        SaveToken(c, tkn);
        node->u.returnStatement.value = ParseExpr(c);
        FRequire(c, ';');
    }
    return node;
}

/* ParseBlock - parse a {} block */
static ParseTreeNode *ParseBlock(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeBlock);
    NodeListEntry **pNextStatement = &node->u.blockStatement.statements;
    int tkn;    
    while ((tkn = GetToken(c)) != '}') {
        SaveToken(c, tkn);
        AddNodeToList(c, &pNextStatement, ParseStatement(c));
    }
    return node;
}

/* ParseTry - parse the 'try/catch/finally' statement */
static ParseTreeNode *ParseTry(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeTry);
    int tkn;
        
    FRequire(c, '{');
    node->u.tryStatement.statement = ParseBlock(c);
    
    if ((tkn = GetToken(c)) == T_CATCH) {
        LocalSymbol *sym;
        if (++c->currentTryDepth > c->currentFunction->u.functionDef.maximumTryDepth)
            c->currentFunction->u.functionDef.maximumTryDepth = c->currentTryDepth;
        FRequire(c, '(');
        FRequire(c, T_IDENTIFIER);
        sym = MakeLocalSymbol(c, c->token, c->currentFunction->u.functionDef.locals.count + c->currentTryDepth - 1);
        sym->next = c->trySymbols;
        c->trySymbols = sym;
        FRequire(c, ')');
        FRequire(c, '{');
        node->u.tryStatement.catchSymbol = sym;
        node->u.tryStatement.catchStatement = ParseBlock(c);
        sym = c->trySymbols;
        c->trySymbols = sym->next;
        --c->currentTryDepth;
    }
    else {
        SaveToken(c, tkn);
    }
    
    if ((tkn = GetToken(c)) == T_FINALLY) {
        FRequire(c, '{');
        node->u.tryStatement.finallyStatement = ParseBlock(c);
    }
    else {
        SaveToken(c, tkn);
    }
    
    if (!node->u.tryStatement.catchStatement && !node->u.tryStatement.finallyStatement)
        ParseError(c, "try requires either a catch or a finally clause");
            
    return node;
}

/* ParseThrow - parse the 'throw' statement */
static ParseTreeNode *ParseThrow(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeThrow);
    node->u.throwStatement.expr = ParseExpr(c);
    FRequire(c, ';');
    return node;
}

/* ParseExprStatement - parse an expression statement */
static ParseTreeNode *ParseExprStatement(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeExpr);
    node->u.exprStatement.expr = ParseExpr(c);
    FRequire(c, ';');
    return node;
}

/* ParseEmpty - parse an empty statement */
static ParseTreeNode *ParseEmpty(ParseContext *c)
{
    return NewParseTreeNode(c, NodeTypeEmpty);
}

/* ParsePrint - handle the 'PRINT' statement */
static ParseTreeNode *ParsePrint(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypePrint);
    PrintOp *op, **pNext = &node->u.printStatement.ops;
    int needNewline = VMTRUE;
    ParseTreeNode *expr;
    int tkn;

    while ((tkn = GetToken(c)) != ';') {
        switch (tkn) {
        case ',':
            needNewline = VMFALSE;
            op = LocalAlloc(c, sizeof(PrintOp));
            op->trap = TRAP_PrintTab;
            op->expr = NULL;
            op->next = NULL;
            *pNext = op;
            pNext = &op->next;
            break;
        case '$':
            needNewline = VMFALSE;
            break;
        case '#':
            op = LocalAlloc(c, sizeof(PrintOp));
            op->trap = TRAP_PrintStr;
            op->expr = ParseExpr(c);
            op->next = NULL;
            *pNext = op;
            pNext = &op->next;
            break;
        default:
            needNewline = VMTRUE;
            SaveToken(c, tkn);
            expr = ParseExpr(c);
            switch (expr->nodeType) {
            case NodeTypeStringLit:
                op = LocalAlloc(c, sizeof(PrintOp));
                op->trap = TRAP_PrintStr;
                op->expr = expr;
                op->next = NULL;
                *pNext = op;
                pNext = &op->next;
                break;
            default:
                op = LocalAlloc(c, sizeof(PrintOp));
                op->trap = TRAP_PrintInt;
                op->expr = expr;
                op->next = NULL;
                *pNext = op;
                pNext = &op->next;
                break;
            }
            break;
        }
    }

    if (needNewline) {
        op = LocalAlloc(c, sizeof(PrintOp));
        op->trap = TRAP_PrintNL;
        op->expr = NULL;
        op->next = NULL;
        *pNext = op;
        pNext = &op->next;
    }
    else {
        op = LocalAlloc(c, sizeof(PrintOp));
        op->trap = TRAP_PrintFlush;
        op->expr = NULL;
        op->next = NULL;
        *pNext = op;
        pNext = &op->next;
    }
        
    return node;
}

/* ParseIntegerLiteralExpr - parse an integer literal expression */
static VMVALUE ParseIntegerLiteralExpr(ParseContext *c)
{
    ParseTreeNode *expr = ParseExpr(c);
    VMVALUE value;
    if (!IsIntegerLit(expr, &value))
        ParseError(c, "expecting a constant expression");
    return value;
}

/* ParseConstantLiteralExpr - parse a constant literal expression (including objects and functions) */
static VMVALUE ParseConstantLiteralExpr(ParseContext *c, VMVALUE offset)
{
    ParseTreeNode *expr = ParseExpr(c);
    VMVALUE value = NIL;
    switch (expr->nodeType) {
    case NodeTypeIntegerLit:
        value = expr->u.integerLit.value;
        break;
    case NodeTypeStringLit:
        value = expr->u.stringLit.string->offset;
        break;
    case NodeTypeGlobalSymbolRef:
        switch (expr->u.symbolRef.symbol->storageClass) {
        case SC_OBJECT:
        case SC_FUNCTION:
            value = AddSymbolRef(c, expr->u.symbolRef.symbol, FT_DATA, offset);
            break;
        default:
            ParseError(c, "expecting a constant expression, object, or function");
            break;
        }
        break;
    default:
        ParseError(c, "expecting a constant expression, object, or function");
        break;
    }
    return value;
}

/* ParseExpr - handle assignment operators */
static ParseTreeNode *ParseExpr(ParseContext *c)
{
    ParseTreeNode *node;
    int tkn;
    node = ParseExpr1(c);
    while ((tkn = GetToken(c)) == '='
    ||      tkn == T_ADDEQ || tkn == T_SUBEQ
    ||      tkn == T_MULEQ || tkn == T_DIVEQ || tkn == T_REMEQ
    ||      tkn == T_ANDEQ || tkn == T_OREQ  || tkn == T_XOREQ
    ||      tkn == T_SHLEQ || tkn == T_SHREQ) {
        ParseTreeNode *node2 = ParseExpr(c);
        int op;
        switch (tkn) {
        case '=':
            op = OP_EQ; // indicator of simple assignment
            break;
        case T_ADDEQ:
            op = OP_ADD;
            break;
        case T_SUBEQ:
            op = OP_SUB;
            break;
        case T_MULEQ:
            op = OP_MUL;
            break;
        case T_DIVEQ:
            op = OP_DIV;
            break;
        case T_REMEQ:
            op = OP_REM;
            break;
        case T_ANDEQ:
            op = OP_BAND;
            break;
        case T_OREQ:
            op = OP_BOR;
            break;
        case T_XOREQ:
            op = OP_BXOR;
            break;
        case T_SHLEQ:
            op = OP_SHL;
            break;
        case T_SHREQ:
            op = OP_SHR;
            break;
        default:
            /* not reached */
            op = 0;
            break;
        }
        node = MakeAssignmentOpNode(c, op, node, node2);
    }
    SaveToken(c, tkn);
    return node;
}

/* ParseExpr1 - handle the '||' operator */
static ParseTreeNode *ParseExpr1(ParseContext *c)
{
    ParseTreeNode *node;
    int tkn;
    node = ParseExpr2(c);
    if ((tkn = GetToken(c)) == T_OR) {
        ParseTreeNode *node2 = NewParseTreeNode(c, NodeTypeDisjunction);
        NodeListEntry *entry, **pLast;
        node2->u.exprList.exprs = entry = (NodeListEntry *)LocalAlloc(c, sizeof(NodeListEntry));
        entry->node = node;
        entry->next = NULL;
        pLast = &entry->next;
        do {
            entry = (NodeListEntry *)LocalAlloc(c, sizeof(NodeListEntry));
            entry->node = ParseExpr2(c);
            entry->next = NULL;
            *pLast = entry;
            pLast = &entry->next;
        } while ((tkn = GetToken(c)) == T_OR);
        node = node2;
    }
    SaveToken(c, tkn);
    return node;
}

/* ParseExpr2 - handle the '&&' operator */
static ParseTreeNode *ParseExpr2(ParseContext *c)
{
    ParseTreeNode *node;
    int tkn;
    node = ParseExpr3(c);
    if ((tkn = GetToken(c)) == T_AND) {
        ParseTreeNode *node2 = NewParseTreeNode(c, NodeTypeConjunction);
        NodeListEntry *entry, **pLast;
        node2->u.exprList.exprs = entry = (NodeListEntry *)LocalAlloc(c, sizeof(NodeListEntry));
        entry->node = node;
        entry->next = NULL;
        pLast = &entry->next;
        do {
            entry = (NodeListEntry *)LocalAlloc(c, sizeof(NodeListEntry));
            entry->node = ParseExpr2(c);
            entry->next = NULL;
            *pLast = entry;
            pLast = &entry->next;
        } while ((tkn = GetToken(c)) == T_AND);
        node = node2;
    }
    SaveToken(c, tkn);
    return node;
}

/* ParseExpr3 - handle the '^' operator */
static ParseTreeNode *ParseExpr3(ParseContext *c)
{
    ParseTreeNode *expr, *expr2;
    int tkn;
    expr = ParseExpr4(c);
    while ((tkn = GetToken(c)) == '^') {
        VMVALUE value, value2;
        expr2 = ParseExpr4(c);
        if (IsIntegerLit(expr, &value) && IsIntegerLit(expr2, &value2))
            expr->u.integerLit.value = value ^ value2;
        else
            expr = MakeBinaryOpNode(c, OP_BXOR, expr, expr2);
    }
    SaveToken(c,tkn);
    return expr;
}

/* ParseExpr4 - handle the '|' operator */
static ParseTreeNode *ParseExpr4(ParseContext *c)
{
    ParseTreeNode *expr, *expr2;
    int tkn;
    expr = ParseExpr5(c);
    while ((tkn = GetToken(c)) == '|') {
        VMVALUE value, value2;
        expr2 = ParseExpr5(c);
        if (IsIntegerLit(expr, &value) && IsIntegerLit(expr2, &value2))
            expr->u.integerLit.value = value | value2;
        else
            expr = MakeBinaryOpNode(c, OP_BOR, expr, expr2);
    }
    SaveToken(c,tkn);
    return expr;
}

/* ParseExpr5 - handle the '&' operator */
static ParseTreeNode *ParseExpr5(ParseContext *c)
{
    ParseTreeNode *expr, *expr2;
    int tkn;
    expr = ParseExpr6(c);
    while ((tkn = GetToken(c)) == '&') {
        VMVALUE value, value2;
        expr2 = ParseExpr6(c);
        if (IsIntegerLit(expr, &value) && IsIntegerLit(expr2, &value2))
            expr->u.integerLit.value = value & value2;
        else
            expr = MakeBinaryOpNode(c, OP_BAND, expr, expr2);
    }
    SaveToken(c,tkn);
    return expr;
}

/* ParseExpr6 - handle the '==' and '!=' operators */
static ParseTreeNode *ParseExpr6(ParseContext *c)
{
    ParseTreeNode *expr, *expr2;
    int tkn;
    expr = ParseExpr7(c);
    while ((tkn = GetToken(c)) == T_EQ || tkn == T_NE) {
        int op;
        expr2 = ParseExpr7(c);
        switch (tkn) {
        case T_EQ:
            op = OP_EQ;
            break;
        case T_NE:
            op = OP_NE;
            break;
        default:
            /* not reached */
            op = 0;
            break;
        }
        expr = MakeBinaryOpNode(c, op, expr, expr2);
    }
    SaveToken(c,tkn);
    return expr;
}

/* ParseExpr7 - handle the '<', '<=', '>=' and '>' operators */
static ParseTreeNode *ParseExpr7(ParseContext *c)
{
    ParseTreeNode *expr, *expr2;
    int tkn;
    expr = ParseExpr8(c);
    while ((tkn = GetToken(c)) == '<' || tkn == T_LE || tkn == T_GE || tkn == '>') {
        int op;
        expr2 = ParseExpr8(c);
        switch (tkn) {
        case '<':
            op = OP_LT;
            break;
        case T_LE:
            op = OP_LE;
            break;
        case T_GE:
            op = OP_GE;
            break;
        case '>':
            op = OP_GT;
            break;
        default:
            /* not reached */
            op = 0;
            break;
        }
        expr = MakeBinaryOpNode(c, op, expr, expr2);
    }
    SaveToken(c,tkn);
    return expr;
}

/* ParseExpr8 - handle the '<<' and '>>' operators */
static ParseTreeNode *ParseExpr8(ParseContext *c)
{
    ParseTreeNode *expr, *expr2;
    int tkn;
    expr = ParseExpr9(c);
    while ((tkn = GetToken(c)) == T_SHL || tkn == T_SHR) {
        VMVALUE value, value2;
        expr2 = ParseExpr9(c);
        if (IsIntegerLit(expr, &value) && IsIntegerLit(expr2, &value2)) {
            switch (tkn) {
            case T_SHL:
                expr->u.integerLit.value = value << value2;
                break;
            case T_SHR:
                expr->u.integerLit.value = value >> value2;
                break;
            default:
                /* not reached */
                break;
            }
        }
        else {
            int op;
            switch (tkn) {
            case T_SHL:
                op = OP_SHL;
                break;
            case T_SHR:
                op = OP_SHR;
                break;
            default:
                /* not reached */
                op = 0;
                break;
            }
            expr = MakeBinaryOpNode(c, op, expr, expr2);
        }
    }
    SaveToken(c,tkn);
    return expr;
}

/* ParseExpr9 - handle the '+' and '-' operators */
static ParseTreeNode *ParseExpr9(ParseContext *c)
{
    ParseTreeNode *expr, *expr2;
    int tkn;
    expr = ParseExpr10(c);
    while ((tkn = GetToken(c)) == '+' || tkn == '-') {
        VMVALUE value, value2;
        expr2 = ParseExpr10(c);
        if (IsIntegerLit(expr, &value) && IsIntegerLit(expr2, &value2)) {
            switch (tkn) {
            case '+':
                expr->u.integerLit.value = value + value2;
                break;
            case '-':
                expr->u.integerLit.value = value - value2;
                break;
            default:
                /* not reached */
                break;
            }
        }
        else {
            int op;
            switch (tkn) {
            case '+':
                op = OP_ADD;
                break;
            case '-':
                op = OP_SUB;
                break;
            default:
                /* not reached */
                op = 0;
                break;
            }
            expr = MakeBinaryOpNode(c, op, expr, expr2);
        }
    }
    SaveToken(c, tkn);
    return expr;
}

/* ParseExpr10 - handle the '*', '/' and '%' operators */
static ParseTreeNode *ParseExpr10(ParseContext *c)
{
    ParseTreeNode *expr, *expr2;
    int tkn;
    expr = ParseExpr11(c);
    while ((tkn = GetToken(c)) == '*' || tkn == '/' || tkn == '%') {
        VMVALUE value, value2;
        expr2 = ParseExpr11(c);
        if (IsIntegerLit(expr, &value) && IsIntegerLit(expr2, &value2)) {
            switch (tkn) {
            case '*':
                expr->u.integerLit.value = value * value2;
                break;
            case '/':
                if (expr2->u.integerLit.value == 0)
                    ParseError(c, "division by zero in constant expression");
                expr->u.integerLit.value = value / value2;
                break;
            case '%':
                if (value2 == 0)
                    ParseError(c, "division by zero in constant expression");
                expr->u.integerLit.value = value % value2;
                break;
            default:
                /* not reached */
                break;
            }
        }
        else {
            int op;
            switch (tkn) {
            case '*':
                op = OP_MUL;
                break;
            case '/':
                op = OP_DIV;
                break;
            case '%':
                op = OP_REM;
                break;
            default:
                /* not reached */
                op = 0;
                break;
            }
            expr = MakeBinaryOpNode(c, op, expr, expr2);
        }
    }
    SaveToken(c, tkn);
    return expr;
}

/* ParseExpr11 - handle unary operators */
static ParseTreeNode *ParseExpr11(ParseContext *c)
{
    ParseTreeNode *node;
    VMVALUE value;
    int tkn;
    switch (tkn = GetToken(c)) {
    case '+':
        node = ParsePrimary(c);
        break;
    case '-':
        node = ParsePrimary(c);
        if (IsIntegerLit(node, &value))
            node->u.integerLit.value = -value;
        else
            node = MakeUnaryOpNode(c, OP_NEG, node);
        break;
    case '!':
        node = ParsePrimary(c);
        if (IsIntegerLit(node, &value))
            node->u.integerLit.value = !value;
        else
            node = MakeUnaryOpNode(c, OP_NOT, node);
        break;
    case '~':
        node = ParsePrimary(c);
        if (IsIntegerLit(node, &value))
            node->u.integerLit.value = ~value;
        else
            node = MakeUnaryOpNode(c, OP_BNOT, node);
        break;
    case T_INC:
        node = NewParseTreeNode(c, NodeTypePreincrementOp);
        node->u.incrementOp.increment = 1;
        node->u.incrementOp.expr = ParsePrimary(c);
        break;
    case T_DEC:
        node = NewParseTreeNode(c, NodeTypePreincrementOp);
        node->u.incrementOp.increment = -1;
        node->u.incrementOp.expr = ParsePrimary(c);
        break;
    default:
        SaveToken(c,tkn);
        node = ParsePrimary(c);
        break;
    }
    return node;
}

/* ParsePrimary - parse function calls and array references */
static ParseTreeNode *ParsePrimary(ParseContext *c)
{
    ParseTreeNode *node, *node2;
    int tkn;
    node = ParseSimplePrimary(c);
    while ((tkn = GetToken(c)) == '[' || tkn == '(' || tkn == '.' || tkn == T_INC || tkn == T_DEC) {
        switch (tkn) {
        case '[':
            node = ParseArrayReference(c, node);
            break;
        case '(':
            node = ParseCall(c, node);
            break;
        case '.':
            node = ParsePropertyRef(c, node);
            break;
        case T_INC:
            node2 = NewParseTreeNode(c, NodeTypePostincrementOp);
            node2->u.incrementOp.increment = 1;
            node2->u.incrementOp.expr = node;
            node = node2;
            break;
        case T_DEC:
            node2 = NewParseTreeNode(c, NodeTypePostincrementOp);
            node2->u.incrementOp.increment = -1;
            node2->u.incrementOp.expr = node;
            node = node2;
            break;
        }
    }
    SaveToken(c, tkn);
    return node;
}

/* ParseArrayReference - parse an array reference */
static ParseTreeNode *ParseArrayReference(ParseContext *c, ParseTreeNode *arrayNode)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeArrayRef);

    /* setup the array reference */
    node->u.arrayRef.array = arrayNode;

    /* get the index expression */
    node->u.arrayRef.index = ParseExpr(c);

    /* check for the close bracket */
    FRequire(c, ']');
    return node;
}

/* ParseCall - parse a function call */
static ParseTreeNode *ParseCall(ParseContext *c, ParseTreeNode *functionNode)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeFunctionCall);
    NodeListEntry **pLast;
    int tkn;

    /* intialize the function call node */
    node->u.functionCall.fcn = functionNode;
    pLast = &node->u.functionCall.args;

    /* parse the argument list */
    if ((tkn = GetToken(c)) != ')') {
        SaveToken(c, tkn);
        do {
            NodeListEntry *actual;
            actual = (NodeListEntry *)LocalAlloc(c, sizeof(NodeListEntry));
            actual->node = ParseExpr(c);
            actual->next = NULL;
            *pLast = actual;
            pLast = &actual->next;
            ++node->u.functionCall.argc;
        } while ((tkn = GetToken(c)) == ',');
        Require(c, tkn, ')');
    }

    /* return the function call node */
    return node;
}

/* ParseSelector - parse a property selector */
static ParseTreeNode *ParseSelector(ParseContext *c)
{
    ParseTreeNode *node;
    int tkn;
    if ((tkn = GetToken(c)) == T_IDENTIFIER) {
        node = NewParseTreeNode(c, NodeTypeIntegerLit);
        node->u.integerLit.value = AddProperty(c, c->token);
    }
    else if (tkn == '(') {
        SaveToken(c, tkn);
        node = ParseSimplePrimary(c);
    }
    else {
        ParseError(c, "expecting a property name or parenthesized expression");
        node = NULL; // never reached
    }
    return node;
}

/* ParseSend - parse a message send */
static ParseTreeNode *ParseSend(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeSend);
    NodeListEntry **pLast;
    int tkn;

    /* parse the object and selector */
    if ((tkn = GetToken(c)) == T_SUPER) {
        if (!c->currentObjectSymbol)
            ParseError(c, "super outside of a method definition");
        node->u.send.class = NewParseTreeNode(c, NodeTypeGlobalSymbolRef);
        node->u.send.class->u.symbolRef.symbol = c->currentObjectSymbol;
        node->u.send.object = NewParseTreeNode(c, NodeTypeArgumentRef);
        node->u.send.object->u.localSymbolRef.symbol = FindLocalSymbol(&c->currentFunction->u.functionDef.arguments, "self");
    }
    else {
        SaveToken(c, tkn);
        node->u.send.class = NULL;
        node->u.send.object = ParseExpr(c);
    }
    node->u.send.selector = ParseSelector(c);

    /* parse the argument list */
    pLast = &node->u.send.args;
    if ((tkn = GetToken(c)) != ']') {
        SaveToken(c, tkn);
        do {
            NodeListEntry *actual;
            actual = (NodeListEntry *)LocalAlloc(c, sizeof(NodeListEntry));
            actual->node = ParseExpr(c);
            actual->next = NULL;
            *pLast = actual;
            pLast = &actual->next;
            ++node->u.send.argc;
        } while ((tkn = GetToken(c)) == ',');
        Require(c, tkn, ']');
    }

    /* return the function call node */
    return node;
}

/* ParsePropertyRef - parse a property reference */
static ParseTreeNode *ParsePropertyRef(ParseContext *c, ParseTreeNode *object)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypePropertyRef);
    ParseTreeNode *property;
    int tkn;
    
    if ((tkn = GetToken(c)) == T_IDENTIFIER) {
        property = NewParseTreeNode(c, NodeTypeIntegerLit);
        property->u.integerLit.value = AddProperty(c, c->token);
    }
    else if (tkn == '(') {
        SaveToken(c, tkn);
        property = ParseSimplePrimary(c);
    }
    else {
        ParseError(c, "expecting a property name or parenthesized expression");
        property = NULL; // never reached
    }
    
    node->u.propertyRef.object = object;
    node->u.propertyRef.property = property;
    
    return node;
}

/* ParseSimplePrimary - parse a primary expression */
static ParseTreeNode *ParseSimplePrimary(ParseContext *c)
{
    ParseTreeNode *node;
    switch (GetToken(c)) {
    case '(':
        node = ParseExpr(c);
        FRequire(c,')');
        break;
    case '[':
        node = ParseSend(c);
        break;
    case T_NUMBER:
        node = NewParseTreeNode(c, NodeTypeIntegerLit);
        node->u.integerLit.value = c->value;
        break;
    case T_STRING:
        node = NewParseTreeNode(c, NodeTypeStringLit);
        node->u.stringLit.string = AddString(c, c->token);
        break;
    case T_IDENTIFIER:
        node = GetSymbolRef(c, c->token);
        break;
    default:
        ParseError(c, "Expecting a primary expression");
        node = NULL; /* not reached */
        break;
    }
    return node;
}

/* GetSymbolRef - setup a symbol reference */
static ParseTreeNode *GetSymbolRef(ParseContext *c, char *name)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeGlobalSymbolRef);
    LocalSymbol *localSymbol = NULL;
    Symbol *symbol;

    /* handle references to try/catch symbols */
    if (c->currentFunction) {
        for (localSymbol = c->trySymbols; localSymbol != NULL; localSymbol = localSymbol->next) {
            if (strcmp(name, localSymbol->name) == 0) {
                node->nodeType = NodeTypeLocalSymbolRef;
                node->u.localSymbolRef.symbol = localSymbol;
                return node;
            }
        }
    }
    
    /* handle local variables within a function */
    if (c->currentFunction && (localSymbol = FindLocalSymbol(&c->currentFunction->u.functionDef.locals, name)) != NULL) {
        node->nodeType = NodeTypeLocalSymbolRef;
        node->u.localSymbolRef.symbol = localSymbol;
    }

    /* handle function arguments */
    else if (c->currentFunction && (localSymbol = FindLocalSymbol(&c->currentFunction->u.functionDef.arguments, name)) != NULL) {
        node->nodeType = NodeTypeArgumentRef;
        node->u.localSymbolRef.symbol = localSymbol;
    }

    /* handle global symbols */
    else if ((symbol = FindSymbol(c, c->token)) != NULL) {
        if (symbol->storageClass == SC_CONSTANT) {
            node->nodeType = NodeTypeIntegerLit;
            node->u.integerLit.value = symbol->v.value;
        }
        else {
            node->u.symbolRef.symbol = symbol;
        }
    }

    /* handle undefined symbols */
    else {
        node->u.symbolRef.symbol = AddUndefinedSymbol(c, name, SC_OBJECT);
    }

    /* return the symbol reference node */
    return node;
}

/* MakeUnaryOpNode - allocate a unary operation parse tree node */
static ParseTreeNode *MakeUnaryOpNode(ParseContext *c, int op, ParseTreeNode *expr)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeUnaryOp);
    node->u.unaryOp.op = op;
    node->u.unaryOp.expr = expr;
    return node;
}

/* MakeBinaryOpNode - allocate a binary operation parse tree node */
static ParseTreeNode *MakeBinaryOpNode(ParseContext *c, int op, ParseTreeNode *left, ParseTreeNode *right)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeBinaryOp);
    node->u.binaryOp.op = op;
    node->u.binaryOp.left = left;
    node->u.binaryOp.right = right;
    return node;
}

/* MakeAssignmentOpNode - allocate an assignment operation parse tree node */
static ParseTreeNode *MakeAssignmentOpNode(ParseContext *c, int op, ParseTreeNode *left, ParseTreeNode *right)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeAssignmentOp);
    node->u.binaryOp.op = op;
    node->u.binaryOp.left = left;
    node->u.binaryOp.right = right;
    return node;
}

/* NewParseTreeNode - allocate a new parse tree node */
static ParseTreeNode *NewParseTreeNode(ParseContext *c, int type)
{
    ParseTreeNode *node = (ParseTreeNode *)LocalAlloc(c, sizeof(ParseTreeNode));
    memset(node, 0, sizeof(ParseTreeNode));
    node->nodeType = type;
    return node;
}

/* AddNodeToList - add a node to a parse tree node list */
static void AddNodeToList(ParseContext *c, NodeListEntry ***ppNextEntry, ParseTreeNode *node)
{
    NodeListEntry *entry = (NodeListEntry *)LocalAlloc(c, sizeof(NodeListEntry));
    entry->node = node;
    entry->next = NULL;
    **ppNextEntry = entry;
    *ppNextEntry = &entry->next;
}

/* InitLocalSymbolTable - initialize a local symbol table */
static void InitLocalSymbolTable(LocalSymbolTable *table)
{
    table->head = NULL;
    table->pTail = &table->head;
    table->count = 0;
}

/* AddLocalSymbol - add a symbol to a local symbol table */
static LocalSymbol *AddLocalSymbol(ParseContext *c, LocalSymbolTable *table, const char *name, int offset)
{
    LocalSymbol *sym = MakeLocalSymbol(c, name, offset);
    *table->pTail = sym;
    table->pTail = &sym->next;
    ++table->count;
    return sym;
}

/* MakeLocalSymbol - allocate and initialize a local symbol structure */
static LocalSymbol *MakeLocalSymbol(ParseContext *c, const char *name, int offset)
{
    size_t size = sizeof(LocalSymbol) + strlen(name);
    LocalSymbol *sym = (LocalSymbol *)LocalAlloc(c, size);
    memset(sym, 0, sizeof(LocalSymbol));
    strcpy(sym->name, name);
    sym->offset = offset;
    return sym;
}

/* FindLocalSymbol - find a symbol in a local symbol table */
static LocalSymbol *FindLocalSymbol(LocalSymbolTable *table, const char *name)
{
    LocalSymbol *sym = table->head;
    while (sym) {
        if (strcmp(name, sym->name) == 0)
            return sym;
        sym = sym->next;
    }
    return NULL;
}

/* PrintLocalSymbols - print a symbol table */
void PrintLocalSymbols(LocalSymbolTable *table, char *tag, int indent)
{
    LocalSymbol *sym;
    if ((sym = table->head) != NULL) {
	    printf("%*s%s\n", indent, "", tag);
        for (; sym != NULL; sym = sym->next)
            printf("%*s%s\t%d\n", indent + 2, "", sym->name, sym->offset);
    }
}

/* IsIntegerLit - check to see if a node is an integer literal */
static int IsIntegerLit(ParseTreeNode *node, VMVALUE *pValue)
{
    int result = VMTRUE;
    switch (node->nodeType) {
    case NodeTypeIntegerLit:
        *pValue = node->u.integerLit.value;
        break;
    case NodeTypeStringLit:
        *pValue = node->u.stringLit.string->offset;
        break;
    default:
        result = VMFALSE;
        break;
    }
    return result;
}
