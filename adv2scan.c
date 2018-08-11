/* db_scan.c - token scanner
 *
 * Copyright (c) 2014 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>
#include <ctype.h>
#include "adv2compiler.h"

/* keyword table */
static struct {
    char *keyword;
    int token;
} ktab[] = {

/* these must be in the same order as the int enum */
{   "def",      T_DEF       },
{   "var",      T_VAR       },
{   "if",       T_IF        },
{   "else",     T_ELSE      },
{   "for",      T_FOR       },
{   "do",       T_DO        },
{   "while",    T_WHILE     },
{   "continue", T_CONTINUE  },
{   "break",    T_BREAK     },
{   "return",   T_RETURN    },
{   "object",   T_OBJECT    },
{   "method",   T_METHOD    },
{   "shared",   T_SHARED    },
{   "super",    T_SUPER     },
{   "print",    T_PRINT     },
{   NULL,       0           }
};

/* special no-identifier tokens */
static char *ttab[] = {
"<=",
"==",
"!=",
">=",
"<<",
">>",
"&&",
"||",
"++",
"--",
"+=",
"-=",
"*=",
"/=",
"%=",
"&=",
"|=",
"^=",
"<<=",
">>=",
"<identifier>",
"<integer>",
"<string>",
"<eol>",
"<eof>"
};

/* local function prototypes */
static int NextToken(ParseContext *c);
static int IdentifierToken(ParseContext *c, int ch);
static int IdentifierCharP(int ch);
static int NumberToken(ParseContext *c, int ch);
static int HexNumberToken(ParseContext *c);
static int BinaryNumberToken(ParseContext *c);
static int StringToken(ParseContext *c);
static int CharToken(ParseContext *c);
static int LiteralChar(ParseContext *c);

/* InitScan - initialize the token scanner */
void InitScan(ParseContext *c)
{
    c->currentFile = NULL;
    c->includedFiles = NULL;
    c->currentInclude = NULL;
    c->linePtr = c->lineBuf;
    c->lineBuf[0] = '\0';
    c->savedToken = T_NONE;
    c->inComment = VMFALSE;
}

/* PushFile - push a file onto the input file stack */
int PushFile(ParseContext *c, const char *name)
{
    IncludedFile *inc;
    ParseFile *f;
    
    /* check to see if the file has already been included */
    for (inc = c->includedFiles; inc != NULL; inc = inc->next)
        if (strcmp(name, inc->name) == 0)
            return VMTRUE;
    
    /* add this file to the list of already included files */
    if (!(inc = (IncludedFile *)malloc(sizeof(IncludedFile) + strlen(name))))
        ParseError(c, "insufficient memory");
    strcpy(inc->name, name);
    inc->next = c->includedFiles;
    c->includedFiles = inc;

    /* allocate a parse file structure */
    if (!(f = (ParseFile *)malloc(sizeof(ParseFile))))
        ParseError(c, "insufficient memory");
    
    /* open the input file */
    if (!(f->file.fp = fopen(name, "r"))) {
        free(f);
        return VMFALSE;
    }
    f->file.file = inc;
    
    /* initialize the parse context */
    f->lineNumber = 0;
    
    /* push the file onto the input file stack */
    f->next = c->currentFile;
    c->currentFile = f;
    c->currentInclude = inc;
    
    /* return successfully */
    return VMTRUE;
}

/* GetLine - get the next input line */
int GetLine(ParseContext *c)
{
    ParseFile *f;
    int len;

    /* get the next input line */
    for (;;) {
        
        /* get the current input file */
        if (!(f = c->currentFile))
            return VMFALSE;
        
        /* get a line from the main input */
        if (fgets(c->lineBuf, sizeof(c->lineBuf) - 1, f->file.fp))
            break;
        
        /* pop the input file stack on end of file */
        if ((c->currentFile = f->next) != NULL)
            c->currentInclude = c->currentFile->file.file;
        else
            c->currentInclude = NULL;
            
        /* close the file we just finished */
        fclose(f->file.fp);
        free(f);
    }
    
    /* make sure the line is correctly terminated */
    len = strlen(c->lineBuf);
    if (len == 0 || c->lineBuf[len - 1] != '\n') {
        c->lineBuf[len++] = '\n';
        c->lineBuf[len] = '\0';
    }

    /* initialize the input buffer */
    c->linePtr = c->lineBuf;
    ++f->lineNumber;

    /* clear lookahead token */
    c->savedToken = T_NONE;

    /* return successfully */
    return VMTRUE;
}

/* FRequire - fetch a token and check it */
void FRequire(ParseContext *c, int requiredToken)
{
    Require(c, GetToken(c), requiredToken);
}

/* Require - check for a required token */
void Require(ParseContext *c, int token, int requiredToken)
{
    if (token != requiredToken) {
        char tknbuf[MAXTOKEN];
        strcpy(tknbuf, TokenName(requiredToken));
        ParseError(c, "Expecting '%s', found '%s'", tknbuf, TokenName(token));
    }
}

/* GetToken - get the next token */
int GetToken(ParseContext *c)
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
void SaveToken(ParseContext *c, int token)
{
    c->savedToken = token;
}

