/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		 Los Alamos National Laboratory
 	gen_ss_code.c,v 1.2 1995/06/27 15:25:43 wright Exp
	DESCRIPTION: gen_ss_code.c -- routines to generate state set code
	ENVIRONMENT: UNIX
	HISTORY:
19nov91,ajk	Changed find_var() to findVar().
28apr92,ajk	Implemented efClear() & efTestAndClear().
01mar94,ajk	Changed table generation to the new structures defined 
		in seqCom.h.
***************************************************************************/
#include	<stdio.h>
#include	"parse.h"
#include	"cadef.h"
#include 	<dbDefs.h>
#include 	<seqCom.h>

/*+************************************************************************
*  NAME: gen_ss_code
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	---------------------------------------------------
*
*  RETURNS:
*
*  FUNCTION: Generate state set C code from tables.
*
*  NOTES: All inputs are external globals.
*-*************************************************************************/
/*#define	DEBUG		1*/

#define	EVENT_STMT	1
#define	ACTION_STMT	2
#define	DELAY_STMT	3
#define	EXIT_STMT	4

gen_ss_code()
{
	extern Expr		*ss_list;
	Expr			*ssp;
	Expr			*sp;

	/* For each state set ... */
	for (ssp = ss_list; ssp != NULL; ssp = ssp->next)
	{
		/* For each state ... */
		for (sp = ssp->left; sp != NULL; sp = sp->next)
		{
			printf("\f/* Code for state \"%s\" in state set \"%s\" */\n",
			 sp->value, ssp->value);

			/* Generate function to set up for delay processing */
			gen_delay_func(sp, ssp);

			/* Generate event processing function */
			gen_event_func(sp, ssp);

			/* Generate action processing function */
			gen_action_func(sp, ssp);
		}
	}

	/* Generate exit handler code */
	gen_exit_handler();
}
/* Generate a function for each state that sets up delay processing:
 * This function gets called prior to the event function to guarantee
 * that the initial delay value specified in delay() calls are used.
 * Each delay() call is assigned a unique id.  The maximum number of
 * delays is recorded in the state set structure.
 */
gen_delay_func(sp, ssp)
Expr		*ssp;
Expr		*sp;
{
	Expr		*tp;
	int		eval_delay();

	printf("\n/* Delay function for state \"%s\" in state set \"%s\" */\n",
	 sp->value, ssp->value);
	printf("static void D_%s_%s(ssId, pVar)\n", ssp->value, sp->value);
	printf("SS_ID\tssId;\n");
	printf("struct UserVar\t*pVar;\n{\n");

	/* For each transition: */
	for (tp = sp->left; tp != NULL; tp = tp->next)
	{
		print_line_num(tp->line_num, tp->src_file);
		traverseExprTree(tp, E_FUNC, "delay", eval_delay, sp);
	}
	printf("}\n");
}

/* Evaluate the expression within a delay() function and generate
 * a call to seq_delayInit().  Adds ssId, delay id parameters and cast to float.
 * Example:  seq_delayInit(ssId, 1, (float)(<some expression>));
 */
eval_delay(ep, sp)
Expr		*ep;
Expr		*sp;
{
	Expr		*epf;
	int		delay_id;
	extern char	*stype[];

#ifdef	DEBUG
	fprintf(stderr, "eval_delay: type=%s\n", stype[ep->type]);
#endif	DEBUG

	/* Generate 1-st part of function w/ 1-st 2 parameters */
	delay_id = (int)ep->right; /* delay id was previously assigned */
	printf("\tseq_delayInit(ssId, %d, (", delay_id);

	/* Evaluate & generate the 3-rd parameter (an expression) */
	eval_expr(EVENT_STMT, ep->left, sp, 0);

	/* Complete the function call */
	printf("));\n");
}



