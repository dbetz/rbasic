/* parse.c - expression parser
 *
 * Copyright (c) 2019 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdlib.h>
#include "compile.h"

/* local function prototypes */
static void ParseDef(ParseContext *c);
static void ParseConstantDef(ParseContext *c, char *name);
static void ParseFunctionDef(ParseContext *c, char *name);
static void ParseEndDef(ParseContext *c);
static void ParseDim(ParseContext *c);
static int ParseVariableDecl(ParseContext *c, char *name, VMVALUE *pSize);
static VMVALUE ParseScalarInitializer(ParseContext *c);
static void ParseArrayInitializers(ParseContext *c, VMVALUE size);
static void ClearArrayInitializers(ParseContext *c, VMVALUE size);
static void ParseImpliedLetOrFunctionCall(ParseContext *c);
static void ParseLet(ParseContext *c);
static void ParseIf(ParseContext *c);
static void ParseElse(ParseContext *c);
static void ParseElseIf(ParseContext *c);
static void ParseEndIf(ParseContext *c);
static void ParseFor(ParseContext *c);
static void ParseNext(ParseContext *c);
static void ParseDo(ParseContext *c);
static void ParseDoWhile(ParseContext *c);
static void ParseDoUntil(ParseContext *c);
static void ParseLoop(ParseContext *c);
static void ParseLoopWhile(ParseContext *c);
static void ParseLoopUntil(ParseContext *c);
static void ParseReturn(ParseContext *c);
static void PushBlock(ParseContext *c, BlockType type, ParseTreeNode *node, NodeListEntry **pNodeList);
static void PopBlock(ParseContext *c);
static ParseTreeNode *ParseExpr2(ParseContext *c);
static ParseTreeNode *ParseExpr3(ParseContext *c);
static ParseTreeNode *ParseExpr4(ParseContext *c);
static ParseTreeNode *ParseExpr5(ParseContext *c);
static ParseTreeNode *ParseExpr6(ParseContext *c);
static ParseTreeNode *ParseExpr7(ParseContext *c);
static ParseTreeNode *ParseExpr8(ParseContext *c);
static ParseTreeNode *ParseExpr9(ParseContext *c);
static ParseTreeNode *ParseExpr10(ParseContext *c);
static ParseTreeNode *ParseExpr11(ParseContext *c);
static ParseTreeNode *ParseSimplePrimary(ParseContext *c);
static ParseTreeNode *ParseArrayReference(ParseContext *c, ParseTreeNode *arrayNode);
static ParseTreeNode *ParseCall(ParseContext *c, ParseTreeNode *functionNode);
static ParseTreeNode *MakeUnaryOpNode(ParseContext *c, int op, ParseTreeNode *expr);
static ParseTreeNode *MakeBinaryOpNode(ParseContext *c, int op, ParseTreeNode *left, ParseTreeNode *right);
static ParseTreeNode *NewParseTreeNode(ParseContext *c, int type);
void AddNodeToList(ParseContext *c, NodeListEntry ***ppNextEntry, ParseTreeNode *node);

/* InitParser - initialize the parser */
ParseTreeNode *InitParser(ParseContext *c)
{   
    ParseTreeNode *main = NewParseTreeNode(c, NodeTypeMain);
    PushBlock(c, BLOCK_MAIN, main, &main->u.main.bodyStatements);
    return main;
}

/* ParseStatement - parse a statement */
void ParseStatement(ParseContext *c, int tkn)
{
    /* dispatch on the statement keyword */
    switch (tkn) {
    case T_REM:
        /* just a comment so ignore the rest of the line */
        break;
    case T_DEF:
        ParseDef(c);
        break;
    case T_END_DEF:
        ParseEndDef(c);
        break;
    case T_DIM:
        ParseDim(c);
        break;
    case T_LET:
        ParseLet(c);
        break;
    case T_IF:
        ParseIf(c);
        break;
    case T_ELSE:
        ParseElse(c);
        break;
    case T_ELSE_IF:
        ParseElseIf(c);
        break;
    case T_END_IF:
        ParseEndIf(c);
        break;
    case T_FOR:
        ParseFor(c);
        break;
    case T_NEXT:
        ParseNext(c);
        break;
    case T_DO:
        ParseDo(c);
        break;
    case T_DO_WHILE:
        ParseDoWhile(c);
        break;
    case T_DO_UNTIL:
        ParseDoUntil(c);
        break;
    case T_LOOP:
        ParseLoop(c);
        break;
    case T_LOOP_WHILE:
        ParseLoopWhile(c);
        break;
    case T_LOOP_UNTIL:
        ParseLoopUntil(c);
        break;
    case T_RETURN:
        ParseReturn(c);
        break;
    default:
        SaveToken(c, tkn);
        ParseImpliedLetOrFunctionCall(c);
        break;
    }
}

/* ParseDef - parse the 'DEF' statement */
static void ParseDef(ParseContext *c)
{
    char name[MAXTOKEN];
    int tkn;

    /* get the name being defined */
    FRequire(c, T_IDENTIFIER);
    strcpy(name, c->token);

    /* check for a constant definition */
    if ((tkn = GetToken(c)) == '=')
        ParseConstantDef(c, name);

    /* otherwise, assume a function definition */
    else {
        SaveToken(c, tkn);
        ParseFunctionDef(c, name);
    }
}

