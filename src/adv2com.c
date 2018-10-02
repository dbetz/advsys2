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
#include "propbinary.h"

extern uint8_t advsys2_run_template_array[];
extern int advsys2_run_template_size;
extern uint8_t advsys2_step_template_array[];
extern int advsys2_step_template_size;
extern uint8_t wordfire_template_array[];
extern int wordfire_template_size;

static void Usage(void);
static uint8_t *BuildImage(ParseContext *c, int *pSize);
static void WriteImage(ParseContext *c, char *name, uint8_t *image, int imageSize);
static void ConnectAll(ParseContext *c);
static void PlaceStrings(ParseContext *c);
static void PrintStrings(ParseContext *c);

int main(int argc, char *argv[])
{
    ParseContext context;
    ParseContext *c = &context;
    char outputFileBuf[100], *p;
    char *inputFile = NULL;
    char *outputFile = NULL;
    char *templateName = NULL;
    int showSymbols = VMFALSE;
    int runProgram = VMFALSE;
    uint8_t *template, *image;
    int templateSize, imageSize;
    char *ext = ".dat";
    int i;
    
    /* initialize the parse context */
    memset(c, 0, sizeof(ParseContext));
    InitSymbolTable(c);
    InitScan(c);
    AddGlobal(c, "nil", SC_CONSTANT, 0);
    c->pNextString = &c->strings;
    c->pNextDataBlock = &c->dataBlocks;
    c->pNextWord = &c->words;
    
    /* add the containment properties */
    c->parentProperty = AddProperty(c, "_parent");
    c->siblingProperty = AddProperty(c, "_sibling");
    c->childProperty = AddProperty(c, "_child");
    
    /* make "loc" a synonym for "parent" */
    AddGlobal(c, "_loc", SC_CONSTANT, c->parentProperty);
    
    /* add the boolean values */
    AddGlobal(c, "true", SC_CONSTANT, 1);
    AddGlobal(c, "false", SC_CONSTANT, 0);
    
    /* add the propeller registers */
    AddSymbol(c, "par",         SC_VARIABLE, COG_BASE + 0x1f0);
    AddSymbol(c, "cnt",         SC_VARIABLE, COG_BASE + 0x1f1);
    AddSymbol(c, "ina",         SC_VARIABLE, COG_BASE + 0x1f2);
    AddSymbol(c, "inb",         SC_VARIABLE, COG_BASE + 0x1f3);
    AddSymbol(c, "outa",        SC_VARIABLE, COG_BASE + 0x1f4);
    AddSymbol(c, "outb",        SC_VARIABLE, COG_BASE + 0x1f5);
    AddSymbol(c, "dira",        SC_VARIABLE, COG_BASE + 0x1f6);
    AddSymbol(c, "dirb",        SC_VARIABLE, COG_BASE + 0x1f7);
    AddSymbol(c, "ctra",        SC_VARIABLE, COG_BASE + 0x1f8);
    AddSymbol(c, "ctrb",        SC_VARIABLE, COG_BASE + 0x1f9);
    AddSymbol(c, "frqa",        SC_VARIABLE, COG_BASE + 0x1fa);
    AddSymbol(c, "frqb",        SC_VARIABLE, COG_BASE + 0x1fb);
    AddSymbol(c, "phsa",        SC_VARIABLE, COG_BASE + 0x1fc);
    AddSymbol(c, "phsb",        SC_VARIABLE, COG_BASE + 0x1fd);
    AddSymbol(c, "vcfg",        SC_VARIABLE, COG_BASE + 0x1fe);
    AddSymbol(c, "vscl",        SC_VARIABLE, COG_BASE + 0x1ff);
    
    /* get the arguments */
    for(i = 1; i < argc; ++i) {

        /* handle switches */
        if(argv[i][0] == '-') {
            switch(argv[i][1]) {
            case 'd':   // enable debug mode
                c->debugMode = VMTRUE;
                break;
            case 'o':
                if(argv[i][2])
                    outputFile = &argv[i][2];
                else if(++i < argc)
                    outputFile = argv[i];
                else
                    Usage();
                break;
            case 'r':   // run program after compiling
                runProgram = VMTRUE;
                break;
            case 's':   // show the global symbol table
                showSymbols = VMTRUE;
                break;
            case 't':
                if(argv[i][2])
                    templateName = &argv[i][2];
                else if(++i < argc)
                    templateName = argv[i];
                else
                    Usage();
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
        
    if (templateName) {
        if (strcmp(templateName, "run") == 0) {
            template = advsys2_run_template_array;
            templateSize = advsys2_run_template_size;
        }
        else if (strcmp(templateName, "step") == 0) {
            template = advsys2_step_template_array;
            templateSize = advsys2_step_template_size;
        }
        else if (strcmp(templateName, "wordfire") == 0) {
            template = wordfire_template_array;
            templateSize = wordfire_template_size;
        }
        else {
            printf("error: unknown template name '%s'\n", templateName);
            return 1;
        }
        ext = ".binary";
    }
    
    if (!outputFile) {
        if (!(p = strrchr(inputFile, '.')))
            strcpy(outputFileBuf, inputFile);
        else {
            strncpy(outputFileBuf, inputFile, p - argv[1]);
            outputFileBuf[p - inputFile] = '\0';
        }
        strcat(outputFileBuf, ext);
        outputFile = outputFileBuf;
    }
    
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
    
    /* place strings at the end of data space */
    PlaceStrings(c);
    
    /* link all child objects with their parents */
    ConnectAll(c);
    
    if (showSymbols || c->debugMode) {
        PrintSymbols(c);
    }
    if (c->debugMode) {
        PrintStrings(c);
    }
    printf("data: %d, code %d, strings: %d\n", (int)(c->dataFree - c->dataBuf), (int)(c->codeFree - c->codeBuf), (int)(c->stringFree - c->stringBuf));
    
    {
        Word *word = c->words;
        if (word) {
            printf("Words:\n");
            while (word != NULL) {
                printf("  %s %d %d\n", word->string->data, word->type, word->string->offset);
                word = word->next;
            }
        }
    }
    
    image = BuildImage(c, &imageSize);
    
    if (template) {
        uint8_t *binary;
        int binarySize;
        binary = BuildBinary(template, templateSize, image, imageSize, &binarySize);
        WriteImage(c, outputFile, binary, binarySize);
    }
    else {
        WriteImage(c, outputFile, image, imageSize);
    }
    
    if (runProgram)
        Execute((ImageHdr *)image, VMFALSE);
    
    return 0;
}
  
static void Usage(void)
{
    printf("\
usage: adv2com [ -d ] [ -o <output-file> ] [ -t <template-name> ] [ -s ] [ -r ] <input-file>\n\
       templates: run, step, wordfire\n");
    exit(1);
}

static uint8_t *BuildImage(ParseContext *c, int *pSize)
{
    int dataSize = c->dataFree - c->dataBuf;
    int stringSize = c->stringFree - c->stringBuf;
    int codeSize = c->codeFree - c->codeBuf;
    int imageSize = sizeof(ImageHdr) + dataSize + codeSize + stringSize;
    ImageHdr *hdr;
    Symbol *sym;
    
    if (!(hdr = (ImageHdr *)malloc(imageSize)))
        ParseError(c, "insufficient memory to build image");
    hdr->dataOffset = sizeof(ImageHdr);
    hdr->dataSize = dataSize;
    hdr->stringOffset = hdr->dataOffset + hdr->dataSize;
    hdr->stringSize = stringSize;
    hdr->codeOffset = hdr->stringOffset + stringSize;
    hdr->codeSize = codeSize;
    
    memcpy((uint8_t *)hdr + sizeof(ImageHdr), c->dataBuf, dataSize);
    memcpy((uint8_t *)hdr + sizeof(ImageHdr) + dataSize, c->stringBuf, stringSize);
    memcpy((uint8_t *)hdr + sizeof(ImageHdr) + dataSize + stringSize, c->codeBuf, codeSize);
    
    if (!(sym = FindSymbol(c, "main")))
        ParseError(c, "no 'main' function");
    else if (!sym->valueDefined)
        ParseError(c, "'main' not defined");
    else if (sym->storageClass != SC_FUNCTION)
        ParseError(c, "expecting 'main' to be a function");
    hdr->mainFunction = sym->v.value;
    
    *pSize = imageSize;
    return (uint8_t *)hdr;
}

static void WriteImage(ParseContext *c, char *name, uint8_t *image, int imageSize)
{
    FILE *fp;
        
    if (!(fp = fopen(name, "wb")))
        ParseError(c, "can't create file '%s'", name);
        
    if (fwrite(image, 1, imageSize, fp) != imageSize)
        ParseError(c, "error writing image file");
        
    fclose(fp);
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
                *(VMVALUE *)&c->dataBuf[fixup->v.offset] = value;
                break;
            case FT_CODE:
                wr_clong(c, fixup->v.offset, value);
                break;
            case FT_PTR:
                // never reached
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
    fixup->v.offset = offset;
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

/* AddStringRef - add a string reference */
void AddStringRef(ParseContext *c, String *string, FixupType fixupType, VMVALUE offset)
{
    Fixup *fixup = (Fixup *)LocalAlloc(c, sizeof(Fixup));
    fixup->type = fixupType;
    fixup->v.offset = offset;
    fixup->next = string->fixups;
    string->fixups = fixup;
}

/* AddStringPtrRef - add a string reference */
void AddStringPtrRef(ParseContext *c, String *string, VMVALUE *pOffset)
{
    Fixup *fixup = (Fixup *)LocalAlloc(c, sizeof(Fixup));
    fixup->type = FT_PTR;
    fixup->v.pOffset = pOffset;
    fixup->next = string->fixups;
    string->fixups = fixup;
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

    /* allocate the string structure */
    size = strlen(value) + 1;
    str = (String *)LocalAlloc(c, sizeof(String) + size - 1);
    memset(str, 0, sizeof(String));
    str->size = size;
    strcpy(str->data, value);
    *c->pNextString = str;
    c->pNextString = &str->next;

    /* return the string table entry */
    return str;
}

/* PlaceStrings - place strings at data space offsets */
void PlaceStrings(ParseContext *c)
{
    String *str;
    Fixup *fixup, *nextFixup;
    
    /* fixup data and code string references */
    for (str = c->strings; str != NULL; str = str->next) {
        str->offset = c->dataFree - c->dataBuf;
        if (c->dataFree + str->size > c->dataTop)
            ParseError(c, "insufficient data space");
        memcpy(c->dataFree, str->data, str->size);
        c->dataFree += str->size;
        for (fixup = str->fixups; fixup != NULL; fixup = nextFixup) {
            nextFixup = fixup->next;
            switch (fixup->type) {
            case FT_DATA:
                *(VMVALUE *)&c->dataBuf[fixup->v.offset] = str->offset;
                break;
            case FT_CODE:
                wr_clong(c, fixup->v.offset, str->offset);
                break;
            case FT_PTR:
                *fixup->v.pOffset = str->offset;
                break;
            }
            free(fixup);
        }
    }
}

static void PrintStrings(ParseContext *c)
{
    String *str;
    for (str = c->strings; str != NULL; str = str->next)
        printf("%d '%s'\n", str->offset, c->dataBuf + str->offset);
}

/* LocalAlloc - allocate memory from the local heap */
void *LocalAlloc(ParseContext *c, size_t size)
{
    void *data = (void *)malloc(size);
    if (!data) Abort(c, "insufficient memory - needed %d bytes", size);
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