/* Generate action processing functions:
   Each state has one action routine.  It's name is derived from the
   state set name and the state name.
*/
gen_action_func(sp, ssp)
Expr		*sp;
Expr		*ssp; /* Parent state set */
{
	Expr		*tp;
	Expr		*ap;
	int		trans_num;
	extern char	*prog_name;
	extern		line_num;

	/* Action function declaration */
	printf("\n/* Action function for state \"%s\" in state set \"%s\" */\n",
	 sp->value, ssp->value);
	printf("static void A_%s_%s(ssId, pVar, transNum)\n",
	 ssp->value, sp->value);
	printf("SS_ID\tssId;\n");
	printf("struct UserVar\t*pVar;\n");
	printf("short\ttransNum;\n{\n");

	/* "switch" statment based on the transition number */
	printf("\tswitch(transNum)\n\t{\n");
	trans_num = 0;
	line_num = 0;
	/* For each transition ("when" statement) ... */
	for (tp = sp->left; tp != NULL; tp = tp->next)
	{
		/* "case" for each transition */
		printf("\tcase %d:\n", trans_num);
		/* For each action statement insert action code */
		for (ap = tp->right; ap != NULL; ap = ap->next)
		{
			if (line_num != ap->line_num)
			{
				print_line_num(ap->line_num, ap->src_file);
				line_num = ap->line_num;
			}
			/* Evaluate statements */
			eval_expr(ACTION_STMT, ap, sp, 2);
		}
		/* end of case */
		printf("\t\treturn;\n");
		trans_num++;
	}
	/* end of switch stmt */
	printf("\t}\n");
	/* end of function */
	printf("}\n");
	return;
}

/* Generate a C function that checks events for a particular state */
gen_event_func(sp, ssp)
Expr		*sp;
Expr		*ssp;
{
	Expr		*tp;
	int		index, trans_num;

	printf("\n/* Event function for state \"%s\" in state set \"%s\" */\n",
	 sp->value, ssp->value);
	printf("static long E_%s_%s(ssId, pVar, pTransNum, pNextState)\n",
	 ssp->value, sp->value);
	printf("SS_ID\tssId;\n");
	printf("struct UserVar\t*pVar;\n");
	printf("short\t*pTransNum, *pNextState;\n{\n");
	trans_num = 0;
	/* For each transition generate an "if" statement ... */
	for (tp = sp->left; tp != NULL; tp = tp->next)
	{
		print_line_num(tp->line_num, tp->src_file);
		printf("\tif (");
		if (tp->left == 0)
			printf("TRUE");
		else
			eval_expr(EVENT_STMT, tp->left, sp, 0);
		printf(")\n\t{\n");
		/* index is the transition number (0, 1, ...) */
		index = state_block_index_from_name(ssp, tp->value);
		if (index < 0)
		{
			fprintf(stderr, "Line %d: ", tp->line_num);
			fprintf(stderr, "No state %s in state set %s\n",
			 tp->value, ssp->value);
			index = 0; /* default to 1-st state */
			printf("\t\t/* state %s does not exist */\n",
			 tp->value);
		}
		printf("\t\t*pNextState = %d;\n", index);
		printf("\t\t*pTransNum = %d;\n", trans_num);
		printf("\t\treturn TRUE;\n\t}\n");
		trans_num++;
	}
	printf("\treturn FALSE;\n");
	printf("}\n");
}

/* Given a state name and state set struct, find the corresponding
   state struct and return its index (1-st one is 0) */
