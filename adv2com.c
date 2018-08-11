/* adv2com.c - a simple compiler for a c-like adventure authoring language
 *
 * Copyright (c) 2018 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include "adv2compiler.h"
#include "adv2debug.h"
#include "adv2vmdebug.h"
#include "adv2vm.h"

int main(int argc, char *argv[])
{
    ParseContext context;
    ParseContext *c = &context;
    ParseTreeNode *node;
    
    memset(c, 0, sizeof(ParseContext));
    InitSymbolTable(&c->globals);
    InitScan(c);
    
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
    
    node = ParseFunction(c);
    PrintNode(node, 0);
    code_functiondef(c, node);
    DecodeFunction(c->codeBuf, c->codeFree - c->codeBuf);
    Execute(c->codeBuf);

    return 0;
}

/* AddGlobal - add a global symbol to the symbol table */
Symbol *AddGlobal(ParseContext *c, const char *name, StorageClass storageClass, VMVALUE value)
{
    int size = sizeof(Symbol) + strlen(name);
    Symbol *sym;
    
    /* check to see if the symbol is already defined */
    for (sym = c->globals.head; sym != NULL; sym = sym->next)
        if (strcmp(name, sym->name) == 0)
            return sym;
    
    /* allocate the symbol structure */
    sym = (Symbol *)LocalAlloc(c, size);
    sym->storageClass = storageClass;
    strcpy(sym->name, name);
    sym->value = value;
    sym->next = NULL;

    /* add it to the symbol table */
    *c->globals.pTail = sym;
    c->globals.pTail = &sym->next;
    ++c->globals.count;
    
    /* return the symbol */
    return sym;
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
