/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		 Los Alamos National Laboratory

	$Id$
	DESCRIPTION: Generate tables for run-time sequencer.
	See also:  phase2.c & gen_ss_code.c
	ENVIRONMENT: UNIX
***************************************************************************/

#include	<stdio.h>
#include	"parse.h"
#include	"seq.h"

/*+************************************************************************
*  NAME: gen_tables
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	---------------------------------------------------
*
*  RETURNS: n/a
*
*  FUNCTION: Generate C code from tables.
*
*  NOTES: All inputs are external globals.
*-*************************************************************************/

int		nstates;	/* total # states in all state sets */

gen_tables()
{
	extern	LIST	var_list;	/* variables (from parse) */
	extern	Expr	*ss_list;	/* state sets (from parse) */
	extern	char	*global_c_code;	/* global C code */

	printf("\f/************************ Tables ***********************/\n");

	/* Generate DB blocks */
	gen_db_blocks();

	/* Generate State Blocks */
	gen_state_blocks();

	/* Generate State Set control blocks as an array */
	gen_sscb_array();

	/* Generate state program table */
	gen_state_prog_table();

	return;
}
/* Generate database blocks with structure and data for each defined channel */
gen_db_blocks()
{
	extern	LIST	chan_list;
	Chan	*cp;
	int	nchan;

	printf("\n/* Database Blocks */\n");
	printf("static CHAN db_channels[NUM_CHANNELS] = {\n");
	nchan = 0;
	for (cp = firstChan(&chan_list); cp != NULL; cp = nextChan(cp))
	{
		/* Only process db variables */
		if (cp->db_name != NULL)
		{
			if (nchan > 0)
				printf(",\n");
			fill_db_block(cp, nchan);
			nchan++;
		}
	}
	printf("\n};\n");
	return;
}

/* Fill in a db block with data (all elements for "CHAN" struct) */
fill_db_block(cp, index)
Chan		*cp;
int		index;
{
	Var		*vp;
	char		*get_type_string, *put_type_string, *postfix;
	extern char	*prog_name;
	extern	int	reent_flag;
	int		size, ev_flag, count;

	vp = cp->var;
	/* Convert variable type to a DB request type */
	switch (vp->type)
	{
	case V_SHORT:
		get_type_string = "DBR_STS_INT";
		put_type_string = "DBR_INT";
		size = sizeof(short);
		break;
	case V_INT:
	case V_LONG:
		/* Assume "long" & "int" are same size */
		get_type_string = "DBR_STS_LONG";
		put_type_string = "DBR_LONG";
		size = sizeof(int);
		break;
	case V_CHAR:
		get_type_string = "DBR_STS_CHAR";
		put_type_string = "DBR_CHAR";
		size = sizeof(char);
		break;
	case V_FLOAT:
		get_type_string = "DBR_STS_FLOAT";
		put_type_string = "DBR_FLOAT";
		size = sizeof(float);
		break;
	case V_DOUBLE:
		get_type_string = "DBR_STS_DOUBLE";
		put_type_string = "DBR_DOUBLE";
		size = sizeof(double);
		break;
	case V_STRING:
		get_type_string = "DBR_STS_STRING";
		put_type_string = "DBR_STRING";
		size = sizeof(char);
		break;
	}

	/* fill in the CHAN structure */
	printf("\t%d, ", index);	/* index for this channel */

	printf("\"%s\", ", cp->db_name);/* unexpanded db channel name */

	printf("(char *)0, ");		/* ch'l name after macro expansion */

	/* Ptr or offset to user variable */
	printf("(char *)");
	if (vp->type == V_STRING || cp->count > 0)
		postfix = "[0]";
	else
		postfix = "";
	if (reent_flag)
		printf("OFFSET(struct UserVar, %s%s), ", vp->name, postfix);
	else
		printf("&%s%s, ", vp->name, postfix);	/* variable ptr */

	printf("(chid)0, ");		/* reserve place for chid */

	printf("0, 0, 0, 0, ");		/* connected, get_complete, status, severity */

	printf("%d,\n", size);		/* element size (bytes) */

	printf("\t%s, ", get_type_string);/* DB request conversion type (get/mon) */

	printf("%s, ", put_type_string);/* DB request conversion type (put) */

	count = cp->count;
	if (count == 0)
		count = 1;
	printf("%d, ", count);		/* count for db requests */

	printf("%d, ", cp->mon_flag);	/* monitor flag */

	printf("0, ");			/* monitored */

	printf("%g, ", cp->delta);	/* monitor delta */

	printf("%g, ", cp->timeout);	/* monitor timeout */

	printf("0, ");			/* event id supplied by CA */

	printf("&%s", prog_name);	/* ptr to state program structure */

	return;
}