state_block_index_from_name(ssp, state_name)
Expr		*ssp;
char		*state_name;
{
	Expr		*sp;
	int		index;

	index = 0;
	for (sp = ssp->left; sp != NULL; sp = sp->next)
	{
		if (strcmp(state_name, sp->value) == 0)
			return index;
		index++;
	}
	return -1; /* State name non-existant */
}
/* Evaluate an expression.
*/
eval_expr(stmt_type, ep, sp, level)
int		stmt_type;	/* EVENT_STMT, ACTION_STMT, or DELAY_STMT */
Expr		*ep;		/* ptr to expression */
Expr		*sp;		/* ptr to current State struct */
int		level;		/* indentation level */
{
	Expr		*epf;
	int		nparams;
	extern int	reent_opt;
	extern int	line_num;

	if (ep == 0)
		return;

	switch(ep->type)
	{
	case E_CMPND:
		indent(level);
		printf("{\n");
		line_num += 1;
		for (epf = ep->left; epf != 0;	epf = epf->next)
		{
			eval_expr(stmt_type, epf, sp, level+1);
		}
		indent(level);
		printf("}\n");
		line_num += 1;
		break;
	case E_STMT:
		indent(level);
		eval_expr(stmt_type, ep->left, sp, 0);
		printf(";\n");
		line_num += 1;
		break;
	case E_IF:
	case E_WHILE:
		indent(level);
		if (ep->type == E_IF)
			printf("if (");
		else
			printf("while (");
		eval_expr(stmt_type, ep->left, sp, 0);
		printf(")\n");
		line_num += 1;
		epf = ep->right;
		if (epf->type == E_CMPND)
			eval_expr(stmt_type, ep->right, sp, level);
		else
			eval_expr(stmt_type, ep->right, sp, level+1);
		break;
	case E_FOR:
		indent(level);
		printf("for (");
		eval_expr(stmt_type, ep->left->left, sp, 0);
		printf("; ");
		eval_expr(stmt_type, ep->left->right, sp, 0);
		printf("; ");
		eval_expr(stmt_type, ep->right->left, sp, 0);
		printf(")\n");
		line_num += 1;
		epf = ep->right->right;
		if (epf->type == E_CMPND)
			eval_expr(stmt_type, epf, sp, level);
		else
			eval_expr(stmt_type, epf, sp, level+1);
		break;
	case E_ELSE:
		indent(level);
		printf("else\n");
		line_num += 1;
		epf = ep->left;
		/* Is it "else if" ? */
		if (epf->type == E_IF || epf->type == E_CMPND)
			eval_expr(stmt_type, ep->left, sp, level);
		else
			eval_expr(stmt_type, ep->left, sp, level+1);
		break;
	case E_VAR:
#ifdef	DEBUG
		fprintf(stderr, "E_VAR: %s\n", ep->value);
#endif	DEBUG
		if(reent_opt)
		{	/* Make variables point to allocated structure */
			Var		*vp;
			vp = (Var *)ep->left;
			if (vp->type != V_NONE && vp->type != V_EVFLAG)
				printf("(pVar->%s)", ep->value);
			else
				printf("%s", ep->value);
		}
		else
			printf("%s", ep->value);
		break;
	case E_CONST:
		printf("%s", ep->value);
		break;
	case E_STRING:
		printf("\"%s\"", ep->value);
		break;
	case E_BREAK:
		indent(level);
		printf("break;\n");
		line_num += 1;
		break;
	case E_FUNC:
#ifdef	DEBUG
		fprintf(stderr, "E_FUNC: %s\n", ep->value);
#endif	DEBUG
		if (special_func(stmt_type, ep, sp))
			break;
		printf("%s(", ep->value);
		for (epf = ep->left, nparams = 0; epf != 0; epf = epf->next, nparams++)
		{
			if (nparams > 0)
				printf(" ,");
			eval_expr(stmt_type, epf, sp, 0);
		}
		printf(") ");
		break;
	case E_ASGNOP:
	case E_BINOP:
		eval_expr(stmt_type, ep->left, sp, 0);
		printf(" %s ", ep->value);
		eval_expr(stmt_type, ep->right, sp, 0);
		break;
	case E_PAREN:
		printf("(");
		eval_expr(stmt_type, ep->left, sp, 0);
		printf(")");
		break;
	case E_UNOP:
		printf("%s", ep->value);
		eval_expr(stmt_type, ep->left, sp, 0);
		break;
	case E_PRE:
		printf("%s", ep->value);
		eval_expr(stmt_type, ep->left, sp, 0);
		break;
	case E_POST:
		eval_expr(stmt_type, ep->left, sp, 0);
		printf("%s", ep->value);
		break;
	case E_SUBSCR:
		eval_expr(stmt_type, ep->left, sp, 0);
		printf("[");
		eval_expr(stmt_type, ep->right, sp, 0);
		printf("]");
		break;
	case E_TEXT:
		printf("%s\n", ep->left);
		line_num += 1;
		break;
	default:
		if (stmt_type == EVENT_STMT)
			printf("TRUE"); /* empty event statement defaults to TRUE */
		else
			printf(" ");
		break;
	}
}

indent(level)
int		level;
{
	while (level-- > 0)
		printf("\t");
}
/* func_name_to_code - convert function name to a code */
enum	fcode { F_DELAY, F_EFSET, F_EFTEST, F_EFCLEAR, F_EFTESTANDCLEAR,
		F_PVGET, F_PVPUT, F_PVTIMESTAMP, F_PVASSIGN,
		F_PVMONITOR, F_PVSTOPMONITOR, F_PVCOUNT, F_PVINDEX,
		F_PVSTATUS, F_PVSEVERITY, F_PVFLUSH, F_PVERROR, F_PVGETCOMPLETE,
		F_PVASSIGNED, F_PVCONNECTED,
		F_PVCHANNELCOUNT, F_PVCONNECTCOUNT, F_PVASSIGNCOUNT,
		F_PVDISCONNECT, F_SEQLOG, F_MACVALUEGET, F_OPTGET,
		F_NONE };

