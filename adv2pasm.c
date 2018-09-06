#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>

#ifndef TRUE
#define TRUE        1
#define FALSE       0
#endif

#define MAXLINE     1024
#define MAXTOKEN    32

typedef int32_t VMVALUE;
typedef uint32_t VMUVALUE;

/* lexical tokens */
enum {
    T_NONE,
    T_LE,       /* non-keyword tokens */
    T_NE,
    T_GE,
    T_SHL,
    T_SHR,
    T_IDENTIFIER,
    T_NUMBER,
    T_STRING,
    T_EOL,
    T_EOF
};

/* instruction layout */
#define OPCODE_SHIFT    26
#define Z_SHIFT         25
#define C_SHIFT         24
#define R_SHIFT         23
#define I_SHIFT         22
#define COND_SHIFT      18
#define DST_SHIFT       9
#define DST_MASK        (0x1ff << DST_SHIFT)
#define SRC_SHIFT       0
#define SRC_MASK        (0x1ff << SRC_SHIFT)

/* forward types */
typedef struct Symbol Symbol;

/* symbol table */
typedef struct {
    Symbol *head;
    Symbol **pTail;
} SymbolTable;

/* symbol types */
typedef enum {
    SYMBOL_UNDEF,
    SYMBOL_VALUE
} SymbolType;

/* symbol structure */
struct Symbol {
    Symbol *next;
    SymbolType type;
    VMVALUE value;
    char name[1];
};

typedef void RewindFcn(void *cookie);
typedef int GetLineFcn(void *cookie, char *buf, int len, int *pLineNumber);

/* parse context */
typedef struct {
    jmp_buf errorTarget;            /* error target */
    RewindFcn *rewind;              /* scan - function to rewind to the start of the source program */
    GetLineFcn *getLine;            /* scan - function to get a line from the source program */
    void *getLineCookie;            /* scan - cookie for the rewind and getLine functions */
    char lineBuf[MAXLINE];          /* scan - line buffer */
    char *linePtr;                  /* scan - pointer to the current character */
    int lineNumber;                 /* scan - current line number */
    int savedToken;               /* scan - lookahead token */
    int tokenOffset;                /* scan - offset to the start of the current token */
    char token[MAXTOKEN];           /* scan - current token string */
    VMVALUE value;                  /* scan - current token integer value */
    int inComment;                  /* scan - inside of a slash/star comment */
    SymbolTable globals;            /* global symbol table */
    SymbolTable locals;             /* local symbol table */
    int pass;                       /* current pass number */
} ParseContext;

/* operand types */
enum {
    OPERANDS_NONE,
    OPERANDS_BOTH,
    OPERANDS_SRC,
    OPERANDS_DST,
    OPERANDS_CALL
};

/* opcode definition structure */
typedef struct {
    char *opname;
    int type;
    VMUVALUE template;
} OpDef;