/* ParseConstantDef - parse a 'DEF <name> =' statement */
static void ParseConstantDef(ParseContext *c, char *name)
{
    ParseTreeNode *expr;

    /* get the constant value */
    expr = ParseExpr(c);

    /* make sure it's a constant */
    if (!IsIntegerLit(expr))
        ParseError(c, "expecting a constant expression");

    /* add the symbol as a global */
    AddGlobal(c, name, SC_CONSTANT, expr->u.integerLit.value);

    FRequire(c, T_EOL);
}

/* ParseFunctionDef - parse a 'DEF <name> =' statement */
static void ParseFunctionDef(ParseContext *c, char *name)
{
    ParseTreeNode *node;
    Symbol *symbol;
    int tkn;
    
    /* make the function node */
    node = NewParseTreeNode(c, NodeTypeFunctionDefinition);
    node->u.functionDefinition.symbol = AddGlobal(c, name, SC_CONSTANT, 0);
    InitSymbolTable(&node->u.functionDefinition.arguments);
    InitSymbolTable(&node->u.functionDefinition.locals);
    node->u.functionDefinition.localOffset = 0;
    PushBlock(c, BLOCK_FUNCTION, node, &node->u.functionDefinition.bodyStatements);
    c->functionNode = node;
    
    /* get the argument list */
    if ((tkn = GetToken(c)) == '(') {
        if ((tkn = GetToken(c)) != ')') {
            int offset = 0;
            SaveToken(c, tkn);
            do {
                FRequire(c, T_IDENTIFIER);
                AddArgument(c, c->token, SC_VARIABLE, offset);
                ++offset;
            } while ((tkn = GetToken(c)) == ',');
        }
        Require(c, tkn, ')');
    }
    else
        SaveToken(c, tkn);

    FRequire(c, T_EOL);
}

/* ParseEndDef - parse the 'END DEF' statement */
static void ParseEndDef(ParseContext *c)
{
    if (c->functionNode == NULL)
        ParseError(c, "not in a function definition");
    PrintNode(c->functionNode, 0);
    c->functionNode = NULL;
}

/* ParseDim - parse the 'DIM' statement */
static void ParseDim(ParseContext *c)
{
    char name[MAXTOKEN];
    VMVALUE value, size = 0;
    int isArray;
    int tkn;

    /* parse variable declarations */
    do {

        /* get variable name */
        isArray = ParseVariableDecl(c, name, &size);

        /* add to the global symbol table if outside a function definition */
        if (c->functionNode == NULL) {
            Symbol *sym;

            /* check for initializers */
            if ((tkn = GetToken(c)) == '=') {
                if (isArray)
                    ParseArrayInitializers(c, size);
                else {
                    VMVALUE *dataPtr = (VMVALUE *)c->codeBuf;
                    if (dataPtr >= (VMVALUE *)c->ctop)
                        ParseError(c, "insufficient image space");
                    *dataPtr = ParseScalarInitializer(c);
                }
            }

            /* no initializers */
            else {
                ClearArrayInitializers(c, isArray ? size : 1);
                SaveToken(c, tkn);
            }

            /* allocate space for the data */
            value = (VMVALUE)c->image->free;
            c->image->free += size;
            
            /* add the symbol to the global symbol table */
            sym = AddGlobal(c, name, SC_VARIABLE, value);
    
            /* reinitialize the code buffer */
            InitCodeBuffer(c);
        }

        /* otherwise, add to the local symbol table */
        else {
            if (isArray)
                ParseError(c, "local arrays are not supported");
            AddLocal(c, name, SC_VARIABLE, c->functionNode->u.functionDefinition.localOffset);
            ++c->functionNode->u.functionDefinition.localOffset;
        }

    } while ((tkn = GetToken(c)) == ',');

    Require(c, tkn, T_EOL);
}

/* ParseVariableDecl - parse a variable declaration */
static int ParseVariableDecl(ParseContext *c, char *name, VMVALUE *pSize)
{
    int isArray;
    int tkn;

    /* parse the variable name */
    FRequire(c, T_IDENTIFIER);
    strcpy(name, c->token);

    /* handle arrays */
    if ((tkn = GetToken(c)) == '[') {

        /* check for an array with unspecified size */
        if ((tkn = GetToken(c)) == ']')
            *pSize = 0;

        /* otherwise, parse the array size */
        else {
            ParseTreeNode *expr;

            /* put back the token */
            SaveToken(c, tkn);

            /* get the array size */
            expr = ParseExpr(c);

            /* make sure it's a constant */
            if (!IsIntegerLit(expr) || expr->u.integerLit.value <= 0)
                ParseError(c, "expecting a positive constant expression");
            *pSize = expr->u.integerLit.value;

            /* only one dimension allowed for now */
            FRequire(c, ']');
        }

        /* return an array and its size */
        isArray = VMTRUE;
        return VMTRUE;
    }

    /* not an array */
    else {
        SaveToken(c, tkn);
        isArray = VMFALSE;
        *pSize = 1;
    }

    /* return array indicator */
    return isArray;
}

/* ParseScalarInitializer - parse a scalar initializer */
static VMVALUE ParseScalarInitializer(ParseContext *c)
{
    ParseTreeNode *expr = ParseExpr(c);
    if (!IsIntegerLit(expr))
        ParseError(c, "expecting a constant expression");
    return expr->u.integerLit.value;
}

