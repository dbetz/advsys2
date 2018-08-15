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
#include "adv2vm.h"

static void WriteImage(ParseContext *c, char *name);

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
    
    WriteImage(c, "adv2sys.out");
    
    //Execute(c->codeBuf);

    return 0;
}

static void WriteImage(ParseContext *c, char *name)
{
    int dataSize = c->dataFree - c->dataBuf;
    int codeSize = c->codeFree - c->codeBuf;
    int stringSize = c->stringFree - c->stringBuf;
    int imageSize = sizeof(ImageHdr) + dataSize + codeSize + stringSize;
    ImageHdr *hdr;
    Symbol *sym;
    
    if (!(hdr = (ImageHdr *)malloc(imageSize)))
        ParseError(c, "insufficient memory to build image");
    hdr->dataOffset = sizeof(ImageHdr);
    hdr->dataSize = dataSize;
    hdr->codeOffset = hdr->dataOffset + dataSize;
    hdr->codeSize = codeSize;
    hdr->stringOffset = hdr->codeOffset + hdr->codeSize;
    hdr->stringSize = stringSize;
    
    memcpy((uint8_t *)hdr + sizeof(ImageHdr), c->dataBuf, dataSize);
    memcpy((uint8_t *)hdr + sizeof(ImageHdr) + dataSize, c->codeBuf, codeSize);
    memcpy((uint8_t *)hdr + sizeof(ImageHdr) + dataSize + codeSize, c->stringBuf, stringSize);
    
    if (!(sym = FindSymbol(c, "main")))
        ParseError(c, "no 'main' function");
    else if (!sym->valueDefined)
        ParseError(c, "'main' not defined");
    else if (sym->storageClass != SC_FUNCTION)
        ParseError(c, "expecting 'main' to be a function");
    hdr->mainFunction = sym->v.value;
    
    Execute(hdr);
}

static char *storageClassNames[] = {
    NULL,
    "a constant",
    "a variable",
    "an object",
    "a function"
};

/* AddGlobal - add a global symbol to the symbol table */
Symbol *AddGlobal(ParseContext *c, const char *name, StorageClass storageClass, VMVALUE value)
{
    Symbol *sym;
    
    /* check to see if the symbol is already defined */
    if ((sym = FindSymbol(c, name)) != NULL) {
        Fixup *fixup, *nextFixup;
        if (sym->valueDefined)
            ParseError(c, "already defined");
        else if (storageClass != sym->storageClass)
            ParseError(c, "expecting %s", storageClassNames[sym->storageClass]);
        for (fixup = sym->v.fixups; fixup != NULL; fixup = nextFixup) {
            nextFixup = fixup->next;
            switch (fixup->type) {
            case FT_DATA:
                *(VMVALUE *)&c->dataBuf[fixup->offset] = value;
                break;
            case FT_CODE:
                wr_clong(c, fixup->offset, value);
                break;
            }
            free(fixup);
        }
        sym->valueDefined = VMTRUE;
        sym->v.value = value;
        return sym;
    }
    
    /* add the symbol */
    return AddSymbol(c, name, storageClass, value);
}

/* FindObject - find an object in the symbol table */
int FindObject(ParseContext *c, const char *name)
{
    Symbol *sym;

    if ((sym = FindSymbol(c, name)) != NULL) {
        if (sym->storageClass != SC_OBJECT)
            ParseError(c, "not an object");
        if (!sym->valueDefined)
            ParseError(c, "object not defined");
        return sym->v.value;
    }

    ParseError(c, "object not defined");
    return NIL; // never reached
}

/* AddProperty - add a property symbol to the symbol table */
VMVALUE AddProperty(ParseContext *c, const char *name)
{
    Symbol *sym;
    
    /* check to see if the symbol is already defined */
    if ((sym = FindSymbol(c, name)) != NULL) {
        if (sym->storageClass != SC_CONSTANT)
            ParseError(c, "not a property or constant");
        return sym->v.value;
    }
    
    /* add the symbol */
    sym = AddSymbol(c, name, SC_CONSTANT, ++c->propertyCount);
    return sym->v.value;
}

/* InitSymbolTable - initialize a symbol table */
void InitSymbolTable(ParseContext *c)
{
    c->globals.head = NULL;
    c->globals.pTail = &c->globals.head;
}

/* AddSymbolRef - add a symbol reference */
int AddSymbolRef(ParseContext *c, Symbol *symbol, FixupType fixupType, VMVALUE offset)
{
    Fixup *fixup;
    if (symbol->valueDefined)
        return symbol->v.value;
    fixup = (Fixup *)LocalAlloc(c, sizeof(Fixup));
    fixup->type = fixupType;
    fixup->offset = offset;
    fixup->next = symbol->v.fixups;
    symbol->v.fixups = fixup;
    return 0;
}

/* AddRawSymbol - add a defined or undefined symbol to a symbol table */
static Symbol *AddRawSymbol(ParseContext *c, const char *name, StorageClass storageClass)
{
    size_t size = sizeof(Symbol) + strlen(name);
    Symbol *sym;
    
    /* allocate the symbol structure */
    sym = (Symbol *)LocalAlloc(c, size);
    memset(sym, 0, sizeof(Symbol));
    strcpy(sym->name, name);
    sym->storageClass = storageClass;

    /* add it to the symbol table */
    *c->globals.pTail = sym;
    c->globals.pTail = &sym->next;
    
    /* return the symbol */
    return sym;
}

/* AddUndefinedSymbol - add an undefined symbol to a symbol table */
Symbol *AddUndefinedSymbol(ParseContext *c, const char *name, StorageClass storageClass)
{
    Symbol *sym = AddRawSymbol(c, name, storageClass);
    sym->valueDefined = VMFALSE;
    return sym;
}

/* AddSymbol - add a symbol to a symbol table */
Symbol *AddSymbol(ParseContext *c, const char *name, StorageClass storageClass, int value)
{
    Symbol *sym = AddRawSymbol(c, name, storageClass);
    sym->valueDefined = VMTRUE;
    sym->v.value = value;
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
        if (sym->valueDefined)
            printf("  %s\t%d\t%d\n", sym->name, sym->storageClass, sym->v.value);
        else
            printf("  %s\t%d\t(undefined)\n", sym->name, sym->storageClass);
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
