/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		 Los Alamos National Laboratory

	@(#)gen_ss_code.c	1.3	1/16/91
	DESCRIPTION: gen_ss_code.c -- routines to generate state set code
	ENVIRONMENT: UNIX
***************************************************************************/
#include	<stdio.h>
#include	"parse.h"
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

gen_ss_code()
{
	extern	LIST ss_list;
	StateSet *ssp;
	State	*sp;

	/* For each state set ... */
	for (ssp = firstSS(&ss_list); ssp != NULL; ssp = nextSS(ssp))
	{
		/* For each state ... */
		for (sp = firstState(&ssp->S_list); sp != NULL; sp = nextState(sp))
		{
			/* Generate delay processing function */
			gen_delay_func(sp, ssp);

			/* Generate event processing function */
			gen_event_func(sp, ssp);

			/* Generate action processing function */
			gen_action_func(sp, ssp);
		}
	}
}


/* Generate action processing functions:
   Each state has one action routine.  It's name is derived from the
   state set name and the state name.
*/
gen_action_func(sp, ssp)
State	*sp;
StateSet *ssp; /* Parent state set */
{
	Trans	*tp;
	Action	*ap;
	int	trans_num;
	extern	char *prog_name;

	printf("\n/* Action function for state \"%s\" in state set \"%s\" */\n",
	 sp->state_name, ssp->ss_name);
	/* action function declaration with ss_ptr as parameter */
	printf("static a_%s_%s(sprog, ss_ptr)\nSPROG *sprog;\nSSCB\t*ss_ptr;\n{\n",
	 ssp->ss_name, sp->state_name);
	/* "switch" stmt based on the transition number */
	printf("\tswitch(ss_ptr->trans_num)\n\t{\n");
	trans_num = 0;
	/* For each transition (when statement) ... */
	for (tp = firstTrans(&sp->T_list); tp != NULL; tp = nextTrans(tp))
	{
		/* "case" for each transition */
		printf("\tcase %d:\n", trans_num);
		/* For each action statement insert action code */
		for (ap = firstAction(&tp->A_list); ap != NULL;
		 ap = nextAction(ap))
		{
			print_line_num(ap->line_num);
			switch (ap->type)
			{
			case A_STMT:
				printf("\t\t");
				/* Evaluate statements */
				eval_expr(ACTION_STMT, ap->stmt.expr, sp);
				printf(";\n");
				break;
			case A_CCODE:
				printf("%s\n", ap->stmt.c_code);
				break;
			}
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
State	*sp;
StateSet *ssp;
{
	Trans	*tp;
	int	index, trans_num;

	printf("\n/* Event function for state \"%s\" in state set \"%s\" */\n",
	 sp->state_name, ssp->ss_name);
	printf("static e_%s_%s(sprog, ss_ptr)\n", ssp->ss_name, sp->state_name);
	printf("SPROG\t*sprog;\n");
	printf("SSCB\t*ss_ptr;\n{\n");
	trans_num = 0;
	sp->ndelay = 0;
	/* For each transition generate an "if" statement ... */
	for (tp = firstTrans(&sp->T_list); tp != NULL; tp = nextTrans(tp))
	{
		print_line_num(tp->line_num);
		printf("\tif (");
		eval_expr(EVENT_STMT, tp->event_expr, sp);
		/* an event triggered, set next state */
		printf(")\n\t{\n");
		/* index is the transition number (0, 1, ...) */
		index = state_block_index_from_name(ssp, tp->new_state_name);
		if (index < 0)
		{
			fprintf(stderr, "Line %d: ", tp->line_num);
			fprintf(stderr, "No state %s in state set %s\n",
			 tp->new_state_name, ssp->ss_name);
			index = 0; /* default to 1-st state */
			printf("\t\t/* state %s does not exist */\n",
			 tp->new_state_name);
		}
		printf("\t\tss_ptr->next_state = ss_ptr->states + %d;\n", index);
		printf("\t\tss_ptr->trans_num = %d;\n", trans_num);
		printf("\t\treturn TRUE;\n\t}\n");
		trans_num++;
	}
	printf("\treturn FALSE;\n");
	printf("}\n");
}

/* Generate delay processing function for a state */
gen_delay_func(sp, ssp)
State	*sp;
StateSet *ssp;
{
	Trans	*tp;

	printf("\n/* Delay function for state \"%s\" in state set \"%s\" */\n",
	 sp->state_name, ssp->ss_name);
	printf("static d_%s_%s(sprog, ss_ptr)\n", ssp->ss_name, sp->state_name);
	printf("SPROG\t*sprog;\n");
	printf("SSCB\t*ss_ptr;\n{\n");
	sp->ndelay = 0;
	/* For each transition ... */
	for (tp = firstTrans(&sp->T_list); tp != NULL; tp = nextTrans(tp))
	{
		print_line_num(tp->line_num);
		eval_delay(tp->event_expr, sp);
	}
	printf("}\n");
	if  (sp->ndelay > ssp->ndelay)
		ssp->ndelay = sp->ndelay; /* max # delay calls in ss */
	return;
}


/* Evaluate an expression.
	Checks for special functions:
		efSet(ef) and efGet(ef) -> set corresponding bit in ef_mask.
		pvPut() & pvGet -> replace variable with ptr to db struct.
*/
eval_expr(stmt_type, ep, sp)
int	stmt_type;	/* EVENT_STMT, ACTION_STMT, or DELAY_STMT */
Expr	*ep;		/* ptr to expression */
State	*sp;		/* ptr to current State struct */
{
	Expr	*epf;
	int	nparams;

	switch(ep->type)
	{
	case E_VAR:
		printf("%s", ep->value.var->name);
		break;
	case E_CONST:
		printf("%s", ep->value.const);
		break;
	case E_STRING:
		printf("\"%s\"", ep->value.const);
		break;
	case E_FUNC:
		if (special_func(stmt_type, ep, sp))
			break;
		printf("%s(", ep->value.const);
		for (epf = ep->right, nparams = 0; epf != 0;
			epf = epf->next, nparams++)
		{
			if (nparams > 0)
				printf(" ,");
			eval_expr(stmt_type, epf, sp);
		}
		printf(") ");
		break;
	case E_BINOP:
		eval_expr(stmt_type, ep->left, sp);
		printf("%s", ep->value.const);
		eval_expr(stmt_type, ep->right, sp);
		break;
	case E_PAREN:
		printf("(");
		eval_expr(stmt_type, ep->right, sp);
		printf(")");
		break;
	case E_UNOP:
		printf("%s", ep->value.const);
		eval_expr(stmt_type, ep->right, sp);
		break;
	case E_SUBSCR:
		eval_expr(stmt_type, ep->left, sp);
		printf("[");
		eval_expr(stmt_type, ep->right, sp);
		printf("]");
		break;
	default:
		if (stmt_type == EVENT_STMT)
			printf("TRUE"); /* empty event statement defaults to TRUE */
		else
			printf(" ");
		break;
	}
}

enum	fcode	{ F_NONE, F_EFSET, F_EFTEST, F_PVGET, F_PVPUT, F_DELAY };

enum fcode func_name_to_code(fname)
char	*fname;
{
	if (strcmp(fname, "efSet") == 0)
		return F_EFSET;
	if (strcmp(fname, "efTest") == 0)
		return F_EFTEST;
	if (strcmp(fname, "pvPut") == 0)
		return F_PVPUT;
	if (strcmp(fname, "pvGet") == 0)
		return F_PVGET;
	if (strcmp(fname, "delay") == 0)
		return F_DELAY;
	return F_NONE;
}

/* Process special function (returns TRUE if this is a special function) */
special_func(stmt_type, ep, sp)
int	stmt_type;	/* ACTION_STMT or EVENT_STMT */
Expr	*ep;		/* ptr to function in the expression */
State	*sp;		/* current State struct */
{
	char	*fname; /* function name */
	Expr	*ep1; /* 1-st parameter */
	Chan	*cp;
	Var	*vp;
	enum	fcode func_code;

	fname = ep->value.const;
	func_code = func_name_to_code(fname);
	if (func_code == F_NONE)
		return FALSE; /* not a special function */

	ep1 = ep->right; /* ptr to 1-st parameters */
	if ( (ep1 != 0) && (ep1->type == E_VAR) )
	{
		vp = ep1->value.var;
		cp = vp->chan;
	}
	else
	{
		vp = 0;
		cp = 0;
	}

	if (func_code == F_EFSET || func_code == F_EFTEST)
	{
		if (vp->type != V_EVFLAG)
		{
			fprintf(stderr, "Line %d: ", ep->line_num);
			fprintf(stderr,
			 "Parameter to efSet() and efTest() must be an event flag\n");
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
			/* OR the ef bit with ef mask in State struct */
			sp->event_flag_mask |= 1 << (vp->ef_num);
		}
		return TRUE;
	}
	else if (func_code == F_PVPUT || func_code == F_PVGET)
	{
		if (cp == 0)
		{
			fprintf(stderr, "Line %d: ", ep->line_num);
			fprintf(stderr,
			 "Parameter to pvPut/pvGet must be DB variable.\n");
		}
		else if (stmt_type == EVENT_STMT)
		{
			fprintf(stderr,
			 "pvPut/pvGet cannot be used as an event.\n");
		}
		else
		{
			printf("%s(%d)", fname, cp->index);
		}
		return TRUE;
	}
	else if (func_code == F_DELAY)
	{	/* Test for delay: "test_delay(delay_id)" */
		printf("test_delay(%d)", sp->ndelay++);
		return TRUE;
	}
	else
		return FALSE; /* not a special function */
}

/* Evaluate delay expression.
	This is similar to eval_expr() except that all output
	is supressed until a delay() function is found.  All
	parameters are then processed with eval_expr().
*/
eval_delay(ep, sp)
Expr	*ep;		/* ptr to expression */
State	*sp;		/* ptr to current State struct */
{
	Expr	*epf;

#ifdef	DEBUG
	fprintf(stderr, "eval_delay: type=%d\n", ep->type);
#endif	DEBUG
	switch(ep->type)
	{
	case E_FUNC:
#ifdef	DEBUG
		fprintf(stderr, "fn=%s\n", ep->value.const);
#endif	DEBUG
		if (strcmp(ep->value.const, "delay") == 0)
		{	/* delay function found:  replace with special
			   function & parameters, then evaluate 1-st
			   parameter only
			*/
			printf("\tstart_delay(%d, ", sp->ndelay++);
			epf = ep->right; /* paramter */
			eval_expr(EVENT_STMT, epf, sp);
			printf(");\n");
		}
		else
		{
			for (epf = ep->right; epf != 0; epf = epf->next)
			{
				eval_delay(epf, sp);
			}
		}
		break;
	case E_BINOP:
		eval_delay(ep->left, sp);
		eval_delay(ep->right, sp);
		break;
	case E_PAREN:
		eval_delay(ep->right, sp);
		break;
	case E_UNOP:
		eval_delay(ep->right, sp);
		break;
	case E_SUBSCR:
		eval_delay(ep->left, sp);
		eval_delay(ep->right, sp);
		break;
	default:
		break;
	}
}