/* ParseArrayInitializers - parse array initializers */
static void ParseArrayInitializers(ParseContext *c, VMVALUE size)
{
    VMVALUE *dataPtr = (VMVALUE *)c->codeBuf;
    VMVALUE *dataTop = (VMVALUE *)c->ctop;
    int done = VMFALSE;
    int tkn;

    FRequire(c, '{');

    /* handle each line of initializers */
    while (!done) {
        int lineDone = VMFALSE;

        /* look for the first non-blank line */
        while ((tkn = GetToken(c)) == T_EOL) {
            if (!GetLine(c->sys))
                ParseError(c, "unexpected end of file in initializers");
        }

        /* check for the end of the initializers */
        if (tkn == '}')
            break;
        SaveToken(c, tkn);

        /* handle each initializer in the current line */
        while (!lineDone) {
            VMVALUE value;

            /* get a constant expression */
            value = ParseScalarInitializer(c);

            /* check for too many initializers */
            if (--size < 0)
                ParseError(c, "too many initializers");

            /* store the initial value */
            if (dataPtr >= dataTop)
                ParseError(c, "insufficient image space");
            *dataPtr++ = value;

            switch (tkn = GetToken(c)) {
            case T_EOL:
                lineDone = VMTRUE;
                break;
            case '}':
                lineDone = VMTRUE;
                done = VMTRUE;
                break;
            case ',':
                break;
            default:
                ParseError(c, "expecting a comma, right brace or end of line");
                break;
            }

        }
    }
}

/* ClearArrayInitializers - clear the array initializers */
static void ClearArrayInitializers(ParseContext *c, VMVALUE size)
{
    VMVALUE *dataPtr = (VMVALUE *)c->codeBuf;
    VMVALUE *dataTop = (VMVALUE *)c->ctop;
    if (dataPtr + size > dataTop)
        ParseError(c, "insufficient image space");
    memset(dataPtr, 0, size * sizeof(VMVALUE));
}

/* ParseImpliedLetOrFunctionCall - parse an implied let statement or a function call */
static void ParseImpliedLetOrFunctionCall(ParseContext *c)
{
    ParseTreeNode *node, *expr;
    int tkn;
    expr = ParsePrimary(c);
    switch (tkn = GetToken(c)) {
    case '=':
        node = NewParseTreeNode(c, NodeTypeLetStatement);
        node->u.letStatement.lvalue = expr;
        node->u.letStatement.rvalue = ParseExpr(c);
        break;
    default:
        SaveToken(c, tkn);
        node = NewParseTreeNode(c, NodeTypeCallStatement);
        node->u.callStatement.expr = expr;
        break;
    }
    AddNodeToList(c, &c->bptr->pNextStatement, node);
    FRequire(c, T_EOL);
}

/* ParseLet - parse the 'LET' statement */
static void ParseLet(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeLetStatement);
    node->u.letStatement.lvalue = ParsePrimary(c);
    FRequire(c, '=');
    node->u.letStatement.rvalue = ParseExpr(c);
    AddNodeToList(c, &c->bptr->pNextStatement, node);
    FRequire(c, T_EOL);
}

/* ParseIf - parse the 'IF' statement */
static void ParseIf(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeIfStatement);
    int tkn;
    node->u.ifStatement.test = ParseExpr(c);
    AddNodeToList(c, &c->bptr->pNextStatement, node);
    FRequire(c, T_THEN);
    PushBlock(c, BLOCK_IF, node, &node->u.ifStatement.thenStatements);
    if ((tkn = GetToken(c)) != T_EOL) {
        ParseStatement(c, tkn);
        PopBlock(c);
    }
    Require(c, tkn, T_EOL);
}

/* ParseElseIf - parse the 'ELSE IF' statement */
static void ParseElseIf(ParseContext *c)
{
    NodeListEntry **pNext;
    ParseTreeNode *node;
    switch (c->bptr->type) {
    case BLOCK_IF:
        node = NewParseTreeNode(c, NodeTypeIfStatement);
        pNext = &c->bptr->node->u.ifStatement.elseStatements;
        AddNodeToList(c, &pNext, node);
        c->bptr->node = node;
        node->u.ifStatement.test = ParseExpr(c);
        c->bptr->pNextStatement = &node->u.ifStatement.thenStatements;
        FRequire(c, T_THEN);
        FRequire(c, T_EOL);
        break;
    default:
        ParseError(c, "ELSE IF without a matching IF");
        break;
    }
}

/* ParseElse - parse the 'ELSE' statement */
static void ParseElse(ParseContext *c)
{
    switch (c->bptr->type) {
    case BLOCK_IF:
        c->bptr->type = BLOCK_ELSE;
        c->bptr->pNextStatement = &c->bptr->node->u.ifStatement.elseStatements;
        FRequire(c, T_EOL);
        break;
    default:
        ParseError(c, "ELSE without a matching IF");
        break;
    }
}

/* ParseEndIf - parse the 'END IF' statement */
static void ParseEndIf(ParseContext *c)
{
    switch (c->bptr->type) {
    case BLOCK_IF:
    case BLOCK_ELSE:
        PopBlock(c);
        break;
    default:
        ParseError(c, "END IF without a matching IF/ELSE IF/ELSE");
        break;
    }
    FRequire(c, T_EOL);
}

