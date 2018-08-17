/* adv2vmdebug.c - debug routines
 *
 * Copyright (c) 2018 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "adv2compiler.h"
#include "adv2debug.h"

static void PrintNodeList(ParseContext *c, NodeListEntry *entry, int indent);
static void PrintPrintOpList(ParseContext *c, PrintOp *op, int indent);

void PrintNode(ParseContext *c, ParseTreeNode *node, int indent)
{
	printf("%*s", indent, "");
    switch (node->nodeType) {
    case NodeTypeFunctionDef:
        printf("FunctionDef\n");
        PrintLocalSymbols(&node->u.functionDef.arguments, "arguments", indent + 2);
        PrintLocalSymbols(&node->u.functionDef.locals, "locals", indent + 2);
        PrintNode(c, node->u.functionDef.body, indent + 2);
        break;
    case NodeTypeIf:
        printf("If\n");
        printf("%*stest\n", indent + 2, "");
        PrintNode(c, node->u.ifStatement.test, indent + 4);
        printf("%*sthen\n", indent + 2, "");
        PrintNode(c, node->u.ifStatement.thenStatement, indent + 4);
        if (node->u.ifStatement.elseStatement) {
            printf("%*selse\n", indent + 2, "");
            PrintNode(c, node->u.ifStatement.elseStatement, indent + 4);
        }
        break;
    case NodeTypeWhile:
        printf("While\n");
        printf("%*stest\n", indent + 2, "");
        PrintNode(c, node->u.whileStatement.test, indent + 4);
        PrintNode(c, node->u.whileStatement.body, indent + 2);
        break;
    case NodeTypeDoWhile:
        printf("DoWhile\n");
        printf("%*stest\n", indent + 2, "");
        PrintNode(c, node->u.doWhileStatement.body, indent + 2);
        PrintNode(c, node->u.doWhileStatement.test, indent + 4);
        break;
    case NodeTypeFor:
        printf("For\n");
        printf("%*sinit\n", indent + 2, "");
        PrintNode(c, node->u.forStatement.init, indent + 4);
        printf("%*stest\n", indent + 2, "");
        PrintNode(c, node->u.forStatement.test, indent + 4);
        printf("%*sincr\n", indent + 2, "");
        PrintNode(c, node->u.forStatement.incr, indent + 4);
        PrintNode(c, node->u.forStatement.body, indent + 2);
        break;
    case NodeTypeReturn:
        printf("Return\n");
        if (node->u.returnStatement.value) {
            printf("%*sexpr\n", indent + 2, "");
            PrintNode(c, node->u.returnStatement.value, indent + 4);
        }
        break;
    case NodeTypeBreak:
        printf("Break\n");
        break;
    case NodeTypeContinue:
        printf("Continue\n");
        break;
    case NodeTypeBlock:
        printf("Block\n");
        PrintNodeList(c, node->u.blockStatement.statements, indent + 2);
        break;
    case NodeTypeTry:
        printf("Try\n");
        printf("%*stry\n", indent + 2, "");
        PrintNode(c, node->u.tryStatement.statement, indent + 4);
        if (node->u.tryStatement.catchStatement) {
            printf("%*scatch\n", indent + 2, "");
            PrintNode(c, node->u.tryStatement.catchStatement, indent + 4);
        }
        if (node->u.tryStatement.finallyStatement) {
            printf("%*sthrow\n", indent + 2, "");
            PrintNode(c, node->u.tryStatement.finallyStatement, indent + 4);
        }
        break;
    case NodeTypeThrow:
        printf("Throw\n");
        PrintNode(c, node->u.returnStatement.value, indent + 2);
        break;
    case NodeTypeExpr:
        printf("Expr\n");
        PrintNode(c, node->u.exprStatement.expr, indent + 4);
        break;
    case NodeTypeEmpty:
        printf("Empty\n");
        break;
    case NodeTypePrint:
        printf("Print\n");
        PrintPrintOpList(c, node->u.printStatement.ops, indent + 2);
        break;
    case NodeTypeGlobalSymbolRef:
        printf("GlobalSymbolRef: %s\n", node->u.symbolRef.symbol->name);
        break;
    case NodeTypeLocalSymbolRef:
        printf("LocalSymbolRef: %s\n", node->u.localSymbolRef.symbol->name);
        break;
    case NodeTypeArgumentRef:
        printf("NodeTypeArgumentRef: %s\n", node->u.localSymbolRef.symbol->name);
        break;
    case NodeTypeStringLit:
		printf("StringLit: '%s'\n", c->stringBuf + node->u.stringLit.string->offset);
        break;
    case NodeTypeIntegerLit:
		printf("IntegerLit: %d\n",node->u.integerLit.value);
        break;
    case NodeTypeUnaryOp:
        printf("UnaryOp: %d\n", node->u.unaryOp.op);
        printf("%*sexpr\n", indent + 2, "");
        PrintNode(c, node->u.unaryOp.expr, indent + 4);
        break;
    case NodeTypePreincrementOp:
        printf("PreincrementOp: %d\n", node->u.incrementOp.increment);
        printf("%*sexpr\n", indent + 2, "");
        PrintNode(c, node->u.incrementOp.expr, indent + 4);
        break;
    case NodeTypePostincrementOp:
        printf("PostincrementOp: %d\n", node->u.incrementOp.increment);
        printf("%*sexpr\n", indent + 2, "");
        PrintNode(c, node->u.incrementOp.expr, indent + 4);
        break;
    case NodeTypeBinaryOp:
        printf("BinaryOp: %d\n", node->u.binaryOp.op);
        printf("%*sleft\n", indent + 2, "");
        PrintNode(c, node->u.binaryOp.left, indent + 4);
        printf("%*sright\n", indent + 2, "");
        PrintNode(c, node->u.binaryOp.right, indent + 4);
        break;
    case NodeTypeAssignmentOp:
        printf("AssignmentOp: %d\n", node->u.binaryOp.op);
        printf("%*sleft\n", indent + 2, "");
        PrintNode(c, node->u.binaryOp.left, indent + 4);
        printf("%*sright\n", indent + 2, "");
        PrintNode(c, node->u.binaryOp.right, indent + 4);
        break;
    case NodeTypeArrayRef:
        printf("ArrayRef\n");
        printf("%*sarray\n", indent + 2, "");
        PrintNode(c, node->u.arrayRef.array, indent + 4);
        printf("%*sindex\n", indent + 2, "");
        PrintNode(c, node->u.arrayRef.index, indent + 4);
        break;
    case NodeTypeFunctionCall:
        printf("FunctionCall: %d\n", node->u.functionCall.argc);
        printf("%*sfcn\n", indent + 2, "");
        PrintNode(c, node->u.functionCall.fcn, indent + 4);
        PrintNodeList(c, node->u.functionCall.args, indent + 2);
        break;
    case NodeTypeSend:
        printf("Send\n");
        printf("%*sobject\n", indent + 2, "");
        if (node->u.send.object)
            PrintNode(c, node->u.send.object, indent + 4);
        else
            printf("%*ssuper\n", indent + 4, "");
        printf("%*sselector\n", indent + 2, "");
        PrintNode(c, node->u.send.selector, indent + 4);
        PrintNodeList(c, node->u.send.args, indent + 2);
        break;
    case NodeTypePropertyRef:
        printf("PropertyRef\n");
        printf("%*sobject\n", indent + 2, "");
        PrintNode(c, node->u.propertyRef.object, indent + 4);
        printf("%*sproperty\n", indent + 2, "");
        PrintNode(c, node->u.propertyRef.property, indent + 4);
        break;
    case NodeTypeDisjunction:
        printf("Disjunction\n");
        PrintNodeList(c, node->u.exprList.exprs, indent + 2);
        break;
    case NodeTypeConjunction:
        printf("Conjunction\n");
        PrintNodeList(c, node->u.exprList.exprs, indent + 2);
        break;
    default:
        printf("<unknown node type: %d>\n", node->nodeType);
        break;
    }
}

static void PrintNodeList(ParseContext *c, NodeListEntry *entry, int indent)
{
    while (entry != NULL) {
        PrintNode(c, entry->node, indent);
        entry = entry->next;
    }
}

static void PrintPrintOpList(ParseContext *c, PrintOp *op, int indent)
{
    while (op != NULL) {
        switch (op->trap) {
        case TRAP_PrintStr:
	        printf("%*sPrintStr\n", indent, "");
            PrintNode(c, op->expr, indent + 4);
	        break;
        case TRAP_PrintInt:
	        printf("%*sPrintInt\n", indent, "");
            PrintNode(c, op->expr, indent + 4);
	        break;
        case TRAP_PrintTab:
	        printf("%*sPrintTab\n", indent, "");
	        break;
        case TRAP_PrintNL:
	        printf("%*sPrintNL\n", indent, "");
	        break;
        case TRAP_PrintFlush:
	        printf("%*sPrintFlush\n", indent, "");
	        break;
        }
        op = op->next;
    }
}
