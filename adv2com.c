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

static void Usage(void);
static void ConnectAll(ParseContext *c);
static void WriteImage(ParseContext *c, char *name);
static void PrintStrings(ParseContext *c);

int main(int argc, char *argv[])
{
    ParseContext context;
    ParseContext *c = &context;
    char outputFile[100], *p;
    char *inputFile = NULL;
    int showSymbols = VMFALSE;
    int i;
    
    /* initialize the parse context */
    memset(c, 0, sizeof(ParseContext));
    InitSymbolTable(c);
    InitScan(c);
    AddGlobal(c, "nil", SC_CONSTANT, 0);
    
    /* add the containment properties */
    c->parentProperty = AddProperty(c, "_parent");
    c->siblingProperty = AddProperty(c, "_sibling");
    c->childProperty = AddProperty(c, "_child");
    
    /* make "loc" a synonym for "parent" */
    AddGlobal(c, "_loc", SC_CONSTANT, c->parentProperty);
    
    /* add the boolean values */
    AddGlobal(c, "true", SC_CONSTANT, 1);
    AddGlobal(c, "false", SC_CONSTANT, 0);
    
    /* get the arguments */
    for(i = 1; i < argc; ++i) {

        /* handle switches */
        if(argv[i][0] == '-') {
            switch(argv[i][1]) {
            case 'd':   // enable debug mode
                c->debugMode = VMTRUE;
                break;
            case 's':   // show the global symbol table
                showSymbols = VMTRUE;
                break;
            default:
                Usage();
                break;
            }
        }

        /* handle the input filename */
        else {
            if (inputFile)
                Usage();
            inputFile = argv[i];
        }
    }
        
    if (!inputFile)
        Usage();
        
    if (!(p = strrchr(inputFile, '.')))
        strcpy(outputFile, inputFile);
    else {
        strncpy(outputFile, inputFile, p - argv[1]);
        outputFile[p - inputFile] = '\0';
    }
    strcat(outputFile, ".dat");
    
    /* initialize the memory spaces */
    c->codeFree = c->codeBuf;
    c->codeTop = c->codeBuf + sizeof(c->codeBuf);
    c->dataFree = c->dataBuf;
    c->dataTop = c->dataBuf + sizeof(c->dataBuf);
    c->stringFree = c->stringBuf;
    c->stringTop = c->stringBuf + sizeof(c->stringBuf);
    
    /* fake place to return to from main */
    putcbyte(c, 0); // argument count from fake CALL instruction
    putcbyte(c, OP_HALT);
    
    /* make sure no object has a zero offset */
    StoreInitializer(c, 0);
    
    if (setjmp(c->errorTarget))
        return 1;
        
    if (!PushFile(c, inputFile)) {
        printf("error: can't open '%s'\n", argv[1]);
        return 1;
    }
    
    /* compile the program */
    ParseDeclarations(c);
    
    /* link all child objects with their parents */
    ConnectAll(c);
    
    if (showSymbols || c->debugMode) {
        PrintSymbols(c);
    }
    if (c->debugMode) {
        PrintStrings(c);
    }
    
    WriteImage(c, outputFile);
    
    return 0;
}
  
static void Usage(void)
{
    printf("usage: adv2com [ -d ] [ -s ] <file>\n");
    exit(1);
}

static void WriteImage(ParseContext *c, char *name)
{
    int dataSize = c->dataFree - c->dataBuf;
    int codeSize = c->codeFree - c->codeBuf;
    int stringSize = c->stringFree - c->stringBuf;
    int imageSize = sizeof(ImageHdr) + dataSize + codeSize + stringSize;
    ImageHdr *hdr;
    Symbol *sym;
    FILE *fp;
    
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
    
    if (!(fp = fopen(name, "wb")))
        ParseError(c, "can't create file '%s'", name);
        
    if (fwrite(hdr, 1, imageSize, fp) != imageSize)
        ParseError(c, "error writing image file");
        
    fclose(fp);
    
    //Execute(hdr);
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

/* AddObject - add an object to the list for parent/sibling/child linking */
void AddObject(ParseContext *c, VMVALUE object)
{
    ObjectListEntry *entry = (ObjectListEntry *)LocalAlloc(c, sizeof(ObjectListEntry));
    entry->next = c->objects;
    entry->object = object;
    c->objects = entry;
}

/* getp - get the value of an object property */
static int getp(ParseContext *c, VMVALUE object, VMVALUE tag, VMVALUE *pValue)
{
    ObjectHdr *objectHdr = (ObjectHdr *)(c->dataBuf + object);
    Property *property = (Property *)(objectHdr + 1);
    int cnt = objectHdr->nProperties;
    while (--cnt >= 0) {
        if ((property->tag & ~P_SHARED) == tag) {
            *pValue = property->value;
            return VMTRUE;
        }
        ++property;
    }
    return VMFALSE;
}

/* setp - set the value of an object property */
static int setp(ParseContext *c, VMVALUE object, VMVALUE tag, VMVALUE value)
{
    ObjectHdr *objectHdr = (ObjectHdr *)(c->dataBuf + object);
    Property *property = (Property *)(objectHdr + 1);
    int cnt = objectHdr->nProperties;
    while (--cnt >= 0) {
        if ((property->tag & ~P_SHARED) == tag) {
            property->value = value;
            return VMTRUE;
        }
        ++property;
    }
    return VMFALSE;
}

/* ConnectAll - link together all children of each parent object */
static void ConnectAll(ParseContext *c)
{
    ObjectListEntry *entry = c->objects;
    while (entry) {
        VMVALUE parent, child;
        if (getp(c, entry->object, c->parentProperty, &parent) && parent) {
            if (getp(c, parent, c->childProperty, &child)) {
                setp(c, entry->object, c->siblingProperty, child);
                setp(c, parent, c->childProperty, entry->object);
            }
        }
        entry = entry->next;
    }
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
    char *storageClassNames[] = { "?", "C", "V", "O", "F" };
    Symbol *sym;
    printf("Globals\n");
    for (sym = c->globals.head; sym != NULL; sym = sym->next)
        if (sym->valueDefined)
            printf("  %20s %s %d\n", sym->name, storageClassNames[sym->storageClass], sym->v.value);
        else
            printf("  %20s %s (undefined)\n", sym->name, storageClassNames[sym->storageClass]);
}

/* AddString - add a string to the string table */
String *AddString(ParseContext *c, char *value)
{
    String *str;
    int size;
    
    /* check to see if the string is already in the table */
    for (str = c->strings; str != NULL; str = str->next)
        if (strcmp(value, (char *)c->stringBuf + str->offset) == 0)
            return str;

    if (c->stringFree + strlen(value) + 1 > c->stringTop)
        ParseError(c, "insufficient string space");

    /* allocate the string structure */
    size = sizeof(String) + strlen(value);
    str = (String *)LocalAlloc(c, size);
    str->offset = c->stringFree - c->stringBuf;
    strcpy((char *)c->stringFree, value);
    c->stringFree += strlen(value) + 1;
    str->next = c->strings;
    c->strings = str;

    /* return the string table entry */
    return str;
}

static void PrintStrings(ParseContext *c)
{
    String *str;
    for (str = c->strings; str != NULL; str = str->next)
        printf("%d '%s'\n", str->offset, c->stringBuf + str->offset);
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