/* ParseFor - parse the 'FOR' statement */
static void ParseFor(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeForStatement);
    int tkn;

    AddNodeToList(c, &c->bptr->pNextStatement, node);

    PushBlock(c, BLOCK_FOR, node, &node->u.forStatement.bodyStatements);

    /* get the control variable */
    FRequire(c, T_IDENTIFIER);
    node->u.forStatement.var = GetSymbolRef(c, c->token);

    /* parse the starting value expression */
    FRequire(c, '=');
    node->u.forStatement.startExpr = ParseExpr(c);

    /* parse the TO expression and generate the loop termination test */
    FRequire(c, T_TO);
    node->u.forStatement.endExpr = ParseExpr(c);

    /* get the STEP expression */
    if ((tkn = GetToken(c)) == T_STEP) {
        node->u.forStatement.stepExpr = ParseExpr(c);
        tkn = GetToken(c);
    }
    Require(c, tkn, T_EOL);
}

/* ParseNext - parse the 'NEXT' statement */
static void ParseNext(ParseContext *c)
{
    switch (c->bptr->type) {
    case BLOCK_FOR:
        FRequire(c, T_IDENTIFIER);
        //if (GetSymbolRef(c, c->token) != c->bptr->node->u.forStatement.var)
        //    ParseError(c, "wrong variable in FOR");
        PopBlock(c);
        break;
    default:
        ParseError(c, "NEXT without a matching FOR");
        break;
    }
    FRequire(c, T_EOL);
}

/* ParseDo - parse the 'DO' statement */
static void ParseDo(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeLoopStatement);
    node->u.loopStatement.test = NULL;
    AddNodeToList(c, &c->bptr->pNextStatement, node);
    PushBlock(c, BLOCK_DO, node, &node->u.loopStatement.bodyStatements);
    FRequire(c, T_EOL);
}

/* ParseDoWhile - parse the 'DO WHILE' statement */
static void ParseDoWhile(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeDoWhileStatement);
    node->u.loopStatement.test = ParseExpr(c);
    AddNodeToList(c, &c->bptr->pNextStatement, node);
    PushBlock(c, BLOCK_DO, node, &node->u.loopStatement.bodyStatements);
    FRequire(c, T_EOL);
}

/* ParseDoUntil - parse the 'DO UNTIL' statement */
static void ParseDoUntil(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeDoUntilStatement);
    node->u.loopStatement.test = ParseExpr(c);
    AddNodeToList(c, &c->bptr->pNextStatement, node);
    PushBlock(c, BLOCK_DO, node, &node->u.loopStatement.bodyStatements);
    FRequire(c, T_EOL);
}

/* ParseLoop - parse the 'LOOP' statement */
static void ParseLoop(ParseContext *c)
{
    switch (c->bptr->type) {
    case BLOCK_DO:
        PopBlock(c);
        break;
    default:
        ParseError(c, "LOOP without a matching DO");
        break;
    }
    FRequire(c, T_EOL);
}

/* ParseLoopWhile - parse the 'LOOP WHILE' statement */
static void ParseLoopWhile(ParseContext *c)
{
    switch (c->bptr->type) {
    case BLOCK_DO:
        if (c->bptr->node->nodeType != NodeTypeLoopStatement)
            ParseError(c, "can't have a test at both the top and bottom of a loop");
        c->bptr->node->nodeType = NodeTypeLoopWhileStatement;
        c->bptr->node->u.loopStatement.test = ParseExpr(c);
        PopBlock(c);
        break;
    default:
        ParseError(c, "LOOP without a matching DO");
        break;
    }
    FRequire(c, T_EOL);
}

/* ParseLoopUntil - parse the 'LOOP UNTIL' statement */
static void ParseLoopUntil(ParseContext *c)
{
    switch (c->bptr->type) {
    case BLOCK_DO:
        if (c->bptr->node->nodeType != NodeTypeLoopStatement)
            ParseError(c, "can't have a test at both the top and bottom of a loop");
        c->bptr->node->nodeType = NodeTypeLoopUntilStatement;
        c->bptr->node->u.loopStatement.test = ParseExpr(c);
        PopBlock(c);
        break;
    default:
        ParseError(c, "LOOP without a matching DO");
        break;
    }
    FRequire(c, T_EOL);
}

/* ParseReturn - parse the 'RETURN' statement */
static void ParseReturn(ParseContext *c)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeReturnStatement);
    int tkn;
            
    /* return with no value returns zero */
    if ((tkn = GetToken(c)) == T_EOL)
        node->u.returnStatement.expr = NULL;
        
    /* handle return with a value */
    else {
        SaveToken(c, tkn);
        node->u.returnStatement.expr = ParseExpr(c);
        FRequire(c, T_EOL);
    }
    
    /* add the statement to the current function */
    AddNodeToList(c, &c->bptr->pNextStatement, node);
}

/* PushBlock - push a block on the stack */
static void PushBlock(ParseContext *c, BlockType type, ParseTreeNode *node, NodeListEntry **pNodeList)
{
    if (++c->bptr >= c->btop)
        ParseError(c, "statements too deeply nested");
    c->bptr->type = type;
    c->bptr->node = node;
    c->bptr->pNextStatement = pNodeList;
}

/* PopBlock - pop a block off the stack */
static void PopBlock(ParseContext *c)
{
    --c->bptr;
}

