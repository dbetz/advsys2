/* db_expr.c - expression parser
 *
 * Copyright (c) 2014 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdlib.h>
#include "adv2compiler.h"

/* local function prototypes */
static void ParseDef(ParseContext *c);
static void ParseConstantDef(ParseContext *c, char *name);
static void ParseFunctionDef(ParseContext *c, char *name);
static void ParseVar(ParseContext *c);
static void ParseObject(ParseContext *c, char *name);
static ParseTreeNode *ParseIf(ParseContext *c);
static ParseTreeNode *ParseWhile(ParseContext *c);
static ParseTreeNode *ParseDoWhile(ParseContext *c);
static ParseTreeNode *ParseFor(ParseContext *c);
static ParseTreeNode *ParseBreak(ParseContext *c);
static ParseTreeNode *ParseContinue(ParseContext *c);
static ParseTreeNode *ParseReturn(ParseContext *c);
static ParseTreeNode *ParseBlock(ParseContext *c);
static ParseTreeNode *ParseExprStatement(ParseContext *c);
static ParseTreeNode *ParseEmpty(ParseContext *c);
static ParseTreeNode *ParsePrint(ParseContext *c);
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
static void AddNodeToList(ParseContext *c, NodeListEntry ***ppNextEntry, ParseTreeNode *node);
static Symbol *AddSymbol(ParseContext *c, SymbolTable *table, const char *name, int value);
static int IsIntegerLit(ParseTreeNode *node);

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
        Require(c, tkn, '(');
        ParseFunctionDef(c, name);
        complete = VMFALSE;
    }
}

/* ParseConstantDef - parse a 'def <name> =' statement */
static void ParseConstantDef(ParseContext *c, char *name)
{
    ParseTreeNode *expr;

    /* get the constant value */
    expr = ParseExpr(c);

    /* make sure it's a constant */
    if (!IsIntegerLit(expr))
        ParseError(c, "expecting a constant expression");

    /* add the symbol as a global */
    AddGlobal(c, name, SC_CONSTANT, expr->u.integerLit.value);

    FRequire(c, ';');
}

/* ParseFunctionDef - parse a 'def <name> () {}' statement */
static void ParseFunctionDef(ParseContext *c, char *name)
{
    /* enter the function name in the global symbol table */
    AddGlobal(c, name, SC_VARIABLE, 0);
    ParseFunction(c);
}

/* ParseVar - parse the 'var' statement */
static void ParseVar(ParseContext *c)
{
    int tkn;
    do {
        FRequire(c, T_IDENTIFIER);
        AddGlobal(c, c->token, SC_VARIABLE, 0);
    } while ((tkn = GetToken(c)) == ',');
    Require(c, tkn, ';');
}

/* ParseObject - parse the 'object' statement */
static void ParseObject(ParseContext *c, char *name)
{
}

#if 0

    /* copy the property list of the class object */
    if (class) {
        obase = otable[class];
        osize = getword(obase+O_NPROPERTIES);
        for (i = p = 0; i < osize; i++, p += 4)
            if ((getword(obase+O_PROPERTIES+p) & P_CLASS) == 0)
                addprop(getword(obase+O_PROPERTIES+p),0,
                        getword(obase+O_PROPERTIES+p+2));
    }

/* do_property - handle the <PROPERTY ... > statement */
void do_property(int flags)
{
    int tkn,name,value;

    while ((tkn = token()) == T_IDENTIFIER || tkn == T_NUMBER) {
        name = (tkn == T_IDENTIFIER ? penter(t_token) : t_value);
        value = getvalue();
        setprop(name,flags,value);
    }
    require(tkn,T_CLOSE);
}

/* do_method - handle <METHOD (FUN ...) ... > statement */
void do_method(void)
{
    int tkn,name,tcnt;

    /* get the property name */
    frequire(T_OPEN);
    frequire(T_IDENTIFIER);
printf("[ method: %s ]\n",t_token);

    /* create a new property */
    name = penter(t_token);

    /* allocate a new (anonymous) action */
    if (acnt < AMAX)
        ++acnt;
    else
        error("too many actions");

    /* store the action as the value of the property */
    setprop(name,P_CLASS,acnt);

    /* initialize the action */
    curact = atable[acnt] = dalloc(A_SIZE);
    putword(curact+A_VERBS,NIL);
    putword(curact+A_PREPOSITIONS,NIL);
    arguments = temporaries = NULL;
    tcnt = 0;

    /* enter the "self" argument */
    addargument(&arguments,"self");
    addargument(&arguments,"(dummy)");

    /* get the argument list */
    while ((tkn = token()) != T_CLOSE) {
        require(tkn,T_IDENTIFIER);
        if (match("&aux"))
            break;
        addargument(&arguments,t_token);
    }
    
    /* check for temporary variable definitions */
    if (tkn == T_IDENTIFIER)
        while ((tkn = token()) != T_CLOSE) {
            require(tkn,T_IDENTIFIER);
            addargument(&temporaries,t_token);
            tcnt++;
        }

    /* store the code address */
    putword(curact+A_CODE,cptr);

    /* allocate space for temporaries */
    if (temporaries) {
        putcbyte(OP_TSPACE);
        putcbyte(tcnt);
    }

    /* compile the code */
    do_code(NULL);

    /* free the argument and temporary variable symbol tables */
    freelist(arguments);
    freelist(temporaries);
    arguments = temporaries = NULL;
}

