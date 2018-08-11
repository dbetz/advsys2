/* db_compiler.h - definitions for a simple basic compiler
 *
 * Copyright (c) 2014 by David Michael Betz.  All rights reserved.
 *
 */

#ifndef __DB_COMPILER_H__
#define __DB_COMPILER_H__

#include <stdio.h>
#include <setjmp.h>
#include "adv2types.h"
#include "adv2image.h"
#include "adv2symbols.h"

/* program limits */
#define MAXLINE         128
#define MAXTOKEN        32
#define MAXCODE         65536

/* forward type declarations */
typedef struct ParseTreeNode ParseTreeNode;
typedef struct NodeListEntry NodeListEntry;
typedef struct NodeListEntry NodeListEntry;

/* lexical tokens */
enum {
    T_NONE,
    _T_FIRST_KEYWORD = 0x100,
    T_DEF = _T_FIRST_KEYWORD,
    T_VAR,
    T_IF,
    T_ELSE,
    T_FOR,
    T_DO,
    T_WHILE,
    T_CONTINUE,
    T_BREAK,
    T_RETURN,
    T_OBJECT,
    T_METHOD,
    T_SHARED,
    T_SUPER,
    T_PRINT,
    _T_NON_KEYWORDS,
    T_LE = _T_NON_KEYWORDS, /* '<=' */
    T_EQ,                   /* '==' */
    T_NE,                   /* '!=' */
    T_GE,                   /* '>=' */
    T_SHL,                  /* '<<' */
    T_SHR,                  /* '>>' */
    T_AND,                  /* '&&' */
    T_OR,                   /* '||' */
    T_INC,                  /* '++' */
    T_DEC,                  /* '--' */
    T_ADDEQ,                /* '+=' */
    T_SUBEQ,                /* '-=' */
    T_MULEQ,                /* '*=' */
    T_DIVEQ,                /* '/=' */
    T_REMEQ,                /* '%=' */
    T_ANDEQ,                /* '&=' */
    T_OREQ,                 /* '|=' */
    T_XOREQ,                /* '^=' */
    T_SHLEQ,                /* '<<=' */
    T_SHREQ,                /* '>>=' */
    T_IDENTIFIER,
    T_NUMBER,
    T_STRING,
    T_EOL,
    T_EOF,
    _T_MAX
};

/* block type */
typedef enum {
    BLOCK_FOR,
    BLOCK_WHILE,
    BLOCK_DO
} BlockType;

/* block structure */
typedef struct Block Block;
struct Block {
    BlockType type;
    int cont;
    int contDefined;
    int nxt;
    int end;
    Block *next;
};

typedef struct IncludedFile IncludedFile;
typedef struct ParseFile ParseFile;

/* current include file */
typedef struct {
    IncludedFile *file;
    void *fp;
} CurrentIncludeFile;

/* parse file */
struct ParseFile {
    ParseFile *next;            /* next file in stack */
    CurrentIncludeFile file;
    int lineNumber;             /* current line number */
};

/* included file */
struct IncludedFile {
    IncludedFile *next;         /* next included file */
    char name[1];               /* file name */
};

/* parse context */
typedef struct {
    jmp_buf errorTarget;            /* error target */
    ParseFile *currentFile;         /* scan - current input file */
    IncludedFile *includedFiles;    /* scan - list of files that have already been included */
    IncludedFile *currentInclude;   /* scan - file currently being included */
    char lineBuf[MAXLINE];          /* scan - current input line */
    char *linePtr;                  /* scan - pointer to the current character */
    int savedToken;                 /* scan - lookahead token */
    int tokenOffset;                /* scan - offset to the start of the current token */
    char token[MAXTOKEN];           /* scan - current token string */
    VMVALUE value;                  /* scan - current token integer value */
    int inComment;                  /* scan - inside of a slash/star comment */
    SymbolTable globals;            /* parse - global symbol table */
    String *strings;                /* parse - string constants */
    ParseTreeNode *function;        /* parse - current function being parsed */
    Block *block;                   /* generate - current loop block */
    uint8_t codeBuf[MAXCODE];       /* code buffer */
    uint8_t *codeFree;              /* next available code location */
    uint8_t *codeTop;               /* top of code buffer */
} ParseContext;

/* partial value type codes */
typedef enum {
    VT_RVALUE,
    VT_LVALUE
} PVAL;

/* partial value function codes */
enum {
    PV_LOAD,
    PV_STORE
};