/* ParseExpr - handle the OR operator */
ParseTreeNode *ParseExpr(ParseContext *c)
{
    ParseTreeNode *node;
    int tkn;
    node = ParseExpr2(c);
    if ((tkn = GetToken(c)) == T_OR) {
        ParseTreeNode *node2 = NewParseTreeNode(c, NodeTypeDisjunction);
        NodeListEntry *entry, **pLast;
        node2->u.exprList.exprs = entry = (NodeListEntry *)LocalAlloc(c, sizeof(NodeListEntry));
        entry->node = node;
        entry->next = NULL;
        pLast = &entry->next;
        do {
            entry = (NodeListEntry *)LocalAlloc(c, sizeof(NodeListEntry));
            entry->node = ParseExpr2(c);
            entry->next = NULL;
            *pLast = entry;
            pLast = &entry->next;
        } while ((tkn = GetToken(c)) == T_OR);
        node = node2;
    }
    SaveToken(c, tkn);
    return node;
}

/* ParseExpr2 - handle the AND operator */
static ParseTreeNode *ParseExpr2(ParseContext *c)
{
    ParseTreeNode *node;
    int tkn;
    node = ParseExpr3(c);
    if ((tkn = GetToken(c)) == T_AND) {
        ParseTreeNode *node2 = NewParseTreeNode(c, NodeTypeConjunction);
        NodeListEntry *entry, **pLast;
        node2->u.exprList.exprs = entry = (NodeListEntry *)LocalAlloc(c, sizeof(NodeListEntry));
        entry->node = node;
        entry->next = NULL;
        pLast = &entry->next;
        do {
            entry = (NodeListEntry *)LocalAlloc(c, sizeof(NodeListEntry));
            entry->node = ParseExpr2(c);
            entry->next = NULL;
            *pLast = entry;
            pLast = &entry->next;
        } while ((tkn = GetToken(c)) == T_AND);
        node = node2;
    }
    SaveToken(c, tkn);
    return node;
}

/* ParseExpr3 - handle the BXOR operator */
static ParseTreeNode *ParseExpr3(ParseContext *c)
{
    ParseTreeNode *expr, *expr2;
    int tkn;
    expr = ParseExpr4(c);
    while ((tkn = GetToken(c)) == '^') {
        expr2 = ParseExpr4(c);
        if (IsIntegerLit(expr) && IsIntegerLit(expr2))
            expr->u.integerLit.value = expr->u.integerLit.value ^ expr2->u.integerLit.value;
        else
            expr = MakeBinaryOpNode(c, OP_BXOR, expr, expr2);
    }
    SaveToken(c,tkn);
    return expr;
}

/* ParseExpr4 - handle the BOR operator */
static ParseTreeNode *ParseExpr4(ParseContext *c)
{
    ParseTreeNode *expr, *expr2;
    int tkn;
    expr = ParseExpr5(c);
    while ((tkn = GetToken(c)) == '|') {
        expr2 = ParseExpr5(c);
        if (IsIntegerLit(expr) && IsIntegerLit(expr2))
            expr->u.integerLit.value = expr->u.integerLit.value | expr2->u.integerLit.value;
        else
            expr = MakeBinaryOpNode(c, OP_BOR, expr, expr2);
    }
    SaveToken(c,tkn);
    return expr;
}

/* ParseExpr5 - handle the BAND operator */
static ParseTreeNode *ParseExpr5(ParseContext *c)
{
    ParseTreeNode *expr, *expr2;
    int tkn;
    expr = ParseExpr6(c);
    while ((tkn = GetToken(c)) == '&') {
        expr2 = ParseExpr6(c);
        if (IsIntegerLit(expr) && IsIntegerLit(expr2))
            expr->u.integerLit.value = expr->u.integerLit.value & expr2->u.integerLit.value;
        else
            expr = MakeBinaryOpNode(c, OP_BAND, expr, expr2);
    }
    SaveToken(c,tkn);
    return expr;
}

/* ParseExpr6 - handle the '=' and '<>' operators */
static ParseTreeNode *ParseExpr6(ParseContext *c)
{
    ParseTreeNode *expr, *expr2;
    int tkn;
    expr = ParseExpr7(c);
    while ((tkn = GetToken(c)) == '=' || tkn == T_NE) {
        int op;
        expr2 = ParseExpr7(c);
        switch (tkn) {
        case '=':
            op = OP_EQ;
            break;
        case T_NE:
            op = OP_NE;
            break;
        default:
            /* not reached */
            op = 0;
            break;
        }
        expr = MakeBinaryOpNode(c, op, expr, expr2);
    }
    SaveToken(c,tkn);
    return expr;
}

/* ParseExpr7 - handle the '<', '<=', '>=' and '>' operators */
static ParseTreeNode *ParseExpr7(ParseContext *c)
{
    ParseTreeNode *expr, *expr2;
    int tkn;
    expr = ParseExpr8(c);
    while ((tkn = GetToken(c)) == '<' || tkn == T_LE || tkn == T_GE || tkn == '>') {
        int op;
        expr2 = ParseExpr8(c);
        switch (tkn) {
        case '<':
            op = OP_LT;
            break;
        case T_LE:
            op = OP_LE;
            break;
        case T_GE:
            op = OP_GE;
            break;
        case '>':
            op = OP_GT;
            break;
        default:
            /* not reached */
            op = 0;
            break;
        }
        expr = MakeBinaryOpNode(c, op, expr, expr2);
    }
    SaveToken(c,tkn);
    return expr;
}