char	*fcode_str[] = { "delay", "efSet", "efTest", "efClear", "efTestAndClear",
		"pvGet", "pvPut", "pvTimeStamp", "pvAssign",
		"pvMonitor", "pvStopMonitor", "pvCount", "pvIndex",
		"pvStatus", "pvSeverity", "pvFlush", "pvError", "pvGetComplete",
		"pvAssigned", "pvConnected",
		"pvChannelCount", "pvConnectCount", "pvAssignCount",
		"pvDisconnect", "seqLog", "macValueGet", "optGet",
		NULL };

enum fcode func_name_to_code(fname)
char	*fname;
{
	int		i;

	for (i = 0; fcode_str[i] != NULL; i++)
	{
		if (strcmp(fname, fcode_str[i]) == 0)
			return (enum fcode)i;
	}
			
	return F_NONE;
}

/* Process special function (returns TRUE if this is a special function)
	Checks for one of the following special functions:
	 - event flag functions, e.g. pvSet()
	 - process variable functions, e.g. pvPut()
	 - delay()
	 - macVauleget()
	 - seqLog()
*/
special_func(stmt_type, ep, sp)
int		stmt_type;	/* ACTION_STMT or EVENT_STMT */
Expr		*ep;		/* ptr to function in the expression */
Expr		*sp;		/* current State struct */
{
	char		*fname; /* function name */
	Expr		*ep1, *ep2, *ep3; /* parameters */
	Chan		*cp;
	Var		*vp;
	enum		fcode func_code;
	int		delay_id;

	fname = ep->value;
	func_code = func_name_to_code(fname);
	if (func_code == F_NONE)
		return FALSE; /* not a special function */

#ifdef	DEBUG
	fprintf(stderr, "special_func: func_code=%d\n", func_code);
#endif	DEBUG
	switch (func_code)
	{
	    case F_DELAY:
		delay_id = (int)ep->right;
		printf("seq_delay(ssId, %d)", delay_id);
		return TRUE;

	    case F_EFSET:
	    case F_EFTEST:
	    case F_EFCLEAR:
	    case F_EFTESTANDCLEAR:
		/* Event flag funtions */
		gen_ef_func(stmt_type, ep, sp, fname);
		return TRUE;

	    case F_PVPUT:
	    case F_PVGET:
	    case F_PVTIMESTAMP:
	    case F_PVGETCOMPLETE:
	    case F_PVSTATUS:
	    case F_PVSEVERITY:
	    case F_PVCONNECTED:
	    case F_PVASSIGNED:
	    case F_PVMONITOR:
	    case F_PVSTOPMONITOR:
	    case F_PVCOUNT:
	    case F_PVINDEX:
	    case F_PVDISCONNECT:
	    case F_PVASSIGN:
		/* DB functions requiring a channel id */
		gen_pv_func(stmt_type, ep, sp, fname, func_code);
		return TRUE;

	    case F_PVFLUSH:
	    case F_PVERROR:
	    case F_PVCHANNELCOUNT:
	    case F_PVCONNECTCOUNT:
	    case F_PVASSIGNCOUNT:
		/* DB functions NOT requiring a channel structure */
		printf("seq_%s(ssId)", fname);
		return TRUE;

	    case F_SEQLOG:
	    case F_MACVALUEGET:
	    case F_OPTGET:
		/* Any funtion that requires adding ssID as 1st parameter.
		 * Note:  name is changed by prepending "seq_". */
		printf("seq_%s(ssId", fname);
		/* now fill in user-supplied paramters */
		for (ep1 = ep->left; ep1 != 0; ep1 = ep1->next)
		{
			printf(", ");
			eval_expr(stmt_type, ep1, sp, 0);
		}
		printf(") ");
		return TRUE;

	    default:
		/* Not a special function */
		return FALSE;
	}
}