/* parse tree node types */
enum {
    NodeTypeFunctionDef,
    NodeTypeIf,
    NodeTypeWhile,
    NodeTypeDoWhile,
    NodeTypeFor,
    NodeTypeReturn,
    NodeTypeBreak,
    NodeTypeContinue,
    NodeTypeBlock,
    NodeTypeExpr,
    NodeTypeEmpty,
    NodeTypePrint,
    NodeTypeGlobalSymbolRef,
    NodeTypeLocalSymbolRef,
    NodeTypeArgumentRef,
    NodeTypeStringLit,
    NodeTypeIntegerLit,
    NodeTypeFunctionLit,
    NodeTypePreincrementOp,
    NodeTypePostincrementOp,
    NodeTypeUnaryOp,
    NodeTypeBinaryOp,
    NodeTypeAssignmentOp,
    NodeTypeArrayRef,
    NodeTypeFunctionCall,
    NodeTypeSend,
    NodeTypePropertyRef,
    NodeTypeDisjunction,
    NodeTypeConjunction
};

/* node list entry structure */
struct NodeListEntry {
    ParseTreeNode *node;
    NodeListEntry *next;
};

/* print operation structure */
typedef struct PrintOp PrintOp;
struct PrintOp {
    PrintOp *next;
    int trap;
    ParseTreeNode *expr;
};

/* parse tree node structure */
struct ParseTreeNode {
    int nodeType;
    union {
        struct {
            SymbolTable arguments;
            SymbolTable locals;
            ParseTreeNode *body;
        } functionDef;
        struct {
            ParseTreeNode *test;
            ParseTreeNode *thenStatement;
            ParseTreeNode *elseStatement;
        } ifStatement;
        struct {
            ParseTreeNode *test;
            ParseTreeNode *body;
        } whileStatement;
        struct {
            ParseTreeNode *body;
            ParseTreeNode *test;
        } doWhileStatement;
        struct {
            ParseTreeNode *init;
            ParseTreeNode *test;
            ParseTreeNode *incr;
            ParseTreeNode *body;
        } forStatement;
        struct {
            ParseTreeNode *value;
        } returnStatement;
        struct {
            NodeListEntry *statements;
        } blockStatement;
        struct {
            ParseTreeNode *expr;
        } exprStatement;
        struct {
            PrintOp *ops;
        } printStatement;
        struct {
            int valueType;
            Symbol *symbol;
            int offset;
        } symbolRef;
        struct {
            String *string;
        } stringLit;
        struct {
            VMVALUE value;
        } integerLit;
        struct {
            int offset;
        } functionLit;
        struct {
            int op;
            ParseTreeNode *expr;
        } unaryOp;
        struct {
            int increment;
            ParseTreeNode *expr;
        } incrementOp;
        struct {
            int op;
            ParseTreeNode *left;
            ParseTreeNode *right;
        } binaryOp;
        struct {
            ParseTreeNode *array;
            ParseTreeNode *index;
        } arrayRef;
        struct {
            ParseTreeNode *fcn;
            NodeListEntry *args;
            int argc;
        } functionCall;
        struct {
            ParseTreeNode *object;
            ParseTreeNode *selector;
            NodeListEntry *args;
            int argc;
        } send;
        struct {
            ParseTreeNode *object;
            ParseTreeNode *property;
        } propertyRef;
        struct {
            NodeListEntry *exprs;
        } exprList;
    } u;
};

/* adv2com.c */
void Abort(ParseContext *c, const char *fmt, ...);

String *AddString(ParseContext *c, char *value);
void *LocalAlloc(ParseContext *c, size_t size);

/* adv2parse.c */
ParseTreeNode *ParseFunction(ParseContext *c);

/* adv2scan.c */
void InitScan(ParseContext *c);
int PushFile(ParseContext *c, const char *name);
void FRequire(ParseContext *c, int requiredToken);
void Require(ParseContext *c, int token, int requiredToken);
int GetToken(ParseContext *c);
void SaveToken(ParseContext *c, int token);
char *TokenName(int token);
int SkipSpaces(ParseContext *c);
int GetChar(ParseContext *c);
void UngetC(ParseContext *c);
int GetLine(ParseContext *c);
void ParseError(ParseContext *c, char *fmt, ...);

/* db_symbols.c */
void InitSymbolTable(SymbolTable *table);
Symbol *AddGlobal(ParseContext *c, const char *name, StorageClass storageClass, VMVALUE value);
Symbol *AddArgument(ParseContext *c, const char *name, StorageClass storageClass, int value);
Symbol *AddLocal(ParseContext *c, const char *name, StorageClass storageClass, int value);
Symbol *FindSymbol(SymbolTable *table, const char *name);
int IsConstant(Symbol *symbol);

/* db_generate.c */
void code_functiondef(ParseContext *c, ParseTreeNode *expr);

#endif