/* ParseExpr8 - handle the '<<' and '>>' operators */
static ParseTreeNode *ParseExpr8(ParseContext *c)
{
    ParseTreeNode *expr, *expr2;
    int tkn;
    expr = ParseExpr9(c);
    while ((tkn = GetToken(c)) == T_SHL || tkn == T_SHR) {
        expr2 = ParseExpr9(c);
        if (IsIntegerLit(expr) && IsIntegerLit(expr2)) {
            switch (tkn) {
            case T_SHL:
                expr->u.integerLit.value = expr->u.integerLit.value << expr2->u.integerLit.value;
                break;
            case T_SHR:
                expr->u.integerLit.value = expr->u.integerLit.value >> expr2->u.integerLit.value;
                break;
            default:
                /* not reached */
                break;
            }
        }
        else {
            int op;
            switch (tkn) {
            case T_SHL:
                op = OP_SHL;
                break;
            case T_SHR:
                op = OP_SHR;
                break;
            default:
                /* not reached */
                op = 0;
                break;
            }
            expr = MakeBinaryOpNode(c, op, expr, expr2);
        }
    }
    SaveToken(c,tkn);
    return expr;
}

/* ParseExpr9 - handle the '+' and '-' operators */
static ParseTreeNode *ParseExpr9(ParseContext *c)
{
    ParseTreeNode *expr, *expr2;
    int tkn;
    expr = ParseExpr10(c);
    while ((tkn = GetToken(c)) == '+' || tkn == '-') {
        expr2 = ParseExpr10(c);
        if (IsIntegerLit(expr) && IsIntegerLit(expr2)) {
            switch (tkn) {
            case '+':
                expr->u.integerLit.value = expr->u.integerLit.value + expr2->u.integerLit.value;
                break;
            case '-':
                expr->u.integerLit.value = expr->u.integerLit.value - expr2->u.integerLit.value;
                break;
            default:
                /* not reached */
                break;
            }
        }
        else {
            int op;
            switch (tkn) {
            case '+':
                op = OP_ADD;
                break;
            case '-':
                op = OP_SUB;
                break;
            default:
                /* not reached */
                op = 0;
                break;
            }
            expr = MakeBinaryOpNode(c, op, expr, expr2);
        }
    }
    SaveToken(c, tkn);
    return expr;
}

/* ParseExpr10 - handle the '*', '/' and MOD operators */
static ParseTreeNode *ParseExpr10(ParseContext *c)
{
    ParseTreeNode *node, *node2;
    int tkn;
    node = ParseExpr11(c);
    while ((tkn = GetToken(c)) == '*' || tkn == '/' || tkn == T_MOD) {
        node2 = ParseExpr11(c);
        if (IsIntegerLit(node) && IsIntegerLit(node2)) {
            switch (tkn) {
            case '*':
                node->u.integerLit.value = node->u.integerLit.value * node2->u.integerLit.value;
                break;
            case '/':
                if (node2->u.integerLit.value == 0)
                    ParseError(c, "division by zero in constant expression");
                node->u.integerLit.value = node->u.integerLit.value / node2->u.integerLit.value;
                break;
            case T_MOD:
                if (node2->u.integerLit.value == 0)
                    ParseError(c, "division by zero in constant expression");
                node->u.integerLit.value = node->u.integerLit.value % node2->u.integerLit.value;
                break;
            default:
                /* not reached */
                break;
            }
        }
        else {
            int op;
            switch (tkn) {
            case '*':
                op = OP_MUL;
                break;
            case '/':
                op = OP_DIV;
                break;
            case T_MOD:
                op = OP_REM;
                break;
            default:
                /* not reached */
                op = 0;
                break;
            }
            node = MakeBinaryOpNode(c, op, node, node2);
        }
    }
    SaveToken(c, tkn);
    return node;
}

/* ParseExpr11 - handle unary operators */
static ParseTreeNode *ParseExpr11(ParseContext *c)
{
    ParseTreeNode *node;
    int tkn;
    switch (tkn = GetToken(c)) {
    case '+':
        node = ParsePrimary(c);
        break;
    case '-':
        node = ParsePrimary(c);
        if (IsIntegerLit(node))
            node->u.integerLit.value = -node->u.integerLit.value;
        else
            node = MakeUnaryOpNode(c, OP_NEG, node);
        break;
    case T_NOT:
        node = ParsePrimary(c);
        if (IsIntegerLit(node))
            node->u.integerLit.value = !node->u.integerLit.value;
        else
            node = MakeUnaryOpNode(c, OP_NOT, node);
        break;
    case '~':
        node = ParsePrimary(c);
        if (IsIntegerLit(node))
            node->u.integerLit.value = ~node->u.integerLit.value;
        else
            node = MakeUnaryOpNode(c, OP_BNOT, node);
        break;
    default:
        SaveToken(c,tkn);
        node = ParsePrimary(c);
        break;
    }
    return node;
}

/* ParsePrimary - parse function calls and array references */
ParseTreeNode *ParsePrimary(ParseContext *c)
{
    ParseTreeNode *node;
    int tkn;
    node = ParseSimplePrimary(c);
    while ((tkn = GetToken(c)) == '(' || tkn == '[') {
        switch (tkn) {
        case '[':
            node = ParseArrayReference(c, node);
            break;
        case '(':
            node = ParseCall(c, node);
            break;
        }
    }
    SaveToken(c, tkn);
    return node;
}

