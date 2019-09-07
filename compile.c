/* compile.c - compiler
 *
 * Copyright (c) 2019 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdlib.h>
#include "compile.h"

/* Compile - compile a program */
VMVALUE Compile(System *sys)
{
    uint8_t *freeMark;
    VMVALUE mainCode;
    ParseContext *c;
    ParseTreeNode *main;

    /* mark the current position in free space to allow compiler data structures to be freed */
    freeMark = sys->freeNext;

    /* allocate and initialize the parse context */
    if (!(c = (ParseContext *)AllocateFreeSpace(sys, sizeof(ParseContext))))
        return NIL;
    memset(c, 0, sizeof(ParseContext));
    c->sys = sys;
    c->freeMark = freeMark;
    
    /* setup an error target */
    if (setjmp(c->sys->errorTarget) != 0) {
        sys->freeNext = c->freeMark;
        return NIL;
    }

    /* use the rest of the free space for the compiler heap */
    c->nextGlobal = sys->freeNext;
    c->nextLocal = sys->freeTop;
    c->heapSize = sys->freeTop - sys->freeNext;
    c->maxHeapUsed = 0;

    /* initialize block nesting table */
    c->btop = (Block *)((char *)c->blockBuf + sizeof(c->blockBuf));
    c->bptr = &c->blockBuf[0] - 1;

    /* initialize the string and label tables */
    c->strings = NULL;
    c->labels = NULL;

    /* initialize the global symbol table */
    InitSymbolTable(&c->globals);

    /* initialize scanner */
    InitScanner(c);
    
    /* initialize the parser */
    main = InitParser(c);
    
    /* parse each statement */
    while (GetLine(c->sys)) {
        int tkn;
        if ((tkn = GetToken(c)) != T_EOL) {
            ParseStatement(c, tkn);
        }    
    }
    
    PrintNode(main, 0);

    /* free up the space the compiler was consuming */
    sys->freeNext = c->freeMark;

    /* return the main function */
    return mainCode;
}

