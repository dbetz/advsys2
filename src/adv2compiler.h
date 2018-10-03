/* adv2compiler.h - definitions for a simple basic compiler
 *
 * Copyright (c) 2018 by David Michael Betz.  All rights reserved.
 *
 */

#ifndef __ADV2COMPILER_H__
#define __ADV2COMPILER_H__

#include <stdio.h>
#include <setjmp.h>
#include "adv2types.h"
#include "adv2image.h"

#define K               1024

/* program limits */
#define MAXLINE         128
#define MAXTOKEN        32
#define MAXCODE         (32*K)
#define MAXDATA         (64*K)
#define MAXSTRING       (256*K)

/* forward type declarations */
typedef struct ParseTreeNode ParseTreeNode;
typedef struct NodeListEntry NodeListEntry;

/* lexical tokens */
enum {
    T_NONE,
    _T_FIRST_KEYWORD = 0x100,
    T_INCLUDE = _T_FIRST_KEYWORD,
    T_DEF,
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
    T_CLASS,
    T_PROPERTY,
    T_METHOD,
    T_SHARED,
    T_SUPER,
    T_TRY,
    T_CATCH,
    T_FINALLY,
    T_THROW,
    T_BYTE,
    T_ASM,
    T_PRINT,
    T_PRINTLN,
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
    BLOCK_FOR = 1,
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

/* storage class ids */
typedef enum {
    SC_CONSTANT = 1,
    SC_VARIABLE,
    SC_OBJECT,
    SC_FUNCTION
} StorageClass;

/* forward type declarations */
typedef struct Symbol Symbol;
typedef struct String String;
typedef struct Fixup Fixup;

/* symbol table */
typedef struct {
    Symbol *head;
    Symbol **pTail;
} SymbolTable;

/* symbol structure */
struct Symbol {
    Symbol *next;
    StorageClass storageClass;
    int valueDefined;
    union {
        Fixup *fixups;
        VMVALUE value;
    } v;
    char name[1];
};

/* fixup types */
typedef enum {
    FT_DATA,
    FT_CODE,
    FT_PTR
} FixupType;

/* fixup structure */
struct Fixup {
    Fixup *next;
    FixupType type;
    union {
        VMVALUE offset;
        VMVALUE *pOffset;
    } v;
};

/* forward type declarations */
typedef struct LocalSymbol LocalSymbol;

/* local symbol table */
typedef struct {
    LocalSymbol *head;
    LocalSymbol **pTail;
    int count;
} LocalSymbolTable;

/* local symbol structure */
struct LocalSymbol {
    LocalSymbol *next;
    ParseTreeNode *initialValue; // not used for arguments
    int offset;
    char name[1];
};

/* symbol data fixup structure */
typedef struct SymbolDataFixup SymbolDataFixup;
struct SymbolDataFixup {
    SymbolDataFixup *next;
    Symbol *symbol;
    VMVALUE offset;
};

/* string data fixup structure */
typedef struct StringDataFixup StringDataFixup;
struct StringDataFixup {
    StringDataFixup *next;
    String *string;
    VMVALUE offset;
};

/* data block structure */
typedef struct DataBlock DataBlock;
struct DataBlock {
    DataBlock *next;
    DataBlock *parent;
    SymbolDataFixup *symbolFixups;
    StringDataFixup *stringFixups;
    VMVALUE parentOffset;
    VMVALUE offset;
    int size;
    uint8_t *data;
};

/* string structure */
struct String {
    String *next;
    Fixup *fixups;
    int offset;
    int size;
    char data[1];
};

/* object list structure */
typedef struct ObjectListEntry ObjectListEntry;
struct ObjectListEntry {
    ObjectListEntry *next;
    VMVALUE object;
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

/* word structure */
typedef struct Word Word;
struct Word {
    Word *next;
    int type;
    String *string;
};

/* word type structure */
typedef struct {
    char *name;
    int type;
} WordType;

/* parse context */
typedef struct {
    jmp_buf errorTarget;                            /* error target */
    ParseFile *currentFile;                         /* scan - current input file */
    IncludedFile *includedFiles;                    /* scan - list of files that have already been included */
    IncludedFile *currentInclude;                   /* scan - file currently being included */
    char lineBuf[MAXLINE];                          /* scan - current input line */
    char *linePtr;                                  /* scan - pointer to the current character */
    int savedToken;                                 /* scan - lookahead token */
    int tokenOffset;                                /* scan - offset to the start of the current token */
    char token[MAXTOKEN];                           /* scan - current token string */
    VMVALUE value;                                  /* scan - current token integer value */
    int inComment;                                  /* scan - inside of a slash/star comment */
    SymbolTable globals;                            /* parse - global symbol table */
    String *strings;                                /* parse - string constants */
    String **pNextString;                           /* parse - place to store next string constant */
    ObjectListEntry *objects;                       /* parse - list of objects for parent/sibling/child linking */
    VMVALUE parentProperty;                         /* parse - parent property tag */
    VMVALUE siblingProperty;                        /* parse - sibling property tag */
    VMVALUE childProperty;                          /* parse - child property tag */
    Symbol *currentObjectSymbol;                    /* parse - current object being parsed */
    ParseTreeNode *currentFunction;                 /* parse - current function being parsed */
    LocalSymbol *trySymbols;                        /* parse - stack of try catch symbols */
    int currentTryDepth;                            /* parse - current depth of try statements */
    Block *block;                                   /* generate - current loop block */
    int propertyCount;                              /* property count */
    int dataDepth;                                  /* depth of data block nesting */
    DataBlock *dataBlocks;                          /* list of data blocks */
    DataBlock **pNextDataBlock;                     /* place to store next data block */
    uint8_t codeBuf[MAXCODE];                       /* code buffer */
    uint8_t *codeFree;                              /* next available code location */
    uint8_t *codeTop;                               /* top of code buffer */
    uint8_t dataBuf[MAXDATA];                       /* data buffer */
    uint8_t *dataFree;                              /* next available data location */
    uint8_t *dataTop;                               /* top of data buffer */
    uint8_t stringBuf[MAXSTRING];                   /* string buffer */
    uint8_t *stringFree;                            /* next available string location */
    uint8_t *stringTop;                             /* top of string buffer */
    Symbol *wordsSymbol;                            /* symbol table entry for '_words' */
    Symbol *wordTypesSymbol;                        /* symbol table entry for '_wordTypes' */
    Word *words;                                    /* list of words */
    Word **pNextWord;                               /* place to store next word */
    int wordCount;                                  /* number of words */
    int wordType;                                   /* word type of current property */
    int debugMode;                                  /* debug mode flag */
} ParseContext;

/* partial value function codes */
typedef enum {
    PVF_LOAD,
    PVF_STORE
} PvFcn;

/* partial value typess */
typedef enum {
    PVT_LONG,
    PVT_BYTE
} PvType;

/* partial value structure */
typedef struct PVAL PVAL;
struct PVAL {
    void (*fcn)(ParseContext *c, PvFcn fcn, PVAL *pv);
    PvType type;
    VMVALUE val;
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
    NodeTypeTry,
    NodeTypeThrow,
    NodeTypeExpr,
    NodeTypeAsm,
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
    NodeTypeCommaOp,
    NodeTypeUnaryOp,
    NodeTypeBinaryOp,
    NodeTypeTernaryOp,
    NodeTypeAssignmentOp,
    NodeTypeArrayRef,
    NodeTypeFunctionCall,
    NodeTypeMethodCall,
    NodeTypeClassRef,
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
            char *name;
            LocalSymbolTable arguments;
            LocalSymbolTable locals;
            int maximumTryDepth;
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
            ParseTreeNode *statement;
            LocalSymbol *catchSymbol;
            ParseTreeNode *catchStatement;
            ParseTreeNode *finallyStatement;
        } tryStatement;
        struct {
            ParseTreeNode *expr;
        } throwStatement;
        struct {
            ParseTreeNode *expr;
        } exprStatement;
        struct {
            uint8_t *code;
            int length;
        } asmStatement;
        struct {
            PrintOp *ops;
        } printStatement;
        struct {
            Symbol *symbol;
        } symbolRef;
        struct {
            LocalSymbol *symbol;
        } localSymbolRef;
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
            ParseTreeNode *left;
            ParseTreeNode *right;
        } commaOp;
        struct {
            int op;
            ParseTreeNode *left;
            ParseTreeNode *right;
        } binaryOp;
        struct {
            ParseTreeNode *test;
            ParseTreeNode *thenExpr;
            ParseTreeNode *elseExpr;
        } ternaryOp;
        struct {
            ParseTreeNode *array;
            ParseTreeNode *index;
            PvType type;
        } arrayRef;
        struct {
            ParseTreeNode *fcn;
            NodeListEntry *args;
            int argc;
        } functionCall;
        struct {
            ParseTreeNode *class;
            ParseTreeNode *object;
            ParseTreeNode *selector;
            NodeListEntry *args;
            int argc;
        } methodCall;
        struct {
            ParseTreeNode *object;
        } classRef;
        struct {
            ParseTreeNode *object;
            ParseTreeNode *selector;
        } propertyRef;
        struct {
            NodeListEntry *exprs;
        } exprList;
    } u;
};