/* ParseArrayReference - parse an array reference */
static ParseTreeNode *ParseArrayReference(ParseContext *c, ParseTreeNode *arrayNode)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeArrayRef);

    /* setup the array reference */
    node->u.arrayRef.array = arrayNode;

    /* get the index expression */
    node->u.arrayRef.index = ParseExpr(c);

    /* check for the close bracket */
    FRequire(c, ']');
    return node;
}

/* ParseCall - parse a function or subroutine call */
static ParseTreeNode *ParseCall(ParseContext *c, ParseTreeNode *functionNode)
{
    ParseTreeNode *node = NewParseTreeNode(c, NodeTypeFunctionCall);
    NodeListEntry **pLast;
    int tkn;

    /* intialize the function call node */
    node->u.functionCall.fcn = functionNode;
    pLast = &node->u.functionCall.args;

    /* parse the argument list */
    if ((tkn = GetToken(c)) != ')') {
        SaveToken(c, tkn);
        do {
            NodeListEntry *actual;
            actual = (NodeListEntry *)LocalAlloc(c, sizeof(NodeListEntry));
            actual->node = ParseExpr(c);
            actual->next = NULL;
            *pLast = actual;
            pLast = &actual->next;
            ++node->u.functionCall.argc;
        } while ((tkn = GetToken(c)) == ',');
        Require(c, tkn, ')');
    }

    /* return the function call node */
    return node;
}

/* ParseSimplePrimary - parse a primary expression */
static ParseTreeNode *ParseSimplePrimary(ParseContext *c)
{
    ParseTreeNode *node;
    switch (GetToken(c)) {
    case '(':
        node = ParseExpr(c);
        FRequire(c,')');
        break;
    case T_NUMBER:
        node = NewParseTreeNode(c, NodeTypeIntegerLit);
        node->u.integerLit.value = c->value;
        break;
    case T_STRING:
        node = NewParseTreeNode(c, NodeTypeStringLit);
        node->u.stringLit.string = AddString(c, c->token);
        break;
    case T_IDENTIFIER:
        node = GetSymbolRef(c, c->token);
        break;
    default:
        ParseError(c, "Expecting a primary expression");
        node = NULL; /* not reached */
        break;
    }
    return node;
}

/* GetSymbolRef - setup a symbol reference */
ParseTreeNode *GetSymbolRef(ParseContext *c, const char *name)
{
    ParseTreeNode *node;
    Symbol *symbol;
    /* handle local variables within a function or subroutine */
    if ((symbol = FindLocal(c, name)) != NULL) {
        if (symbol->storageClass == SC_CONSTANT) {
            node = NewParseTreeNode(c, NodeTypeIntegerLit);
            node->u.integerLit.value = symbol->v.value;
        }
        else {
            node = NewParseTreeNode(c, NodeTypeLocalRef);
            node->u.localRef.symbol = symbol;
        }
    }

    /* handle function or subroutine arguments */
    else if ((symbol = FindArgument(c, name)) != NULL) {
        node = NewParseTreeNode(c, NodeTypeArgumentRef);
        node->u.argumentRef.symbol = symbol;
    }

    /* handle global symbols */
    else if ((symbol = FindGlobal(c, c->token)) != NULL) {
        if (symbol->storageClass == SC_CONSTANT) {
            node = NewParseTreeNode(c, NodeTypeIntegerLit);
            node->u.integerLit.value = symbol->v.value;
        }
        else {
            node = NewParseTreeNode(c, NodeTypeGlobalRef);
            node->u.globalRef.symbol = symbol;
        }
    }

    /* handle undefined symbols */
    else {
        symbol = AddGlobal(c, name, SC_VARIABLE, 0);
        node = NewParseTreeNode(c, NodeTypeGlobalRef);
        node->u.globalRef.symbol = symbol;
    }

    /* return the symbol reference node */
    return node;
}

/* MakeUnaryOpNode - allocate a unary operation parse tree node */
static ParseTreeNode *MakeUnaryOpNode(ParseContext *c, int op, ParseTreeNode *expr)
{
    ParseTreeNode *node;
    if (IsIntegerLit(expr)) {
        node = expr;
        switch (op) {
        case OP_NEG:
            node->u.integerLit.value = -expr->u.integerLit.value;
            break;
        case OP_NOT:
            node->u.integerLit.value = !expr->u.integerLit.value;
            break;
        case OP_BNOT:
            node->u.integerLit.value = ~expr->u.integerLit.value;
            break;
        }
    }
    else if (expr->type == TYPE_INTEGER) {
        node = NewParseTreeNode(c, NodeTypeUnaryOp);
        node->type = TYPE_INTEGER;
        node->u.unaryOp.op = op;
        node->u.unaryOp.expr = expr;
    }
    else {
        ParseError(c, "Expecting a numeric expression");
        node = NULL; /* not reached */
    }
    return node;
}