/* TokenName - get the name of a token */
char *TokenName(int token)
{
    char *name;
    
    if (token < _T_FIRST_KEYWORD) {
        static char nameBuf[4];
        nameBuf[0] = '\'';
        nameBuf[1] = token;
        nameBuf[2] = '\'';
        nameBuf[3] = '\0';
        name = nameBuf;
    }
    else if (token < _T_NON_KEYWORDS)
        name = ktab[token - _T_FIRST_KEYWORD].keyword;
    else if (token < _T_MAX)
        name = ttab[token - _T_NON_KEYWORDS];
    else
        name = "<invalid>";
        
    /* return the token name */
    return name;
}

/* NextToken - read the next token */
static int NextToken(ParseContext *c)
{
    int tkn = T_NONE;
    int ch, ch2;
    
again:
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
        switch (ch = GetChar(c)) {
        case '=':
            return T_LE;
        case '<':
            if ((ch = GetChar(c)) == '=')
                return T_SHLEQ;
            UngetC(c);
            return T_SHL;
        default:
            UngetC(c);
            return '<';
        }
    case '=':
        if ((ch = GetChar(c)) == '=')
            return T_EQ;
        else {
            UngetC(c);
            return '=';
        }
        break;
    case '!':
        if ((ch = GetChar(c)) == '=')
            tkn = T_NE;
        else {
            UngetC(c);
            tkn = '!';
        }
        break;
    case '>':
       switch (ch = GetChar(c)) {
        case '=':
            return T_GE;
        case '>':
            if ((ch = GetChar(c)) == '=')
                return T_SHREQ;
            UngetC(c);
            return T_SHR;
        default:
            UngetC(c);
            return '>';
        }
    case '&':
        switch (ch = GetChar(c)) {
        case '&':
            return T_AND;
        case '=':
            return T_ANDEQ;
        default:
            UngetC(c);
            return '&';
        }
    case '|':
        switch (ch = GetChar(c)) {
        case '|':
            return T_OR;
        case '=':
            return T_OREQ;
        default:
            UngetC(c);
            return '|';
        }
    case '^':
        if ((ch = GetChar(c)) == '=')
            return T_XOREQ;
        UngetC(c);
        return '^';
    case '+':
        switch (ch = GetChar(c)) {
        case '+':
            return T_INC;
        case '=':
            return T_ADDEQ;
        default:
            UngetC(c);
            return '+';
        }
    case '-':
        switch (ch = GetChar(c)) {
        case '-':
            return T_DEC;
        case '=':
            return T_SUBEQ;
        default:
            UngetC(c);
            return '-';
        }
    case '*':
        if ((ch = GetChar(c)) == '=')
            return T_MULEQ;
        UngetC(c);
        return '*';
    case '/':
        switch (ch = GetChar(c)) {
        case '=':
            return T_DIVEQ;
        case '/':
            while ((ch = GetChar(c)) != EOF)
                if (ch == '\n')
                    goto again;
            break;
        case '*':
            ch = ch2 = EOF;
            for (; (ch2 = GetChar(c)) != EOF; ch = ch2)
                if (ch == '*' && ch2 == '/')
                    goto again;
            break;
        default:
            UngetC(c);
            return '/';
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
    for (i = 0; ktab[i].keyword != NULL; ++i)
        if (strcmp(ktab[i].keyword, c->token) == 0)
            return ktab[i].token;

    /* otherwise, it is an identifier */
    return T_IDENTIFIER;
}

/* IdentifierCharP - is this an identifier character? */
static int IdentifierCharP(int ch)
{
    return isupper(ch)
        || islower(ch)
        || isdigit(ch)
        || ch == '_';
}

/* NumberToken - get a number */
static int NumberToken(ParseContext *c, int ch)
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
    while ((ch = GetChar(c)) != EOF && ch != '"') {
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
    if (GetChar(c) != '\'')
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
    switch (ch = GetChar(c)) {
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
int SkipSpaces(ParseContext *c)
{
    int ch;
    while ((ch = GetChar(c)) != EOF)
        if (!isspace(ch))
            break;
    return ch;
}

/* GetChar - get the next character */
int GetChar(ParseContext *c)
{
    int ch;
    
    /* get the next character on the current line */
    while (!(ch = *c->linePtr++)) {
        if (!GetLine(c))
            return EOF;
    }
    
    /* return the character */
    return ch;
}

/* UngetC - unget the most recent character */
void UngetC(ParseContext *c)
{
    /* backup the input pointer */
    --c->linePtr;
}

/* ParseError - report a parsing error */
void ParseError(ParseContext *c, char *fmt, ...)
{
    va_list ap;

    /* print the error message */
    va_start(ap, fmt);
    printf("error: ");
    vprintf(fmt, ap);
    putchar('\n');
    va_end(ap);

    /* show the context */
    printf("  line %d\n", c->currentFile->lineNumber);
    printf("    %s\n", c->lineBuf);
    printf("    %*s\n", c->tokenOffset, "^");

    /* exit until we fix the compiler so it can recover from parse errors */
    longjmp(c->errorTarget, 1);
}