/* adv2com.c */
int FindObject(ParseContext *c, const char *name);
VMVALUE AddProperty(ParseContext *c, const char *name);
void AddObject(ParseContext *c, VMVALUE object);
void InitSymbolTable(ParseContext *c);
Symbol *AddGlobal(ParseContext *c, const char *name, StorageClass storageClass, VMVALUE value);
int AddSymbolRef(ParseContext *c, Symbol *symbol, FixupType fixupType, VMVALUE offset);
Symbol *AddUndefinedSymbol(ParseContext *c, const char *name, StorageClass storageClass);
Symbol *AddSymbol(ParseContext *c, const char *name, StorageClass storageClass, int value);
Symbol *FindSymbol(ParseContext *c, const char *name);
void PrintSymbols(ParseContext *c);
void Abort(ParseContext *c, const char *fmt, ...);
void AddStringRef(ParseContext *c, String *string, FixupType fixupType, VMVALUE offset);
void AddStringPtrRef(ParseContext *c, String *string, VMVALUE *pOffset);
String *AddString(ParseContext *c, char *value);
void *LocalAlloc(ParseContext *c, size_t size);

/* adv2parse.c */
void ParseDeclarations(ParseContext *c);
void PrintLocalSymbols(LocalSymbolTable *table, char *tag, int indent);
void StoreInitializer(ParseContext *c, VMVALUE value);

/* adv2pasm.c */
int PasmAssemble1(char *line, uint32_t *pValue);

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

/* adv2gen.c */
uint8_t *code_functiondef(ParseContext *c, ParseTreeNode *expr, int *pLength);
int putcbyte(ParseContext *c, int v);
int putcword(ParseContext *c, VMWORD v);
int putclong(ParseContext *c, VMVALUE v);
void wr_clong(ParseContext *c, VMUVALUE off, VMVALUE v);

#endif