/* Generate structure and data for state blocks (STATE) */
gen_state_blocks()
{
	extern Expr		*ss_list;
	Expr			*ssp;
	Expr			*sp;
	int			ns;
	extern int		nstates;

	printf("\n/* State Blocks:\n");
	printf(" action_func, event_func, delay_func, event_flag_mask");
	printf(", *delay, *name */\n");

	nstates = 0;
	for (ssp = ss_list; ssp != NULL; ssp = ssp->next)
	{
		printf("\nstatic STATE state_%s[] = {\n", ssp->value);
		ns = 0;
		for (sp = ssp->left; sp != NULL; sp = sp->next)
		{
			if (ns > 0)
				printf(",\n\n");
			ns++;
			nstates++;
			fill_state_block(sp, ssp->value);
		}
		printf("\n};\n");
	}
	return;
}

/* Fill in data for a the state block */
fill_state_block(sp, ss_name)
Expr		*sp;
char		*ss_name;
{
	bitMask		events[NWRDS];
	int		n;

	printf("\t/* State \"%s\"*/\n", sp->value);

	printf("\tA_%s_%s,\t/* action_function */\n", ss_name, sp->value);

	printf("\tE_%s_%s,\t/* event_function */\n", ss_name, sp->value);

	printf("\tD_%s_%s,\t/* delay_function */\n", ss_name, sp->value);

	eval_state_event_mask(sp, events);
	printf("\t/* Event mask for this state: */\n");
	for (n = 0; n < NWRDS; n++)
		printf("\t0x%08x,\n", events[n]);

	printf("\t\"%s\"\t/* *name */", sp->value);

	return;
}

/* Generate the structure with data for a state program table (SPROG) */
gen_state_prog_table()
{
	extern char		*prog_name, *prog_param;
	extern int		async_flag, debug_flag, reent_flag, conn_flag;
	extern int		nstates;
	extern Expr		exit_code_list;

	printf("\n/* Program parameter list */\n");

	printf("static char prog_param[] = \"%s\";\n", prog_param);

	printf("\n/* State Program table (global) */\n");

	printf("SPROG %s = {\n", prog_name);

	printf("\t%d,\t/* magic number */\n", MAGIC);

	printf("\t0,\t/* task id */\n");

	printf("\t0,\t/* dyn_ptr */\n");

	printf("\t0,\t/* task_is_deleted */\n");

	printf("\t1,\t/* relative task priority */\n");

	printf("\t0,\t/* sem_id */\n");

	printf("\tdb_channels,\t/* *channels */\n");

	printf("\tNUM_CHANNELS,\t/* nchan */\n");

	printf("\t0,\t/* conn_count */\n");

	printf("\tsscb,\t/* *sscb */\n");

	printf("\tNUM_SS,\t/* nss */\n");

	printf("\tNULL,\t/* ptr to states */\n");

	printf("\t%d,\t/* number of states */\n", nstates);

	printf("\tNULL,\t/* ptr to user area (not used) */\n");

	if (reent_flag)
		printf("\tsizeof(struct UserVar),\t/* user area size */\n");
	else
		printf("\t0,\t/* user area size (not used) */\n");

	printf("\tNULL,\t/* mac_ptr */\n");

	printf("\tNULL,\t/* scr_ptr */\n");

	printf("\t0,\t/* scr_nleft */\n");

	printf("\t\"%s\",\t/* *name */\n", prog_name);

	printf("\tprog_param,\t/* *params */\n");

	printf("\t%d, %d, %d, %d,\t/* async, debug, & reent flags */\n",
	 async_flag, debug_flag, reent_flag, conn_flag);

	printf("\texit_handler,\t/* exit handler */\n");

	printf("\t0, 0\t/* logSemId & logFd */\n");

	printf("};\n");

	return;
}
/* Generate an array of state set control blocks (SSCB),
 one entry for each state set */