/* MakeBinaryOpNode - allocate a binary operation parse tree node */
static ParseTreeNode *MakeBinaryOpNode(ParseContext *c, int op, ParseTreeNode *left, ParseTreeNode *right)
{
    ParseTreeNode *node;
    if (IsIntegerLit(left) && IsIntegerLit(right)) {
        node = left;
        switch (op) {
        case OP_BXOR:
            node->u.integerLit.value = left->u.integerLit.value ^ right->u.integerLit.value;
            break;
        case OP_BOR:
            node->u.integerLit.value = left->u.integerLit.value | right->u.integerLit.value;
            break;
        case OP_BAND:
            node->u.integerLit.value = left->u.integerLit.value & right->u.integerLit.value;
            break;
        case OP_EQ:
            node->u.integerLit.value = left->u.integerLit.value == right->u.integerLit.value;
            break;
        case OP_NE:
            node->u.integerLit.value = left->u.integerLit.value != right->u.integerLit.value;
            break;
        case OP_LT:
            node->u.integerLit.value = left->u.integerLit.value < right->u.integerLit.value;
            break;
        case OP_LE:
            node->u.integerLit.value = left->u.integerLit.value <= right->u.integerLit.value;
            break;
        case OP_GE:
            node->u.integerLit.value = left->u.integerLit.value >= right->u.integerLit.value;
            break;
        case OP_GT:
            node->u.integerLit.value = left->u.integerLit.value > right->u.integerLit.value;
            break;
        case OP_SHL:
            node->u.integerLit.value = left->u.integerLit.value << right->u.integerLit.value;
            break;
        case OP_SHR:
            node->u.integerLit.value = left->u.integerLit.value >> right->u.integerLit.value;
            break;
        case OP_ADD:
            node->u.integerLit.value = left->u.integerLit.value + right->u.integerLit.value;
            break;
        case OP_SUB:
            node->u.integerLit.value = left->u.integerLit.value - right->u.integerLit.value;
            break;
        case OP_MUL:
            node->u.integerLit.value = left->u.integerLit.value * right->u.integerLit.value;
            break;
        case OP_DIV:
            if (right->u.integerLit.value == 0)
                ParseError(c, "division by zero in constant expression");
            node->u.integerLit.value = left->u.integerLit.value / right->u.integerLit.value;
            break;
        case OP_REM:
            if (right->u.integerLit.value == 0)
                ParseError(c, "division by zero in constant expression");
            node->u.integerLit.value = left->u.integerLit.value % right->u.integerLit.value;
            break;
        default:
            goto integerOp;
        }
    }
    else if (left->type == TYPE_INTEGER && right->type == TYPE_INTEGER) {
integerOp:
        node = NewParseTreeNode(c, NodeTypeBinaryOp);
        node->type = TYPE_INTEGER;
        node->u.binaryOp.op = op;
        node->u.binaryOp.left = left;
        node->u.binaryOp.right = right;
    }
    else {
        ParseError(c, "Expecting a numeric expression");
        node = NULL; /* not reached */
    }
    return node;
}

/* NewParseTreeNode - allocate a new parse tree node */
static ParseTreeNode *NewParseTreeNode(ParseContext *c, int type)
{
    ParseTreeNode *node = (ParseTreeNode *)LocalAlloc(c, sizeof(ParseTreeNode));
    memset(node, 0, sizeof(ParseTreeNode));
    node->nodeType = type;
    return node;
}

/* AddNodeToList - add a node to a parse tree node list */
void AddNodeToList(ParseContext *c, NodeListEntry ***ppNextEntry, ParseTreeNode *node)
{
    NodeListEntry *entry = (NodeListEntry *)LocalAlloc(c, sizeof(NodeListEntry));
    entry->node = node;
    entry->next = NULL;
    **ppNextEntry = entry;
    *ppNextEntry = &entry->next;
}

/* IsIntegerLit - check to see if a node is an integer literal */
int IsIntegerLit(ParseTreeNode *node)
{
    return node->nodeType == NodeTypeIntegerLit;
}

/* AddString - add a string to the string table */
String *AddString(ParseContext *c, const char *value)
{
    String *str;
    
    /* check to see if the string is already in the table */
    for (str = c->strings; str != NULL; str = str->next)
        if (strcmp(value, str->data) == 0)
            return str;

    /* allocate the string structure */
    str = (String *)GlobalAlloc(c, sizeof(String) + strlen(value));
    memset(str, 0, sizeof(String));
    strcpy((char *)str->data, value);
    str->next = c->strings;
    c->strings = str;

    /* return the string table entry */
    return str;
}

/* GlobalAlloc - allocate memory from the global heap */
void *GlobalAlloc(ParseContext *c, size_t size)
{
    uint8_t *p = c->nextGlobal;
    size = (size + ALIGN_MASK) & ~ALIGN_MASK;
    if (p + size > c->nextLocal)
        ParseError(c, "insufficient memory");
    c->nextGlobal += size;
    if (c->heapSize - (c->nextLocal - c->nextGlobal) > c->maxHeapUsed)
        c->maxHeapUsed = c->heapSize - (c->nextLocal - c->nextGlobal);
    return p;
}

/* LocalAlloc - allocate memory from the local heap */
void *LocalAlloc(ParseContext *c, size_t size)
{
    size = (size + ALIGN_MASK) & ~ALIGN_MASK;
    if (c->nextLocal - size < c->nextGlobal)
        ParseError(c, "insufficient memory");
    c->nextLocal -= size;
    if (c->heapSize - (c->nextLocal - c->nextGlobal) > c->maxHeapUsed)
        c->maxHeapUsed = c->heapSize - (c->nextLocal - c->nextGlobal);
    return c->nextLocal;
}