/* setprop - set the value of a property */
void setprop(int prop,int flags,int value)
{
    int i;

    /* look for the property */
    for (i = 0; i < nprops; i++)
        if ((objbuf[O_PROPERTIES/2 + i*2] & ~P_CLASS) == prop) {
            objbuf[O_PROPERTIES/2 + i*2 + 1] = value;
            return;
        }
    addprop(prop,flags,value);
}

/* addprop - add a property to the current object's property list */
void addprop(int prop,int flags,int value)
{
    if (nprops >= OPMAX) {
        printf("too many properties for this object\n");
        return;
    }
    objbuf[O_PROPERTIES/2 + nprops*2] = prop|flags;
    objbuf[O_PROPERTIES/2 + nprops*2 + 1] = value;
    objbuf[O_NPROPERTIES/2] = ++nprops;
}

#endif

/* ParseFunction - parse a function definition */
ParseTreeNode *ParseFunction(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeFunctionDef);
    int localOffset = 0;
    int tkn;
    
    c->function = node;
    InitSymbolTable(&node->u.functionDef.arguments);
    InitSymbolTable(&node->u.functionDef.locals);
    c->block = NULL;
    
    /* parse the argument list */
    FRequire(c, '(');
    if ((tkn = GetToken(c)) != ')') {
        int offset = 0;
        SaveToken(c, tkn);
        do {
            FRequire(c, T_IDENTIFIER);
            AddSymbol(c, &node->u.functionDef.arguments, c->token, offset);
            ++offset;
        } while ((tkn = GetToken(c)) == ',');
    }
    Require(c, tkn, ')');
    FRequire(c, '{');
    
    while ((tkn = GetToken(c)) == T_VAR) {
        do {
            FRequire(c, T_IDENTIFIER);
            AddSymbol(c, &node->u.functionDef.locals, c->token, localOffset);
            ++localOffset;
        } while ((tkn = GetToken(c)) == ',');
        Require(c, tkn, ';');
    }
    SaveToken(c, tkn);

    /* parse the function body */
    node->u.functionDef.body = ParseBlock(c);
    
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
            op->next = NULL;
            *pNext = op;
            pNext = &op->next;
            break;
        case '$':
            needNewline = VMFALSE;
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
        op->next = NULL;
        *pNext = op;
        pNext = &op->next;
    }
    else {
        op = LocalAlloc(c, sizeof(PrintOp));
        op->trap = TRAP_PrintFlush;
        op->next = NULL;
        *pNext = op;
        pNext = &op->next;
    }
        
    return node;
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
        expr2 = ParseExpr4(c);
        if (IsIntegerLit(expr) && IsIntegerLit(expr2))
            expr->u.integerLit.value = expr->u.integerLit.value ^ expr2->u.integerLit.value;
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
        expr2 = ParseExpr5(c);
        if (IsIntegerLit(expr) && IsIntegerLit(expr2))
            expr->u.integerLit.value = expr->u.integerLit.value | expr2->u.integerLit.value;
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
        expr2 = ParseExpr6(c);
        if (IsIntegerLit(expr) && IsIntegerLit(expr2))
            expr->u.integerLit.value = expr->u.integerLit.value & expr2->u.integerLit.value;
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
        expr2 = ParseExpr9(c);
        if (IsIntegerLit(expr) && IsIntegerLit(expr2)) {
            switch (tkn) {
            case T_SHL:
                expr->u.integerLit.value = expr->u.integerLit.value << expr2->u.integerLit.value;
                break;
            case T_SHR:
                expr->u.integerLit.value = expr->u.integerLit.value >> expr2->u.integerLit.value;
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
        expr2 = ParseExpr10(c);
        if (IsIntegerLit(expr) && IsIntegerLit(expr2)) {
            switch (tkn) {
            case '+':
                expr->u.integerLit.value = expr->u.integerLit.value + expr2->u.integerLit.value;
                break;
            case '-':
                expr->u.integerLit.value = expr->u.integerLit.value - expr2->u.integerLit.value;
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
    ParseTreeNode *node, *node2;
    int tkn;
    node = ParseExpr11(c);
    while ((tkn = GetToken(c)) == '*' || tkn == '/' || tkn == '%') {
        node2 = ParseExpr11(c);
        if (IsIntegerLit(node) && IsIntegerLit(node2)) {
            switch (tkn) {
            case '*':
                node->u.integerLit.value = node->u.integerLit.value * node2->u.integerLit.value;
                break;
            case '/':
                if (node2->u.integerLit.value == 0)
                    ParseError(c, "division by zero in constant expression");
                node->u.integerLit.value = node->u.integerLit.value / node2->u.integerLit.value;
                break;
            case '%':
                if (node2->u.integerLit.value == 0)
                    ParseError(c, "division by zero in constant expression");
                node->u.integerLit.value = node->u.integerLit.value % node2->u.integerLit.value;
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
            node = MakeBinaryOpNode(c, op, node, node2);
        }
    }
    SaveToken(c, tkn);
    return node;
}

/* ParseExpr11 - handle unary operators */
static ParseTreeNode *ParseExpr11(ParseContext *c)
{
    ParseTreeNode *node;
    int tkn;
    switch (tkn = GetToken(c)) {
    case '+':
        node = ParsePrimary(c);
        break;
    case '-':
        node = ParsePrimary(c);
        if (IsIntegerLit(node))
            node->u.integerLit.value = -node->u.integerLit.value;
        else
            node = MakeUnaryOpNode(c, OP_NEG, node);
        break;
    case '!':
        node = ParsePrimary(c);
        if (IsIntegerLit(node))
            node->u.integerLit.value = !node->u.integerLit.value;
        else
            node = MakeUnaryOpNode(c, OP_NOT, node);
        break;
    case '~':
        node = ParsePrimary(c);
        if (IsIntegerLit(node))
            node->u.integerLit.value = ~node->u.integerLit.value;
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

/* ParseSend - parse a message send */
static ParseTreeNode *ParseSend(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeSend);
    NodeListEntry **pLast;
    int tkn;

    /* parse the object and selector */
    if ((tkn = GetToken(c)) == T_SUPER)
        node->u.send.object = NULL;
    else {
        SaveToken(c, tkn);
        node->u.send.object = ParseExpr(c);
    }
    node->u.send.selector = ParseExpr(c);

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
    node->u.propertyRef.object = object;
    node->u.propertyRef.property = ParseSimplePrimary(c);
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
    Symbol *symbol;

    /* handle local variables within a function or subroutine */
    if ((symbol = FindSymbol(&c->function->u.functionDef.locals, name)) != NULL) {
        node->nodeType = NodeTypeLocalSymbolRef;
        node->u.symbolRef.symbol = symbol;
        node->u.symbolRef.offset = symbol->value;
    }

    /* handle function arguments */
    else if ((symbol = FindSymbol(&c->function->u.functionDef.arguments, name)) != NULL) {
        node->nodeType = NodeTypeArgumentRef;
        node->u.symbolRef.symbol = symbol;
        node->u.symbolRef.offset = symbol->value;
    }

    /* handle global symbols */
    else if ((symbol = FindSymbol(&c->globals, c->token)) != NULL) {
        if (IsConstant(symbol)) {
            node->nodeType = NodeTypeIntegerLit;
            node->u.integerLit.value = symbol->value;
        }
        else {
            node->u.symbolRef.symbol = symbol;
        }
    }

    /* handle undefined symbols */
    else {
        symbol = AddGlobal(c, name, SC_VARIABLE, 0);
        node->u.symbolRef.symbol = symbol;
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

/* InitSymbolTable - initialize a symbol table */
void InitSymbolTable(SymbolTable *table)
{
    table->head = NULL;
    table->pTail = &table->head;
    table->count = 0;
}

/* AddSymbol - add a symbol to a symbol table */
static Symbol *AddSymbol(ParseContext *c, SymbolTable *table, const char *name, int value)
{
    size_t size = sizeof(Symbol) + strlen(name);
    Symbol *sym;
    
    /* allocate the symbol structure */
    sym = (Symbol *)LocalAlloc(c, size);
    strcpy(sym->name, name);
    sym->value = value;
    sym->next = NULL;

    /* add it to the symbol table */
    *table->pTail = sym;
    table->pTail = &sym->next;
    ++table->count;
    
    /* return the symbol */
    return sym;
}

/* FindSymbol - find a symbol in a symbol table */
Symbol *FindSymbol(SymbolTable *table, const char *name)
{
    Symbol *sym = table->head;
    while (sym) {
        if (strcmp(name, sym->name) == 0)
            return sym;
        sym = sym->next;
    }
    return NULL;
}

/* IsConstant - check to see if the value of a symbol is a constant */
int IsConstant(Symbol *symbol)
{
    return symbol->storageClass == SC_CONSTANT;
}

/* IsIntegerLit - check to see if a node is an integer literal */
static int IsIntegerLit(ParseTreeNode *node)
{
    return node->nodeType == NodeTypeIntegerLit;
}

/* LocalAlloc - allocate memory from the local heap */
void *LocalAlloc(ParseContext *c, size_t size)
{
    void *data = (void *)malloc(size);
    if (!data) Abort(c, "insufficient memory");
    return data;
}