gen_sscb_array()
{
	extern Expr		*ss_list;
	Expr			*ssp;
	int			nss, nstates, n;
	bitMask			events[NWRDS];

	printf("\n/* State Set Control Blocks */\n");
	printf("static SSCB sscb[NUM_SS] = {\n");
	nss = 0;
	for (ssp = ss_list; ssp != NULL; ssp = ssp->next)
	{
		if (nss > 0)
			printf(",\n\n");
		nss++;

		printf("\t/* State set \"%s\"*/\n", ssp->value);

		printf("\t0, 1, 0,\t/* task_id, task_prioity, sem_id */\n");

		nstates = exprCount(ssp->left);
		printf("\t%d, state_%s,\t/* num_states, *states */\n", nstates,
		 ssp->value);

		printf("\t0, 0, 0,");
		printf("\t/* current_state, next_state, prev_state */\n");

		printf("\t%d,\t/* error_state */\n", find_error_state(ssp));

		printf("\t0, FALSE,\t/* trans_number, action_complete */\n");

		printf("\t/* outstanding events */\n\t");
		for (n = 0; n < NWRDS; n++)
			printf("0, ");
		printf("\n");

		printf("\t0,\t/* pMask - ptr to current event mask */\n");

		printf("\t0,\t/* number of delays in use */\n");

		printf("\t/* array of delay values: */\n\t");
		for (n = 0; n < MAX_NDELAY; n++)
			printf("0,");
		printf("\n");

		printf("\t0,\t/* time (ticks) when state was entered */\n");

		printf("\t\"%s\"\t\t/* ss name */", ssp->value);
	}
	printf("\n};\n");
	return;
}

/* Find the state named "error" in a state set */
find_error_state(ssp)
Expr		*ssp;
{
	Expr		*sp;
	int		error_state;
	for (sp = ssp->left, error_state = 0; sp != 0; sp = sp->next, error_state++)
	{
		if (strcmp(sp->value, "error") == 0)
			return error_state;
	}
	return -1; /* no state named "error" in this state set */
}

/* Evaluate composite event mask for a single state */
eval_state_event_mask(sp, events)
Expr		*sp;
bitMask		events[NWRDS];
{
	int		n;
	int		eval_tr_event_mask();
	Expr		*tp;

	/* Clear all bits in mask */
	for (n = 0; n < NWRDS; n++)
		events[n] = 0;

	/* Set appropriate bits based on transition expressions */
	for (tp = sp->left; tp != 0; tp = tp->next)
		traverseExprTree(tp->left, E_VAR, 0,
		 eval_tr_event_mask, events);
}

/* Evaluate the event mask for a given transition. */
eval_tr_event_mask(ep, events)
Expr		*ep;
bitMask		events[NWRDS];
{
	Chan		*cp;
	Var		*vp;

	vp = (Var *)ep->left;
	if (vp == 0)
		return; /* this shouldn't happen */

#ifdef	DEBUG
	fprintf(stderr, "eval_tr_event_mask: %s, ef_num=%d\n",
	 vp->name, vp->ef_num);
#endif
	if (vp->ef_num != 0)
		bitSet(events, vp->ef_num);
}

/* Count the number of linked expressions */
exprCount(ep)
Expr		*ep;
{
	int		count;

	for (count = 0; ep != 0; ep = ep->next)
		count++;
	return count;
}

