/* adv2com.c - a simple compiler for a c-like adventure authoring language
 *
 * Copyright (c) 2018 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>
#include "adv2compiler.h"

int main(int argc, char *argv[])
{
    ParseContext context;
    ParseContext *c = &context;
    
    /* initialize the parse context */
    memset(c, 0, sizeof(ParseContext));
    InitSymbolTable(c);
    InitScan(c);
    
    /* initialize the memory spaces */
    c->codeFree = c->codeBuf;
    c->codeTop = c->codeBuf + sizeof(c->codeBuf);
    c->dataFree = c->dataBuf;
    c->dataTop = c->dataBuf + sizeof(c->dataBuf);
    c->stringFree = c->stringBuf;
    c->stringTop = c->stringBuf + sizeof(c->stringBuf);
    
    if (argc != 2) {
        printf("usage: adv2com <file>\n");
        return 1;
    }
    
    if (setjmp(c->errorTarget))
        return 1;
        
    if (!PushFile(c, argv[1])) {
        printf("error: can't open '%s'\n", argv[1]);
        return 1;
    }
    
    ParseDeclarations(c);
    
    PrintSymbols(c);
    
    //Execute(c->codeBuf);

    return 0;
}

/* AddGlobal - add a global symbol to the symbol table */
Symbol *AddGlobal(ParseContext *c, const char *name, StorageClass storageClass, VMVALUE value)
{
    Symbol *sym;
    
    /* check to see if the symbol is already defined */
    if ((sym = FindSymbol(c, name)) != NULL)
        return sym;
    
    /* add the symbol */
    return AddSymbol(c, name, storageClass, value);
}

/* AddObject - enter an object into the symbol table */
int AddObject(ParseContext *c, const char *name)
{
    Symbol *sym;

    if ((sym = FindSymbol(c, name)) != NULL) {
        if (sym->storageClass != SC_OBJECT)
            ParseError(c, "not an object");
        return sym->value;
    }
    
    if (c->objectCount < MAXOBJECTS) {
        AddSymbol(c, name, SC_OBJECT, ++c->objectCount);
        c->objectTable[c->objectCount] = 0;
    }
    else
        ParseError(c, "too many objects");
        
    return c->objectCount;
}

/* FindObject - find an object in the symbol table */
int FindObject(ParseContext *c, const char *name)
{
    Symbol *sym;

    if ((sym = FindSymbol(c, name)) != NULL) {
        if (sym->storageClass != SC_OBJECT)
            ParseError(c, "not an object");
        if (sym->value == NIL)
            ParseError(c, "object not defined");
        return sym->value;
    }

    ParseError(c, "object not defined");
    return NIL; // never reached
}


/* InitSymbolTable - initialize a symbol table */
void InitSymbolTable(ParseContext *c)
{
    c->globals.head = NULL;
    c->globals.pTail = &c->globals.head;
}

/* AddSymbol - add a symbol to a symbol table */
Symbol *AddSymbol(ParseContext *c, const char *name, StorageClass storageClass, int value)
{
    size_t size = sizeof(Symbol) + strlen(name);
    Symbol *sym;
    
    /* allocate the symbol structure */
    sym = (Symbol *)LocalAlloc(c, size);
    memset(sym, 0, sizeof(Symbol));
    strcpy(sym->name, name);
    sym->storageClass = storageClass;
    sym->value = value;

    /* add it to the symbol table */
    *c->globals.pTail = sym;
    c->globals.pTail = &sym->next;
    
    /* return the symbol */
    return sym;
}

/* FindSymbol - find a symbol in a symbol table */
Symbol *FindSymbol(ParseContext *c, const char *name)
{
    Symbol *sym = c->globals.head;
    while (sym) {
        if (strcmp(name, sym->name) == 0)
            return sym;
        sym = sym->next;
    }
    return NULL;
}

/* PrintSymbols - print a symbol table */
void PrintSymbols(ParseContext *c)
{
    Symbol *sym;
    printf("Globals\n");
    for (sym = c->globals.head; sym != NULL; sym = sym->next)
        printf("  %s\t%d\t%d\n", sym->name, sym->storageClass, sym->value);
}

/* AddString - add a string to the string table */
String *AddString(ParseContext *c, char *value)
{
    String *str;
    int size;
    
    /* check to see if the string is already in the table */
    for (str = c->strings; str != NULL; str = str->next)
        if (strcmp(value, str->data) == 0)
            return str;

    /* allocate the string structure */
    size = sizeof(String) + strlen(value);
    str = (String *)LocalAlloc(c, size);
    strcpy((char *)str->data, value);
    str->next = c->strings;
    c->strings = str;

    /* return the string table entry */
    return str;
}

/* LocalAlloc - allocate memory from the local heap */
void *LocalAlloc(ParseContext *c, size_t size)
{
    void *data = (void *)malloc(size);
    if (!data) Abort(c, "insufficient memory");
    return data;
}

void Abort(ParseContext *c, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("error: ");
    vprintf(fmt, ap);
    putchar('\n');
    va_end(ap);
    longjmp(c->errorTarget, 1);
}