/* opcode table */
static OpDef opcodeDefs[] = {
{   "nop",      OPERANDS_NONE,  (0x00<<OPCODE_SHIFT)|(0<<R_SHIFT)|(0<<I_SHIFT)|(0x0<<COND_SHIFT)                },
{   "wrbyte",   OPERANDS_BOTH,  (0x00<<OPCODE_SHIFT)|(0<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "rdbyte",   OPERANDS_BOTH,  (0x00<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "wrword",   OPERANDS_BOTH,  (0x01<<OPCODE_SHIFT)|(0<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "rdword",   OPERANDS_BOTH,  (0x01<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "wrlong",   OPERANDS_BOTH,  (0x02<<OPCODE_SHIFT)|(0<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "rdlong",   OPERANDS_BOTH,  (0x02<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "hubop",    OPERANDS_BOTH,  (0x03<<OPCODE_SHIFT)|(0<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "clkset",   OPERANDS_DST,   (0x03<<OPCODE_SHIFT)|(0<<R_SHIFT)|(1<<I_SHIFT)|(0xf<<COND_SHIFT)|(0<<SRC_SHIFT) },
{   "cogid",    OPERANDS_DST,   (0x03<<OPCODE_SHIFT)|(1<<R_SHIFT)|(1<<I_SHIFT)|(0xf<<COND_SHIFT)|(1<<SRC_SHIFT) },
{   "coginit",  OPERANDS_DST,   (0x03<<OPCODE_SHIFT)|(0<<R_SHIFT)|(1<<I_SHIFT)|(0xf<<COND_SHIFT)|(2<<SRC_SHIFT) },
{   "cogstop",  OPERANDS_DST,   (0x03<<OPCODE_SHIFT)|(0<<R_SHIFT)|(1<<I_SHIFT)|(0xf<<COND_SHIFT)|(3<<SRC_SHIFT) },
{   "locknew",  OPERANDS_DST,   (0x03<<OPCODE_SHIFT)|(1<<R_SHIFT)|(1<<I_SHIFT)|(0xf<<COND_SHIFT)|(4<<SRC_SHIFT) },
{   "lockret",  OPERANDS_DST,   (0x03<<OPCODE_SHIFT)|(0<<R_SHIFT)|(1<<I_SHIFT)|(0xf<<COND_SHIFT)|(5<<SRC_SHIFT) },
{   "lockset",  OPERANDS_DST,   (0x03<<OPCODE_SHIFT)|(0<<R_SHIFT)|(1<<I_SHIFT)|(0xf<<COND_SHIFT)|(6<<SRC_SHIFT) },
{   "lockclr",  OPERANDS_DST,   (0x03<<OPCODE_SHIFT)|(0<<R_SHIFT)|(1<<I_SHIFT)|(0xf<<COND_SHIFT)|(7<<SRC_SHIFT) },
{   "mul",      OPERANDS_BOTH,  (0x04<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "muls",     OPERANDS_BOTH,  (0x05<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "enc",      OPERANDS_BOTH,  (0x06<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "ones",     OPERANDS_BOTH,  (0x07<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "ror",      OPERANDS_BOTH,  (0x08<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "rol",      OPERANDS_BOTH,  (0x09<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "shr",      OPERANDS_BOTH,  (0x0a<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "shl",      OPERANDS_BOTH,  (0x0b<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "rcr",      OPERANDS_BOTH,  (0x0c<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "rcl",      OPERANDS_BOTH,  (0x0d<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "sar",      OPERANDS_BOTH,  (0x0e<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "rev",      OPERANDS_BOTH,  (0x0f<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "mins",     OPERANDS_BOTH,  (0x10<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "maxs",     OPERANDS_BOTH,  (0x11<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "min",      OPERANDS_BOTH,  (0x12<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "max",      OPERANDS_BOTH,  (0x13<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },   
{   "movs",     OPERANDS_BOTH,  (0x14<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "movd",     OPERANDS_BOTH,  (0x15<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "movi",     OPERANDS_BOTH,  (0x16<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "jmp",      OPERANDS_SRC,   (0x17<<OPCODE_SHIFT)|(0<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "jmpret",   OPERANDS_BOTH,  (0x17<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "call",     OPERANDS_CALL,  (0x17<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "ret",      OPERANDS_NONE,  (0x17<<OPCODE_SHIFT)|(0<<R_SHIFT)|(1<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "test",     OPERANDS_BOTH,  (0x18<<OPCODE_SHIFT)|(0<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "and",      OPERANDS_BOTH,  (0x18<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "testn",    OPERANDS_BOTH,  (0x19<<OPCODE_SHIFT)|(0<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "andn",     OPERANDS_BOTH,  (0x19<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "or",       OPERANDS_BOTH,  (0x1a<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "xor",      OPERANDS_BOTH,  (0x1b<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "muxc",     OPERANDS_BOTH,  (0x1c<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "muxnc",    OPERANDS_BOTH,  (0x1d<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "muxz",     OPERANDS_BOTH,  (0x1e<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "muxnz",    OPERANDS_BOTH,  (0x1f<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "add",      OPERANDS_BOTH,  (0x20<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "cmp",      OPERANDS_BOTH,  (0x21<<OPCODE_SHIFT)|(0<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "sub",      OPERANDS_BOTH,  (0x21<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "addabs",   OPERANDS_BOTH,  (0x22<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "subabs",   OPERANDS_BOTH,  (0x23<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "sumc",     OPERANDS_BOTH,  (0x24<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "sumnc",    OPERANDS_BOTH,  (0x25<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "sumz",     OPERANDS_BOTH,  (0x26<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "sumnz",    OPERANDS_BOTH,  (0x27<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "mov",      OPERANDS_BOTH,  (0x28<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "neg",      OPERANDS_BOTH,  (0x29<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "abs",      OPERANDS_BOTH,  (0x2a<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "absneg",   OPERANDS_BOTH,  (0x2b<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "negc",     OPERANDS_BOTH,  (0x2c<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "negnc",    OPERANDS_BOTH,  (0x2d<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "negz",     OPERANDS_BOTH,  (0x2e<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "negnz",    OPERANDS_BOTH,  (0x2f<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "cmps",     OPERANDS_BOTH,  (0x30<<OPCODE_SHIFT)|(0<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "cmpsx",    OPERANDS_BOTH,  (0x31<<OPCODE_SHIFT)|(0<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "addx",     OPERANDS_BOTH,  (0x32<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "subx",     OPERANDS_BOTH,  (0x33<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "adds",     OPERANDS_BOTH,  (0x34<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "subs",     OPERANDS_BOTH,  (0x35<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "addsx",    OPERANDS_BOTH,  (0x36<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "subsx",    OPERANDS_BOTH,  (0x37<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "cmpsub",   OPERANDS_BOTH,  (0x38<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "djnz",     OPERANDS_BOTH,  (0x39<<OPCODE_SHIFT)|(1<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "tjnz",     OPERANDS_BOTH,  (0x3a<<OPCODE_SHIFT)|(0<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "tjz",      OPERANDS_BOTH,  (0x3b<<OPCODE_SHIFT)|(0<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "waitpeq",  OPERANDS_BOTH,  (0x3c<<OPCODE_SHIFT)|(0<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "waitpne",  OPERANDS_BOTH,  (0x3d<<OPCODE_SHIFT)|(0<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "waitcnt",  OPERANDS_BOTH,  (0x3e<<OPCODE_SHIFT)|(0<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   "waitvid",  OPERANDS_BOTH,  (0x3f<<OPCODE_SHIFT)|(0<<R_SHIFT)|(0<<I_SHIFT)|(0xf<<COND_SHIFT)                },
{   NULL,       0,              0                                                                               }
};

/* keyword table */
static struct {
    char *keyword;
    int token;
} asmktab[] = {
{   NULL,           0       }
};

/* keyword types */
enum {
    FIELD_COND,
    FIELD_EFFECT
};

/* keyword table */
typedef struct {
    char *keyword;
    int type;
    VMUVALUE value;
    VMUVALUE mask;
} FieldDef;

static FieldDef fieldDefs[] = {
{   "if_always",    FIELD_COND,     0xf<<COND_SHIFT,    0xf<<COND_SHIFT },  // always
{   "if_never",     FIELD_COND,     0x0<<COND_SHIFT,    0xf<<COND_SHIFT },  // never
{   "if_e",         FIELD_COND,     0xa<<COND_SHIFT,    0xf<<COND_SHIFT },  // if equal (Z == 1)
{   "if_ne",        FIELD_COND,     0x5<<COND_SHIFT,    0xf<<COND_SHIFT },  // if not equal (Z == 0)
{   "if_a",         FIELD_COND,     0x1<<COND_SHIFT,    0xf<<COND_SHIFT },  // if above (!C && !Z == 1)
{   "if_b",         FIELD_COND,     0xc<<COND_SHIFT,    0xf<<COND_SHIFT },  // if below (C == 1)
{   "if_ae",        FIELD_COND,     0x3<<COND_SHIFT,    0xf<<COND_SHIFT },  // if above or equal (C == 0)
{   "if_be",        FIELD_COND,     0xe<<COND_SHIFT,    0xf<<COND_SHIFT },  // if below or equal (C == 1 || Z == 1)
{   "if_c",         FIELD_COND,     0xc<<COND_SHIFT,    0xf<<COND_SHIFT },  // if C set
{   "if_nc",        FIELD_COND,     0x3<<COND_SHIFT,    0xf<<COND_SHIFT },  // if C clear
{   "if_z",         FIELD_COND,     0xa<<COND_SHIFT,    0xf<<COND_SHIFT },  // if Z set
{   "if_nz",        FIELD_COND,     0x5<<COND_SHIFT,    0xf<<COND_SHIFT },  // if Z clear
{   "if_c_eq_z",    FIELD_COND,     0x9<<COND_SHIFT,    0xf<<COND_SHIFT },  // if C equal to Z
{   "if_c_ne_z",    FIELD_COND,     0x6<<COND_SHIFT,    0xf<<COND_SHIFT },  // if C not equal to Z
{   "if_c_and_z",   FIELD_COND,     0x8<<COND_SHIFT,    0xf<<COND_SHIFT },  // if C set and Z set
{   "if_c_and_nz",  FIELD_COND,     0x4<<COND_SHIFT,    0xf<<COND_SHIFT },  // if C set and Z clear
{   "if_nc_and_z",  FIELD_COND,     0x2<<COND_SHIFT,    0xf<<COND_SHIFT },  // if C clear and Z set
{   "if_nc_and_nz", FIELD_COND,     0x1<<COND_SHIFT,    0xf<<COND_SHIFT },  // if C clear and Z clear
{   "if_c_or_z",    FIELD_COND,     0xe<<COND_SHIFT,    0xf<<COND_SHIFT },  // if C set or Z set
{   "if_c_or_nz",   FIELD_COND,     0xd<<COND_SHIFT,    0xf<<COND_SHIFT },  // if C set or Z clear
{   "if_nc_or_z",   FIELD_COND,     0xb<<COND_SHIFT,    0xf<<COND_SHIFT },  // if C clear or Z set
{   "if_nc_or_nz",  FIELD_COND,     0x1<<COND_SHIFT,    0xf<<COND_SHIFT },  // if C clear or Z clear
{   "if_z_eq_c",    FIELD_COND,     0x9<<COND_SHIFT,    0xf<<COND_SHIFT },  // if Z equal to C
{   "if_z_ne_c",    FIELD_COND,     0x6<<COND_SHIFT,    0xf<<COND_SHIFT },  // if Z not equal to C
{   "if_z_and_c",   FIELD_COND,     0x8<<COND_SHIFT,    0xf<<COND_SHIFT },  // if Z set and C set
{   "if_z_and_nc",  FIELD_COND,     0x2<<COND_SHIFT,    0xf<<COND_SHIFT },  // if Z set and C clear
{   "if_nz_and_c",  FIELD_COND,     0x4<<COND_SHIFT,    0xf<<COND_SHIFT },  // if Z clear and C set
{   "if_nz_and_nc", FIELD_COND,     0x1<<COND_SHIFT,    0xf<<COND_SHIFT },  // if Z clear and C clear
{   "if_z_or_c",    FIELD_COND,     0xe<<COND_SHIFT,    0xf<<COND_SHIFT },  // if Z set or C set
{   "if_z_or_nc",   FIELD_COND,     0xb<<COND_SHIFT,    0xf<<COND_SHIFT },  // if Z set or C clear
{   "if_nz_or_c",   FIELD_COND,     0xd<<COND_SHIFT,    0xf<<COND_SHIFT },  // if Z clear or C set
{   "if_nz_or_nc",  FIELD_COND,     0x7<<COND_SHIFT,    0xf<<COND_SHIFT },  // if Z clear or C clear
{   "wz",           FIELD_EFFECT,   0x1<<Z_SHIFT,       0x1<<Z_SHIFT    },  // write Z
{   "wc",           FIELD_EFFECT,   0x1<<C_SHIFT,       0x1<<C_SHIFT    },  // write C
{   "wr",           FIELD_EFFECT,   0x1<<R_SHIFT,       0x1<<R_SHIFT    },  // write result
{   "nr",           FIELD_EFFECT,   0x0<<R_SHIFT,       0x1<<R_SHIFT    },  // no result
{   NULL,           0,              0,                  0               }
};

static int Assemble(ParseContext *c);
static void ParseFile(ParseContext *c, int pass);
static VMVALUE ParseExpr(ParseContext *c);
static VMVALUE ParseExpr2(ParseContext *c);
static VMVALUE ParseExpr3(ParseContext *c);
static VMVALUE ParseExpr4(ParseContext *c);
static VMVALUE ParseExpr5(ParseContext *c);
static VMVALUE ParseExpr6(ParseContext *c);
static VMVALUE ParseExpr7(ParseContext *c);
static VMVALUE ParsePrimary(ParseContext *c);
static int GetLine(ParseContext *c);
static void FRequire(ParseContext *c, int requiredToken);
static void Require(ParseContext *c, int token, int requiredToken);
static char *TokenName(int token);
static int GetIdentifier(ParseContext *c, char *errtext);
static int GetToken(ParseContext *c);
static void SaveToken(ParseContext *c, int token);
static int NextToken(ParseContext *c);
static int IdentifierToken(ParseContext *c, int ch);
static int LiteralChar(ParseContext *c);
static int SkipComment(ParseContext *c);
static int XGetC(ParseContext *c);
static int SkipSpaces(ParseContext *c);
static int GetChar(ParseContext *c);
static void UngetC(ParseContext *c);
static int IdentifierCharP(int ch);
static int NumberToken(ParseContext *c, int ch);
static int HexNumberToken(ParseContext *c);
static int BinaryNumberToken(ParseContext *c);
static int StringToken(ParseContext *c);
static int CharToken(ParseContext *c);
static void ParseError(ParseContext *c, char *fmt, ...);
static OpDef *FindOpcode(char *name);
static FieldDef *FindField(char *name);
static void InitSymbolTable(SymbolTable *table);
static Symbol *AddSymbol(ParseContext *c, const char *name, SymbolType type, VMVALUE value);
static Symbol *FindSymbol(ParseContext *c, const char *name);
static void EmptySymbolTable(SymbolTable *table);
#ifdef MAIN
static void DumpSymbols(SymbolTable *table);
#endif
static void putcword(ParseContext *c, VMVALUE w);

static void SourceRewind(void *cookie);
static int SourceGetLine(void *cookie, char *buf, int len, int *pLineNumber);

#ifdef MAIN

typedef struct {
    FILE *fp;
    int lineNumber;
} SourceInfo;

int main(int argc, char *argv[])
{
    ParseContext context;
    SourceInfo sourceInfo;
    
    /* check the argument count */
    if (argc != 2) {
        fprintf(stderr, "usage: pasm <infile>\n");
        return 1;
    }
    
    /* open the input file */
    if (!(sourceInfo.fp = fopen(argv[1], "r"))) {
        fprintf(stderr, "error: can't open '%s'\n", argv[1]);
        return 1;
    }
    sourceInfo.lineNumber = 0;
    
    /* initialize the parse context */
    memset(&context, 0, sizeof(context));
    
    /* setup the parser callbacks */
    context.rewind = SourceRewind;
    context.getLine = SourceGetLine;
    context.getLineCookie = &sourceInfo;
    
    /* assemble the file */
    Assemble(&context);
    
    /* close the source file */
    fclose(sourceInfo.fp);
    
    return 0;
}

static void SourceRewind(void *cookie)
{
    SourceInfo *info = (SourceInfo *)cookie;
    fseek(info->fp, 0, SEEK_SET);
}

static int SourceGetLine(void *cookie, char *buf, int len, int *pLineNumber)
{
    SourceInfo *info = (SourceInfo *)cookie;
    *pLineNumber = ++info->lineNumber;
	return fgets(buf, len, info->fp) != NULL;
}

static void putcword(ParseContext *c, VMVALUE w)
{
    printf("%08x\n", w);
}

#else

typedef struct {
    char *line;
    int lineNumber;
    int eof;
    uint32_t *pValue;
} SourceInfo;

int PasmAssemble1(char *line, uint32_t *pValue)
{
    ParseContext context;
    SourceInfo sourceInfo;

    /* setup the source */
    sourceInfo.line = line;
    sourceInfo.lineNumber = 1;
    sourceInfo.eof = FALSE;
    sourceInfo.pValue = pValue;

    /* initialize the parse context */
    memset(&context, 0, sizeof(context));
    
    /* setup the parser callbacks */
    context.rewind = SourceRewind;
    context.getLine = SourceGetLine;
    context.getLineCookie = &sourceInfo;
    
    /* assemble the line */
    return Assemble(&context);
}

static void SourceRewind(void *cookie)
{
    SourceInfo *info = (SourceInfo *)cookie;
    info->line = info->line;
    info->eof = FALSE;
}

static int SourceGetLine(void *cookie, char *buf, int len, int *pLineNumber)
{
    SourceInfo *info = (SourceInfo *)cookie;
    if (info->eof)
        return FALSE;
    strncpy(buf, info->line, len);
    buf[len - 1] = '\0';
    *pLineNumber = info->lineNumber;
    info->eof = TRUE;
	return TRUE;
}

static void putcword(ParseContext *c, VMVALUE w)
{
    SourceInfo *info = (SourceInfo *)c->getLineCookie;
    *info->pValue = w;
}

#endif

static int Assemble(ParseContext *c)
{
	/* setup an error target */
    if (setjmp(c->errorTarget) != 0)
        return FALSE;

    /* initialize the symbol tables */
    InitSymbolTable(&c->globals);
    InitSymbolTable(&c->locals);
    
    /* add the initial globals */
    AddSymbol(c, "dbase", SYMBOL_VALUE, 0x001);
    AddSymbol(c, "cbase", SYMBOL_VALUE, 0x002);
    AddSymbol(c, "sbase", SYMBOL_VALUE, 0x003);
    AddSymbol(c, "tos",   SYMBOL_VALUE, 0x004);
    AddSymbol(c, "sp",    SYMBOL_VALUE, 0x005);
    AddSymbol(c, "fp",    SYMBOL_VALUE, 0x006);
    AddSymbol(c, "pc",    SYMBOL_VALUE, 0x007);
    AddSymbol(c, "efp",   SYMBOL_VALUE, 0x008);
    AddSymbol(c, "t1",    SYMBOL_VALUE, 0x009);
    AddSymbol(c, "t2",    SYMBOL_VALUE, 0x00a);
    AddSymbol(c, "t3",    SYMBOL_VALUE, 0x00b);
    AddSymbol(c, "t4",    SYMBOL_VALUE, 0x00c);
    AddSymbol(c, "par",   SYMBOL_VALUE, 0x1f0);
    AddSymbol(c, "cnt",   SYMBOL_VALUE, 0x1f1);
    AddSymbol(c, "ina",   SYMBOL_VALUE, 0x1f2);
    AddSymbol(c, "inb",   SYMBOL_VALUE, 0x1f3);
    AddSymbol(c, "outa",  SYMBOL_VALUE, 0x1f4);
    AddSymbol(c, "outb",  SYMBOL_VALUE, 0x1f5);
    AddSymbol(c, "dira",  SYMBOL_VALUE, 0x1f6);
    AddSymbol(c, "dirb",  SYMBOL_VALUE, 0x1f7);
    AddSymbol(c, "ctra",  SYMBOL_VALUE, 0x1f8);
    AddSymbol(c, "ctrb",  SYMBOL_VALUE, 0x1f9);
    AddSymbol(c, "frqa",  SYMBOL_VALUE, 0x1fa);
    AddSymbol(c, "frqb",  SYMBOL_VALUE, 0x1fb);
    AddSymbol(c, "phsa",  SYMBOL_VALUE, 0x1fc);
    AddSymbol(c, "phsb",  SYMBOL_VALUE, 0x1fd);
    AddSymbol(c, "vcfg",  SYMBOL_VALUE, 0x1fe);
    AddSymbol(c, "vscl",  SYMBOL_VALUE, 0x1ff);
    
    ParseFile(c, 1);
    (*c->rewind)(c->getLineCookie);
    ParseFile(c, 2);
    
#ifdef MAIN
    DumpSymbols(&c->globals);
#endif
    
    return TRUE;
}

static void ParseFile(ParseContext *c, int pass)
{
    VMUVALUE lc = 0;
    
    /* store the pass number */
    c->pass = pass;
    
    /* get the next line */
    while (GetLine(c)) {
        char retLabel[MAXTOKEN + 4];
        Symbol *sym = NULL;
        FieldDef *fdef = NULL;
        OpDef *odef = NULL;
        VMUVALUE inst;
        int tkn;
        
        /* get a label, a conditional, or an opcode */
        if ((tkn = GetIdentifier(c, "expecting a label, a conditional, or an opcode")) != T_EOL) {
        
            /* check for an opcode */
            if (!(odef = FindOpcode(c->token))) {
            
                /* check for a conditional */
                if (!(fdef = FindField(c->token))) {
                
                    /* handle a label */
                    sym = AddSymbol(c, c->token, SYMBOL_VALUE, lc);
                    
                    /* get a conditional or an opcode */
                    if ((tkn = GetIdentifier(c, "expecting a conditional or an opcode")) == T_EOL)
                        continue;
                        
                    /* check again for an opcode */
                    if (!(odef = FindOpcode(c->token))) {
                    
                        /* check for a conditional */
                        if (!(fdef = FindField(c->token)))
                            ParseError(c, "Expecting a conditional or an opcode");
                            
                        /* get an opcode */
                        if ((tkn = GetIdentifier(c, "expecting an opcode")) == T_EOL || !(odef = FindOpcode(c->token)))
                            ParseError(c, "Expecting an opcode");
                    }
                }
                
                /* parse an opcode after a conditional */
                else {
                    if ((tkn = GetIdentifier(c, "expecting an opcode")) == T_EOL || !(odef = FindOpcode(c->token)))
                        ParseError(c, "Expecting an opcode");
                }
            }
        
            /* check the conditional */
            if (fdef && fdef->type != FIELD_COND)
                ParseError(c, "only conditionals are allowed before the opcode");
                
            /* get the instruction template */
            inst = odef->template;
            
            /* combine with the conditional */
            if (fdef)
                inst = (inst & ~fdef->mask) | fdef->value;
                
            /* parse the instruction operands */
            switch (odef->type) {
            case OPERANDS_NONE:
                /* nothing to do */
                break;
            case OPERANDS_BOTH:
                inst = (inst & ~DST_MASK) | ((ParseExpr(c) << DST_SHIFT) & DST_MASK);
                FRequire(c, ',');
                if ((tkn = GetToken(c)) == '#')
                    inst |= (1 << I_SHIFT);
                else
                    SaveToken(c, tkn);
                inst = (inst & ~SRC_MASK) | ((ParseExpr(c) << SRC_SHIFT) & SRC_MASK);
                break;
            case OPERANDS_SRC:
                if ((tkn = GetToken(c)) == '#')
                    inst |= (1 << I_SHIFT);
                else
                    SaveToken(c, tkn);
                inst = (inst & ~SRC_MASK) | ((ParseExpr(c) << SRC_SHIFT) & SRC_MASK);
                break;
            case OPERANDS_DST:
                inst = (inst & ~DST_MASK) | ((ParseExpr(c) << DST_SHIFT) & DST_MASK);
                break;
            case OPERANDS_CALL:
                FRequire(c, '#');
                FRequire(c, T_IDENTIFIER);
                sprintf(retLabel, "%s_ret", c->token);
                //ParseError(c, "CALL not yet implemented");
                break;
            }
            
            /* parse any flag operations */
            while ((tkn = GetIdentifier(c, "expecting effects")) != T_EOL) {
                if (!(fdef = FindField(c->token)) || fdef->type != FIELD_EFFECT)
                    ParseError(c, "expecting effects");
                inst = (inst & ~fdef->mask) | fdef->value;
                if ((tkn = GetToken(c)) != ',')
                    break;
            }
            Require(c, tkn, T_EOL);
            
            /* write out the instruction on the second pass */
            if (c->pass == 2)
                putcword(c, inst);
                
            /* update the location counter */
            ++lc;
        }
    }
}

/* ParseExpr - handle the BXOR operator */
static VMVALUE ParseExpr(ParseContext *c)
{
    VMVALUE expr;
    int tkn;
    expr = ParseExpr2(c);
    while ((tkn = GetToken(c)) == '^')
        expr = ParseExpr2(c);
    SaveToken(c,tkn);
    return expr;
}

/* ParseExpr2 - handle the BOR operator */
static VMVALUE ParseExpr2(ParseContext *c)
{
    VMVALUE expr;
    int tkn;
    expr = ParseExpr3(c);
    while ((tkn = GetToken(c)) == '|')
        expr |= ParseExpr3(c);
    SaveToken(c,tkn);
    return expr;
}

/* ParseExpr3 - handle the BAND operator */
static VMVALUE ParseExpr3(ParseContext *c)
{
    VMVALUE expr;
    int tkn;
    expr = ParseExpr4(c);
    while ((tkn = GetToken(c)) == '&')
        expr &= ParseExpr4(c);
    SaveToken(c,tkn);
    return expr;
}

/* ParseExpr4 - handle the '<<' and '>>' operators */
static VMVALUE ParseExpr4(ParseContext *c)
{
    VMVALUE expr, expr2;
    int tkn;
    expr = ParseExpr5(c);
    while ((tkn = GetToken(c)) == T_SHL || tkn == T_SHR) {
        expr2 = ParseExpr5(c);
        switch (tkn) {
        case T_SHL:
            expr <<= expr2;
            break;
        case T_SHR:
            expr >>= expr2;
            break;
        default:
            /* not reached */
            expr = 0;
            break;
        }
    }
    SaveToken(c,tkn);
    return expr;
}

/* ParseExpr5 - handle the '+' and '-' operators */
static VMVALUE ParseExpr5(ParseContext *c)
{
    VMVALUE expr, expr2;
    int tkn;
    expr = ParseExpr6(c);
    while ((tkn = GetToken(c)) == '+' || tkn == '-') {
        expr2 = ParseExpr6(c);
        switch (tkn) {
        case '+':
            expr += expr2;
            break;
        case '-':
            expr -= expr2;
            break;
        default:
            /* not reached */
            expr = 0;
            break;
        }
    }
    SaveToken(c, tkn);
    return expr;
}

/* ParseExpr6 - handle the '*', '/' and MOD operators */
static VMVALUE ParseExpr6(ParseContext *c)
{
    VMVALUE expr, expr2;
    int tkn;
    expr = ParseExpr7(c);
    while ((tkn = GetToken(c)) == '*' || tkn == '/' || tkn == '%') {
        expr2 = ParseExpr7(c);
        switch (tkn) {
        case '*':
            expr *= expr2;
            break;
        case '/':
            expr /= expr2;
            break;
        case '%':
            expr %= expr2;
            break;
        default:
            /* not reached */
            expr = 0;
            break;
        }
    }
    SaveToken(c, tkn);
    return expr;
}

/* ParseExpr7 - handle unary operators */
static VMVALUE ParseExpr7(ParseContext *c)
{
    VMVALUE expr;
    int tkn;
    switch (tkn = GetToken(c)) {
    case '+':
        expr = ParsePrimary(c);
        break;
    case '-':
        expr = -ParsePrimary(c);
        break;
    case '~':
        expr = ~ParsePrimary(c);
        break;
    default:
        SaveToken(c,tkn);
        expr = ParsePrimary(c);
        break;
    }
    return expr;
}

/* ParsePrimary - parse a primary expression */
static VMVALUE ParsePrimary(ParseContext *c)
{
    VMVALUE expr;
    Symbol *sym;
    switch (GetToken(c)) {
    case '(':
        expr = ParseExpr(c);
        FRequire(c,')');
        break;
    case T_NUMBER:
        expr = c->value;
        break;
    case T_IDENTIFIER:
        if (!(sym = FindSymbol(c, c->token)))
            sym = AddSymbol(c, c->token, SYMBOL_UNDEF, 0);
        expr = sym->value;
        break;
    default:
        ParseError(c, "Expecting a primary expression");
        /* not reached */
        expr = 0;
        break;
    }
    return expr;
}

/* GetLine - get the next input line */
static int GetLine(ParseContext *c)
{
    int lineNumber;
    if (!(*c->getLine)(c->getLineCookie, c->lineBuf, sizeof(c->lineBuf), &lineNumber))
        return FALSE;
    c->lineNumber = lineNumber;
    c->linePtr = c->lineBuf;
    c->savedToken = T_NONE;
    return TRUE;
}

/* FRequire - fetch a token and check it */
static void FRequire(ParseContext *c, int requiredToken)
{
    Require(c, GetToken(c), requiredToken);
}

/* Require - check for a required token */
static void Require(ParseContext *c, int token, int requiredToken)
{
    char tknbuf[MAXTOKEN];
    if (token != requiredToken) {
        strcpy(tknbuf, TokenName(requiredToken));
        ParseError(c, "Expecting '%s', found '%s'", tknbuf, TokenName(token));
    }
}

/* TokenName - get the name of a token */
static char *TokenName(int token)
{
    static char nameBuf[4];
    char *name;

    switch (token) {
    case T_NONE:
        name = "<NONE>";
        break;
    case T_LE:
        name = "<=";
        break;
    case T_NE:
        name = "<>";
        break;
    case T_GE:
        name = ">=";
        break;
    case T_SHL:
        name = "<<";
        break;
    case T_SHR:
        name = ">>";
        break;
    case T_IDENTIFIER:
        name = "<IDENTIFIER>";
        break;
    case T_NUMBER:
        name = "<NUMBER>";
        break;
    case T_STRING:
        name = "<STRING>";
        break;
    case T_EOL:
        name = "<EOL>";
        break;
    case T_EOF:
        name = "<EOF>";
        break;
    default:
        nameBuf[0] = '\'';
        nameBuf[1] = token;
        nameBuf[2] = '\'';
        nameBuf[3] = '\0';
        name = nameBuf;
        break;
    }

    /* return the token name */
    return name;
}

/* GetIdentifier - get an identifier token */
static int GetIdentifier(ParseContext *c, char *errtext)
{
    int tkn;
    if ((tkn = GetToken(c)) == T_EOL)
        return tkn;
    else if (tkn != T_IDENTIFIER)
        ParseError(c, errtext);
    return tkn;
}

/* GetToken - get the next token */
static int GetToken(ParseContext *c)
{
    int tkn;

    /* check for a saved token */
    if ((tkn = c->savedToken) != T_NONE)
        c->savedToken = T_NONE;

    /* otherwise, get the next token */
    else
        tkn = NextToken(c);

    /* return the token */
    return tkn;
}

/* SaveToken - save a token */
static void SaveToken(ParseContext *c, int token)
{
    c->savedToken = token;
}

/* NextToken - read the next token */
static int NextToken(ParseContext *c)
{
    int ch, tkn;
    
    /* skip leading blanks */
    ch = SkipSpaces(c);

    /* remember the start of the current token */
    c->tokenOffset = (int)(c->linePtr - c->lineBuf);

    /* check the next character */
    switch (ch) {
    case EOF:
        tkn = T_EOL;
        break;
    case '"':
        tkn = StringToken(c);
        break;
    case '\'':
        tkn = CharToken(c);
        break;
    case '<':
        if ((ch = GetChar(c)) == '=')
            tkn = T_LE;
        else if (ch == '>')
            tkn = T_NE;
        else if (ch == '<')
            tkn = T_SHL;
        else {
            UngetC(c);
            tkn = '<';
        }
        break;
    case '>':
        if ((ch = GetChar(c)) == '=')
            tkn = T_GE;
        else if (ch == '>')
            tkn = T_SHR;
        else {
            UngetC(c);
            tkn = '>';
        }
        break;
    case '0':
        switch (GetChar(c)) {
        case 'x':
        case 'X':
            tkn = HexNumberToken(c);
            break;
        case 'b':
        case 'B':
            tkn = BinaryNumberToken(c);
            break;
        default:
            UngetC(c);
            tkn = NumberToken(c, '0');
            break;
        }
        break;
    default:
        if (isdigit(ch))
            tkn = NumberToken(c, ch);
        else if (IdentifierCharP(ch))
            tkn = IdentifierToken(c,ch);
        else
            tkn = ch;
    }

    /* return the token */
    return tkn;
}

/* IdentifierToken - get an identifier */
static int IdentifierToken(ParseContext *c, int ch)
{
    int len, i;
    char *p;

    /* get the identifier */
    p = c->token; *p++ = ch; len = 1;
    while ((ch = GetChar(c)) != EOF && IdentifierCharP(ch)) {
        if (++len > MAXTOKEN)
            ParseError(c, "Identifier too long");
        *p++ = ch;
    }
    UngetC(c);
    *p = '\0';

    /* check to see if it is a keyword */
    for (i = 0; asmktab[i].keyword != NULL; ++i)
        if (strcasecmp(asmktab[i].keyword, c->token) == 0)
            return asmktab[i].token;

    /* otherwise, it is an identifier */
    return T_IDENTIFIER;
}

/* IdentifierCharP - is this an identifier character? */
static int IdentifierCharP(int ch)
{
    return isupper(ch)
        || islower(ch)
        || isdigit(ch)
        || strchr(":_", ch) != NULL;
}

/* NumberToken - get a number */
static  int NumberToken(ParseContext *c, int ch)
{
    char *p = c->token;

    /* get the number */
    *p++ = ch;
    while ((ch = GetChar(c)) != EOF) {
        if (isdigit(ch))
            *p++ = ch;
        else if (ch != '_')
            break;
    }
    UngetC(c);
    *p = '\0';
    
    /* convert the string to an integer */
    c->value = (VMVALUE)atol(c->token);
    
    /* return the token */
    return T_NUMBER;
}

/* HexNumberToken - get a hexadecimal number */
static int HexNumberToken(ParseContext *c)
{
    char *p = c->token;
    int ch;

    /* get the number */
    while ((ch = GetChar(c)) != EOF) {
        if (isxdigit(ch))
            *p++ = ch;
        else if (ch != '_')
            break;
    }
    UngetC(c);
    *p = '\0';
    
    /* convert the string to an integer */
    c->value = (VMVALUE)strtoul(c->token, NULL, 16);
    
    /* return the token */
    return T_NUMBER;
}

/* BinaryNumberToken - get a binary number */
static int BinaryNumberToken(ParseContext *c)
{
    char *p = c->token;
    int ch;

    /* get the number */
    while ((ch = GetChar(c)) != EOF) {
        if (ch == '0' || ch == '1')
            *p++ = ch;
        else if (ch != '_')
            break;
    }
    UngetC(c);
    *p = '\0';
    
    /* convert the string to an integer */
    c->value = (VMVALUE)strtoul(c->token, NULL, 2);
    
    /* return the token */
    return T_NUMBER;
}

/* StringToken - get a string */
static int StringToken(ParseContext *c)
{
    int ch,len;
    char *p;

    /* collect the string */
    p = c->token; len = 0;
    while ((ch = XGetC(c)) != EOF && ch != '"') {
        if (++len > MAXTOKEN)
            ParseError(c, "String too long");
        *p++ = (ch == '\\' ? LiteralChar(c) : ch);
    }
    *p = '\0';

    /* check for premature end of file */
    if (ch != '"')
        ParseError(c, "unterminated string");

    /* return the token */
    return T_STRING;
}

/* CharToken - get a character constant */
static int CharToken(ParseContext *c)
{
    int ch = LiteralChar(c);
    if (XGetC(c) != '\'')
        ParseError(c,"Expecting a closing single quote");
    c->token[0] = ch;
    c->token[1] = '\0';
    c->value = ch;
    return T_NUMBER;
}

/* LiteralChar - get a character from a literal string */
static int LiteralChar(ParseContext *c)
{
    int ch;
    switch (ch = XGetC(c)) {
    case 'n': 
        ch = '\n';
        break;
    case 'r':
        ch = '\r';
        break;
    case 't':
        ch = '\t';
        break;
    case EOF:
        ch = '\\';
        break;
    }
    return ch;
}

/* SkipSpaces - skip leading spaces and the the next non-blank character */
static int SkipSpaces(ParseContext *c)
{
    int ch;
    while ((ch = GetChar(c)) != EOF)
        if (!isspace(ch))
            break;
    return ch;
}

/* SkipComment - skip characters up to the end of a comment */
static int SkipComment(ParseContext *c)
{
    int lastch, ch;
    lastch = '\0';
    while ((ch = XGetC(c)) != EOF) {
        if (lastch == '*' && ch == '/')
            return TRUE;
        lastch = ch;
    }
    return FALSE;
}

/* GetChar - get the next character */
static int GetChar(ParseContext *c)
{
    int ch;

    /* check for being in a comment */
    if (c->inComment) {
        if (!SkipComment(c))
            return EOF;
        c->inComment = FALSE;
    }

    /* loop until we find a non-comment character */
    for (;;) {
        
        /* get the next character */
        if ((ch = XGetC(c)) == EOF)
            return EOF;

        /* check for a comment */
        else if (ch == '/') {
            if ((ch = XGetC(c)) == '/') {
                while ((ch = XGetC(c)) != EOF)
                    ;
            }
            else if (ch == '*') {
                if (!SkipComment(c)) {
                    c->inComment = TRUE;
                    return EOF;
                }
            }
            else {
                UngetC(c);
                ch = '/';
                break;
            }
        }

        /* handle a normal character */
        else
            break;
    }

    /* return the character */
    return ch;
}

/* XGetC - get the next character without checking for comments */
static int XGetC(ParseContext *c)
{
    int ch;
    
    /* get the next character on the current line */
    if (!(ch = *c->linePtr++)) {
        --c->linePtr;
        return EOF;
    }
    
    /* return the character */
    return ch;
}

/* UngetC - unget the most recent character */
static void UngetC(ParseContext *c)
{
    /* backup the input pointer */
    --c->linePtr;
}

/* ParseError - report a parsing error */
static void ParseError(ParseContext *c, char *fmt, ...)
{
    va_list ap;

    /* print the error message */
    va_start(ap, fmt);
    printf("error: ");
    vprintf(fmt, ap);
    putchar('\n');
    va_end(ap);

    /* show the context */
    printf("  line %d\n", c->lineNumber);
    printf("    %s\n", c->lineBuf);
    printf("    %*s\n", c->tokenOffset, "^");

	/* exit until we fix the assembler so it can recover from parse errors */
    longjmp(c->errorTarget, 1);
}

/* FindOpcode - find an opcode definition */
static OpDef *FindOpcode(char *name)
{
    OpDef *def = opcodeDefs;
    while (def->opname != NULL) {
        if (strcasecmp(name, def->opname) == 0)
            return def;
        ++def;
    }
    return NULL;
}

/* FindField - find a field definition */
static FieldDef *FindField(char *name)
{
    FieldDef *def = fieldDefs;
    while (def->keyword != NULL) {
        if (strcasecmp(name, def->keyword) == 0)
            return def;
        ++def;
    }
    return NULL;
}

/* InitSymbolTable - initialize an assembler symbol table */
static void InitSymbolTable(SymbolTable *table)
{
    table->head = NULL;
    table->pTail = &table->head;
}

/* AddSymbol - add symbol to the assembler symbol table */
static Symbol *AddSymbol(ParseContext *c, const char *name, SymbolType type, VMVALUE value)
{
    size_t size = sizeof(Symbol) + strlen(name);
    SymbolTable *table;
    Symbol *sym;
    
    /* select the local or global symbol table */
    if (*name == ':')
        table = &c->locals;
    else {
        EmptySymbolTable(&c->locals);
        table = &c->globals;
    }

    /* allocate the symbol structure */
    if (!(sym = (Symbol *)malloc(size)))
        ParseError(c, "insufficient memory");
    strcpy(sym->name, name);
    sym->type = type;
    sym->value = value;
    sym->next = NULL;

    /* add it to the symbol table */
    *table->pTail = sym;
    table->pTail = &sym->next;
    
    /* return the symbol */
    return sym;
}

/* FindSymbol - find a symbol in an assembler symbol table */
static Symbol *FindSymbol(ParseContext *c, const char *name)
{
    Symbol *sym;
    
    /* check the local symbol table */
    for (sym = c->locals.head; sym != NULL; sym = sym->next) {
        if (strcasecmp(name, sym->name) == 0)
            return sym;
    }
    
    /* check the global symbol table */
    for (sym = c->globals.head; sym != NULL; sym = sym->next) {
        if (strcasecmp(name, sym->name) == 0)
            return sym;
    }
    
    /* not found */
    return NULL;
}

/* EmptySymbolTable - empty and reinitialize a symbol table */
static void EmptySymbolTable(SymbolTable *table)
{
    Symbol *sym, *next;
    for (sym = table->head; sym != NULL; sym = next) {
        next = sym->next;
        free(sym);
    }
    InitSymbolTable(table);
}

#ifdef MAIN

/* DumpSymbols - dump an assembler symbol table */
static void DumpSymbols(SymbolTable *table)
{
    Symbol *sym;
    for (sym = table->head; sym != NULL; sym = sym->next)
        printf("%08x %s\n", sym->value, sym->name);
}

#endif