/* Generate code for all event flag functions */
gen_ef_func(stmt_type, ep, sp, fname, func_code)
int		stmt_type;	/* ACTION_STMT or EVENT_STMT */
Expr		*ep;		/* ptr to function in the expression */
Expr		*sp;		/* current State struct */
char		*fname;		/* function name */
enum		fcode func_code;
{
	Expr		*ep1, *ep2, *ep3;
	Var		*vp;
	Chan		*cp;

	ep1 = ep->left; /* ptr to 1-st parameters */
	if ( (ep1 != 0) && (ep1->type == E_VAR) )
		vp = (Var *)findVar(ep1->value);
	else
		vp = 0;
		if (vp == 0 || vp->type != V_EVFLAG)
	{
		fprintf(stderr, "Line %d: ", ep->line_num);
		fprintf(stderr,
		 "Parameter to \"%s\" must be an event flag\n", fname);
	}
	else if (func_code == F_EFSET && stmt_type == EVENT_STMT)
	{
		fprintf(stderr, "Line %d: ", ep->line_num);
		fprintf(stderr,
		 "efSet() cannot be used as an event.\n");
	}
	else
	{
		printf("seq_%s(ssId, %s)", fname, vp->name);
	}

	return;
}

/* Generate code for pv functions requiring a database variable.
 * The channel id (index into channel array) is substituted for the variable
 */
gen_pv_func(stmt_type, ep, sp, fname)
int		stmt_type;	/* ACTION_STMT or EVENT_STMT */
Expr		*ep;		/* ptr to function in the expression */
Expr		*sp;		/* current State struct */
char		*fname;		/* function name */
{
	Expr		*ep1, *ep2, *ep3;
	Var		*vp;
	Chan		*cp;
	int		index;

	ep1 = ep->left; /* ptr to 1-st parameter in the function */
	if (ep1 == 0)
	{
		fprintf(stderr, "Line %d: ", ep->line_num);
		fprintf(stderr,
		 "Function \"%s\" requires a parameter.\n", fname);
		return;
	}

	vp = 0;
	if (ep1->type == E_VAR)
	{
		vp = (Var *)findVar(ep1->value);
	}
	else if (ep1->type == E_SUBSCR)
	{	/* Form should be: <db variable>[<expression>] */
		ep2 = ep1->left;	/* variable */
		ep3 = ep1->right;	/* subscript */
		if ( ep2->type == E_VAR )
		{
			vp = (Var *)findVar(ep2->value);
		}
	}

	if (vp == 0)
	{
		fprintf(stderr, "Line %d: ", ep->line_num);
		fprintf(stderr,
		 "Parameter to \"%s\" is not a defined variable.\n", fname);
		cp=0;
	}
		
	else
	{
#ifdef	DEBUG
	fprintf(stderr, "gen_pv_func: var=%s\n", ep1->value);
#endif	DEBUG
		cp = vp->chan;
		index = cp->index;
	}

	if ( (vp != 0) && (cp == 0) )
	{
		fprintf(stderr, "Line %d: ", ep->line_num);
		fprintf(stderr,
		 "Parameter to \"%s\" must be DB variable.\n", fname);
		index=-1;
	}

	printf("seq_%s(ssId, %d", fname, index);

	if (ep1->type == E_SUBSCR) /* subscripted variable? */
	{	/* e.g. pvPut(xyz[i+2]); => seq_pvPut(ssId, 3 + (i+2)); */
		printf(" + (");
		/* evalute the subscript expression */
		eval_expr(stmt_type, ep3, sp, 0);
		printf(")");
	}

	/* Add any additional parameter(s) */
	ep1 = ep1->next;
	while (ep1 != 0)
	{
		printf(", ");
		eval_expr(stmt_type, ep1, sp, 0);
		ep1 = ep1->next;
	}

	/* Close the parameter list */
	printf(") \n");

	return;
}
/* Generate exit handler code */
gen_exit_handler()
{
	extern Expr	*exit_code_list;
	Expr		*ep;

	printf("/* Exit handler */\n");
	printf("static void exit_handler(ssId, pVar)\n");
	printf("int\tssId;\n");
	printf("struct UserVar\t*pVar;\n{\n");
	for (ep = exit_code_list; ep != 0; ep = ep->next)
	{
		eval_expr(EXIT_STMT, ep, 0, 1);
	}
	printf("}\n\n");
}
