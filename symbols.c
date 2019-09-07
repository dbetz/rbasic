/* symbols.c - symbol table routines
 *
 * Copyright (c) 2019 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compile.h"

/* local function prototypes */
static Symbol *AddLocalSymbol(ParseContext *c, SymbolTable *table, const char *name, StorageClass storageClass, int value);
static Symbol *FindSymbol(SymbolTable *table, const char *name);

/* InitSymbolTable - initialize a symbol table */
void InitSymbolTable(SymbolTable *table)
{
    table->head = NULL;
    table->pTail = &table->head;
    table->count = 0;
}

/* AddGlobal - add a global symbol to the symbol table */
Symbol *AddGlobal(ParseContext *c, const char *name, StorageClass storageClass, VMVALUE value)
{
    size_t size = sizeof(Symbol) + strlen(name);
    Symbol *sym;
    
    /* allocate the symbol structure */
    sym = (Symbol *)GlobalAlloc(c, size);
    strcpy(sym->name, name);
    sym->storageClass = storageClass;
    sym->v.value = value;
    sym->next = NULL;

    /* add it to the symbol table */
    *c->globals.pTail = sym;
    c->globals.pTail = &sym->next;
    ++c->globals.count;
    
    /* return the symbol */
    return sym;
}

/* FindGlobal - find a symbol in the global symbol table */
Symbol *FindGlobal(ParseContext *c, const char *name)
{
    return FindSymbol(&c->globals, name);
}

/* AddArgument - add an argument symbol to symbol table */
Symbol *AddArgument(ParseContext *c, const char *name, StorageClass storageClass, int value)
{
    return AddLocalSymbol(c, &c->functionNode->u.functionDefinition.arguments, name, storageClass, value);
}

/* FindArgument - find a symbol in the argument symbol table */
Symbol *FindArgument(ParseContext *c, const char *name)
{
    return c->functionNode ? FindSymbol(&c->functionNode->u.functionDefinition.arguments, name) : NULL;
}

/* AddLocal - add a local symbol to the symbol table */
Symbol *AddLocal(ParseContext *c, const char *name, StorageClass storageClass, int value)
{
    return AddLocalSymbol(c, &c->functionNode->u.functionDefinition.locals, name, storageClass, value);
}

/* FindLocal - find a symbol in the local symbol table */
Symbol *FindLocal(ParseContext *c, const char *name)
{
    return c->functionNode ? FindSymbol(&c->functionNode->u.functionDefinition.locals, name) : NULL;
}

/* AddLocalSymbol - add a symbol to a local symbol table */
static Symbol *AddLocalSymbol(ParseContext *c, SymbolTable *table, const char *name, StorageClass storageClass, int value)
{
    size_t size = sizeof(Symbol) + strlen(name);
    Symbol *sym;
    
    /* allocate the symbol structure */
    sym = (Symbol *)LocalAlloc(c, size);
    strcpy(sym->name, name);
    sym->storageClass = storageClass;
    sym->v.value = value;
    sym->next = NULL;

    /* add it to the symbol table */
    *table->pTail = sym;
    table->pTail = &sym->next;
    ++table->count;
    
    /* return the symbol */
    return sym;
}

/* FindSymbol - find a symbol in a symbol table */
static Symbol *FindSymbol(SymbolTable *table, const char *name)
{
    Symbol *sym = table->head;
    while (sym) {
        if (strcasecmp(name, sym->name) == 0)
            return sym;
        sym = sym->next;
    }
    return NULL;
}

/* DumpSymbols - dump a symbol table */
void DumpSymbols(SymbolTable *table, const char *tag)
{
    Symbol *sym;
    if ((sym = table->head) != NULL) {
        VM_printf("%s:\n", tag);
        for (; sym != NULL; sym = sym->next)
            VM_printf("  %s %08x\n", sym->name, sym->v.value);
    }
}
