/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		 Los Alamos National Laboratory

	@(#)gen_ss_code.c	1.4	4/17/91
	DESCRIPTION: gen_ss_code.c -- routines to generate state set code
	ENVIRONMENT: UNIX
***************************************************************************/
#include	<stdio.h>
#include	"parse.h"
#include	"cadef.h"
#include	"seq.h"

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

			/* Generate delay processing function */
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

	printf("\n/* Action function for state \"%s\" in state set \"%s\" */\n",
	 sp->value, ssp->value);
	/* action function declaration with ss_ptr as parameter */
	printf("static A_%s_%s(sprog, ss_ptr, var_ptr)\n", ssp->value, sp->value);
	printf("SPROG\t*sprog;\n");
	printf("SSCB\t*ss_ptr;\n");
	printf("struct UserVar\t*var_ptr;\n{\n");
	/* "switch" statment based on the transition number */
	printf("\tswitch(ss_ptr->trans_num)\n\t{\n");
	trans_num = 0;
	/* For each transition ("when" statement) ... */
	for (tp = sp->left; tp != NULL; tp = tp->next)
	{
		/* "case" for each transition */
		printf("\tcase %d:\n", trans_num);
		/* For each action statement insert action code */
		for (ap = tp->right; ap != NULL; ap = ap->next)
		{
			print_line_num(ap->line_num, ap->src_file);
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
	printf("static E_%s_%s(sprog, ss_ptr, var_ptr)\n", ssp->value, sp->value);
	printf("SPROG\t*sprog;\n");
	printf("SSCB\t*ss_ptr;\n");
	printf("struct UserVar\t*var_ptr;\n{\n");
	trans_num = 0;
	/* For each transition generate an "if" statement ... */
	for (tp = sp->left; tp != NULL; tp = tp->next)
	{
		print_line_num(tp->line_num, tp->src_file);
		printf("\tif (");
		eval_expr(EVENT_STMT, tp->left, sp, 0);
		/* an event triggered, set next state */
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
		printf("\t\tss_ptr->next_state = %d;\n", index);
		printf("\t\tss_ptr->trans_num = %d;\n", trans_num);
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

/* Generate delay processing function for a state */
gen_delay_func(sp, ssp)
Expr		*sp;
Expr		*ssp;
{
	Expr		*tp;
	int		eval_delay();

	printf("\n/* Delay function for state \"%s\" in state set \"%s\" */\n",
	 sp->value, ssp->value);
	printf("static D_%s_%s(sprog, ss_ptr, var_ptr)\n", ssp->value, sp->value);
	printf("SPROG\t*sprog;\n");
	printf("SSCB\t*ss_ptr;\n");
	printf("struct UserVar\t*var_ptr;\n{\n");

	/* For each transition ... */
	for (tp = sp->left; tp != NULL; tp = tp->next)
	{
		print_line_num(tp->line_num, tp->src_file);
		traverseExprTree(tp, E_FUNC, "delay", eval_delay, sp);
	}
	printf("}\n");

	return;
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
	extern int	reent_flag;
	if (ep == 0)
		return;

	switch(ep->type)
	{
	case E_CMPND:
		indent(level);
		printf("{\n");
		for (epf = ep->left; epf != 0;	epf = epf->next)
		{
			eval_expr(stmt_type, epf, sp, level+1);
		}
		indent(level);
		printf("}\n");
		break;
	case E_STMT:
		indent(level);
		eval_expr(stmt_type, ep->left, sp, 0);
		printf(";\n");
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
		epf = ep->right;
		if (epf->type == E_CMPND)
			eval_expr(stmt_type, ep->right, sp, level);
		else
			eval_expr(stmt_type, ep->right, sp, level+1);
		break;
	case E_ELSE:
		indent(level);
		printf("else\n");
		epf = ep->left;
		/* Is it "else if" ? */
		if (epf->type == E_IF || epf->type == E_CMPND)
			eval_expr(stmt_type, ep->left, sp, level);
		else
			eval_expr(stmt_type, ep->left, sp, level+1);
		break;
	case E_VAR:
		if(reent_flag)
		{	/* Make variables point to allocated structure */
			Var		*vp;
			vp = (Var *)ep->left;
			if (vp->type != V_NONE && vp->type != V_EVFLAG)
				printf("(var_ptr->%s)", ep->value);
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
	case E_FUNC:
		if (special_func(stmt_type, ep, sp))
			break;
		printf("%s(", ep->value);
		for (epf = ep->left, nparams = 0; epf != 0;
			epf = epf->next, nparams++)
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
	case E_SUBSCR:
		eval_expr(stmt_type, ep->left, sp, 0);
		printf("[");
		eval_expr(stmt_type, ep->right, sp, 0);
		printf("]");
		break;
	case E_TEXT:
		printf("%s\n", ep->left);
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
enum	fcode { F_DELAY, F_EFSET, F_EFTEST, F_PVGET, F_PVPUT,
		F_PVMONITOR, F_PVSTOPMONITOR, F_PVCOUNT, F_PVINDEX,
		F_PVSTATUS, F_PVSEVERITY, F_PVFLUSH, F_PVERROR, F_PVGETCOMPLETE,
		F_PVCONNECTED, F_PVCHANNELCOUNT, F_PVCONNECTCOUNT, F_NONE };

char	*fcode_str[] = { "delay", "efSet", "efTest", "pvGet", "pvPut",
		"pvMonitor", "pvStopMonitor", "pvCount", "pvIndex",
		"pvStatus", "pvSeverity", "pvFlush", "pvError", "pvGetComplete",
		"pvConnected", "pvChannelCount", "pvConnectCount", NULL };

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
	Checks for special functions:
		efSet(ef) and efGet(ef) -> set corresponding bit in ef_mask.
		pvPut() & pvGet -> replace variable with ptr to db struct.
		delay() - replaces delay time with delay index.
*/
special_func(stmt_type, ep, sp)
int		stmt_type;	/* ACTION_STMT or EVENT_STMT */
Expr		*ep;		/* ptr to function in the expression */
Expr		*sp;		/* current State struct */
{
	char		*fname; /* function name */
	Expr		*ep1; /* 1-st parameter */
	Chan		*cp;
	Var		*vp;
	enum		fcode func_code;
	int		ndelay;

	fname = ep->value;
	func_code = func_name_to_code(fname);
	if (func_code == F_NONE)
		return FALSE; /* not a special function */

	ep1 = ep->left; /* ptr to 1-st parameters */
	if ( (ep1 != 0) && (ep1->type == E_VAR) )
	{
		vp = (Var *)find_var(ep1->value);
		cp = vp->chan;
	}
	else
	{
		vp = 0;
		cp = 0;
	}

	switch (func_code)
	{
	    case F_EFSET:
	    case F_EFTEST:
		if (vp->type != V_EVFLAG)
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
			printf("%s(%s)", fname, vp->name);
		}
		return TRUE;

	    case F_PVPUT:
	    case F_PVGET:
	    case F_PVGETCOMPLETE:
	    case F_PVSTATUS:
	    case F_PVSEVERITY:
	    case F_PVCONNECTED:
	    case F_PVMONITOR:
	    case F_PVSTOPMONITOR:
	    case F_PVCOUNT:
	    case F_PVINDEX:
		/* DB functions requiring a channel structure */
		if (cp == 0)
		{
			fprintf(stderr, "Line %d: ", ep->line_num);
			fprintf(stderr,
			 "Parameter to \"%s\" must be DB variable.\n", fname);
		}
		else
		{
			printf("%s(%d)", fname, cp->index);
		}
		return TRUE;

	    case F_PVFLUSH:
	    case F_PVERROR:
	    case F_PVCHANNELCOUNT:
	    case F_PVCONNECTCOUNT:
		/* DB functions not requiring a channel structure */
		printf("%s()", fname);
		return TRUE;

	    case F_DELAY:
		/* Test for delay: "test_delay(delay_id)" */
		printf("test_delay(%d)", (int)ep->right);
		return TRUE;

	    default:
		return FALSE; /* not a special function */
	}
}

/* Evaluate delay expression. */
eval_delay(ep, sp)
Expr		*ep;		/* ptr to expression */
Expr		*sp;		/* ptr to current State struct */
{
	Expr		*epf;
	int		delay_id;
	extern char	*stype[];
#ifdef	DEBUG
	fprintf(stderr, "eval_delay: type=%s\n", stype[ep->type]);
#endif

	delay_id = (int)ep->right;
	printf("\tstart_delay(%d, ", delay_id);

	/* Output each parameter */
	eval_expr(EVENT_STMT, ep->left, sp, 0);

	printf(");\n");
}
/* Generate exit handler code */
gen_exit_handler()
{
	extern Expr	*exit_code_list;
	Expr		*ep;

	printf("/* Exit handler */\n");
	printf("static exit_handler(sprog, var_ptr)\n");
	printf("SPROG\t*sprog;\n");
	printf("struct UserVar\t*var_ptr;\n{\n");
	for (ep = exit_code_list; ep != 0; ep = ep->next)
	{
		eval_expr(EXIT_STMT, ep, 0, 1);
	}
	printf("}\n\n");
}
