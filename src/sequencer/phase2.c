/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		 Los Alamos National Laboratory

	@(#)phase2.c	1.4	1/16/91
	DESCRIPTION: Phase 2 code generation routines for SNC.
	Produces code and tables in C output file.
	See also:  gen_ss_code.c
	ENVIRONMENT: UNIX
***************************************************************************/

#include	<stdio.h>
#include	"parse.h"
#include	"seq.h"

/*+************************************************************************
*  NAME: phase2
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
int	num_vars = 0;		/* number of variables declared */
int	num_channels = 0;	/* number of db channels */
int	num_ss = 0;		/* number of state sets */
int	num_errors = 0;		/* number of errors detected in phase 2 processing */

phase2()
{
	extern	LIST	var_list;	/* variables (from parse) */
	extern	LIST	ss_list;	/* state sets (from parse) */
	extern	char	*prog_name;	/* program name (from parse) */
	extern	char	*global_c_code;	/* global C code */

	/* Count number of variables, db channels, and state sets defined */
	num_vars = lstCount((NODE *)&var_list);
	num_channels = db_chan_count();
	num_ss = lstCount((NODE *)&ss_list);

	/* Generate preamble code */
	gen_preamble();

	/* Generate variable declarations */
	gen_var_decl();

	/* Generate definition C code */
	gen_defn_c_code();

	/* Assign bits for event flags */
	assign_ef_bits();

	/* Generate DB blocks */
	gen_db_blocks();

	/* Generate code for each state set */
	gen_ss_code();

	/* Generate State Blocks */
	gen_state_blocks();

	/* Generate State Set control blocks as an array */
	gen_sscb_array();

	/* Generate state program table */
	gen_state_prog_table();

	/* Output global C code */
	printf("%s\n", global_c_code);

	exit(0);
}

/* Generate preamble (includes, defines, etc.) */
gen_preamble()
{
	printf("/* Program \"%s\" */\n", prog_name);

	/* Include files */
	printf("#include \"seq.h\"\n");

	/* Local definitions */
	printf("\n#define NUM_SS %d\n", num_ss);
	printf("#define NUM_VARS %d\n", num_vars);
	printf("#define NUM_CHANNELS %d\n", num_channels);

	/* Forward definition of SPROG structure */
	printf("\nextern SPROG %s;\n", prog_name);

	return;
}

/* Generate a C variable declaration for each variable declared in SNL */
gen_var_decl()
{
	extern	LIST var_list;
	Var	*vp;
	char	*vstr, *vname="User_Var_";
	int	nv;

	printf("\n/* Variable declarations */\n");
	for (nv=0, vp = firstVar(&var_list); vp != NULL; nv++, vp = nextVar(vp))
	{
		if (vp->type != V_EVFLAG)
			printf("#define %s %s.var%d\n", vp->name, vname, nv);
	}
	printf("static struct %s {\n", vname);
	for (nv=0, vp = firstVar(&var_list); vp != NULL; nv++, vp = nextVar(vp))
	{
		switch (vp->type)
		{
		case V_CHAR:
			vstr = "char";
			break;
		case V_INT:
			vstr = "int";
			break;
		case V_SHORT:
			vstr = "short";
			break;
		case V_FLOAT:
			vstr = "float";
			break;
 		case V_DOUBLE:
			vstr = "double";
			break;
		case V_STRING:
			vstr = "char";
			break;
		case V_EVFLAG:
			vstr = NULL;
			break;
		default:
			vstr = "int";
			break;
		}
		if (vstr != NULL)
		{
			printf("\t%s\tvar%d", vstr, nv);
			/* Array? */
			if (vp->length > 0)
				printf("[%d];\n", vp->length);
			else if (vp->type == V_STRING)
				printf("[MAX_STRING_SIZE];\n");
			else
				printf(";\n");
		}
	}
	printf("} %s;\n", vname);
	return;
}		

/* Generate definition C code (C code in definition section) */
gen_defn_c_code()
{
	extern	LIST defn_c_list;
	Action	*ap;

	ap = firstAction(&defn_c_list);
	if (ap != NULL)
	{
		printf("\n\t/* C code definitions */\n");
		for (; ap != NULL; ap = nextAction(ap))
		{
			print_line_num(ap->line_num);
			printf("%s\n", ap->stmt.c_code);
		}
	}
	return;
}

/* Generate the structure with data for a state program table (SPROG) */
gen_state_prog_table()
{
	extern	char *prog_param;

	printf("\n/* Program parameter list */\n");

	printf("static char prog_param[] = \"%s\";\n", prog_param);

	printf("\n/* State Program table (global) */\n");

	printf("SPROG %s = {\n", prog_name);

	printf("\t%d,\t/* magic number */\n", MAGIC);

	printf("\t0,\t/* task id */\n");

	printf("\t0,\t/* task_is_deleted */\n");

	printf("\t1,\t/* relative task priority */\n");

	printf("\t0,\t/* sem_id */\n");

	printf("\tdb_channels,\t/* *channels */\n");

	printf("\tNUM_CHANNELS,\t/* nchan */\n");

	printf("\tsscb,\t/* *sscb */\n");

	printf("\tNUM_SS,\t/* nss */\n");

	printf("\t&User_Var_,\t/* ptr to user area */\n");

	printf("\t4,\t/* user area size */\n");

	printf("\t\"%s\",\t/* *name */\n", prog_name);

	printf("\tprog_param\t/* *params */\n");

	printf("};\n");

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
			fill_db_block(cp);
			nchan++;
		}
	}
	printf("\n};\n");
	return;
}

/* Fill in a db block with data (all elements for "CHAN" struct) */
fill_db_block(cp)
Chan	*cp;
{
	Var	*vp;
	char	*dbr_type_string;
	extern	char *prog_name;
	int	size, ev_flag;

	vp = cp->var;
	switch (vp->type)
	{
	case V_SHORT:
		dbr_type_string = "DBR_INT";
		size = sizeof(short);
		break;
	case V_INT:
		dbr_type_string = "DBR_INT";
		size = sizeof(int);
		break;
	case V_CHAR:
		dbr_type_string = "DBR_INT";
		size = sizeof(char);
		break;
	case V_FLOAT:
		dbr_type_string = "DBR_FLOAT";
		size = sizeof(float);
		break;
	case V_DOUBLE:
		dbr_type_string = "DBR_FLOAT";
		size = sizeof(double);
		break;
	case V_STRING:
		dbr_type_string = "DBR_STRING";
		size = sizeof(char);
		break;
	}
	/* fill in the CHAN structure */
	printf("\t\"%s\"", cp->db_name);/* unexpanded db channel name */

	printf(", (char *) 0");		/* ch'l name after macro expansion */

	printf(", (chid)0");		/* reserve place for chid */

	if (vp->type == V_STRING)
		printf(", %s", vp->name);	/* variable ptr */
	else
		printf(", &%s", vp->name);	/* variable ptr */
	if (vp->length != 0)
		printf("[%d]", cp->offset); /* array offset */

	printf(", %d", size);		/* element size (bytes) */

	printf(",\n\t %s", dbr_type_string);/* DB request conversion type */

	printf(", %d", vp->length);	/* array length (of the variable) */

	printf(", %d", cp->mon_flag);	/* monitor flag */

	printf(", %d", cp->length);	/* count for db requests */

	printf(", %g", cp->delta);	/* monitor delta */

	printf(", %g", cp->timeout);	/* monitor timeout */

	printf(", &%s", prog_name);	/* ptr to state program structure */

	vp = cp->ef_var;
	if (vp == 0)
		ev_flag = 0;
	else
		ev_flag = 1 << vp->ef_num;
	printf(", %d", ev_flag);	/* sync event flag */

	return;
}

/* Generate structure and data for state blocks (STATE) */
gen_state_blocks()
{
	extern	LIST ss_list;
	StateSet	*ssp;
	State	*sp;
	int	nstates;

	printf("\n/* State Blocks */\n");
	printf("/* action_func, event_func, delay_func, event_flag_mask");
	printf(", *delay, *name */\n");

	for (ssp = firstSS(&ss_list); ssp != NULL; ssp = nextSS(ssp))
	{
		printf("\nstatic STATE state_%s[] = {\n", ssp->ss_name);
		nstates = 0;
		for (sp = firstState(&ssp->S_list); sp != NULL; sp = nextState(sp))
		{
			if (nstates > 0)
				printf(",\n\n");
			nstates++;
			fill_state_block(sp, ssp->ss_name);
		}
		printf("\n};\n");
	}
	return;
}

/* Fill in data for a the state block */
fill_state_block(sp, ss_name)
State	*sp;
char	*ss_name;
{
	printf("\t/* State \"%s\"*/\n", sp->state_name);

	printf("\ta_%s_%s,\t/* action_function */\n", ss_name, sp->state_name);

	printf("\te_%s_%s,\t/* event_function */\n", ss_name, sp->state_name);

	printf("\td_%s_%s,\t/* delay_function */\n", ss_name, sp->state_name);

	printf("\t%d,\t/* event_flag_mask */\n", sp->event_flag_mask);

	printf("\t\"%s\"\t/* *name */", sp->state_name);

	return;
}

/* Generate a state set control block (SSCB) for each state set */
gen_sscb_array()
{
	extern	LIST ss_list;
	StateSet	*ssp;
	int	nss, nstates, n;

	printf("\n/* State Set Control Blocks */\n");
	printf("static SSCB sscb[NUM_SS] = {\n");
	nss = 0;
	for (ssp = firstSS(&ss_list); ssp != NULL; ssp = nextSS(ssp))
	{
		if (nss > 0)
			printf(",\n\n");
		nss++;
		nstates = lstCount((NODE *)&ssp->S_list);

		printf("\t/* State set \"%s\"*/\n", ssp->ss_name);

		printf("\t0, 1, 0,\t/* task_id, task_prioity, sem_id */\n");

		printf("\t0, 0,\t\t/* event_flag, event_flag_mask */\n");

		printf("\t%d, state_%s,\t/* num_states, *states */\n", nstates,
		 ssp->ss_name);

		printf("\tNULL, NULL,\t/* *current_state, *next_state */\n");

		printf("\t0, FALSE,\t/* trans_number, action_complete */\n");

		printf("\t%d,\t/* number of time-out structures */\n", ssp->ndelay);

		printf("\t/* array of time-out values: */\n\t");
		for (n = 0; n < MAX_NDELAY; n++)
			printf("0,");
		printf("\n");

		printf("\t0,\t/* time (ticks) when state was entered */\n");

		printf("\t\"%s\"\t\t/* ss_name */", ssp->ss_name);
	}
	printf("\n};\n");
	return;
}

/* Returns number of db channels defined & inserts index into each channel struct */
db_chan_count()
{
	extern	LIST	var_list;
	int	nchan;
	Chan	*cp;

	nchan = 0;
	for (cp = firstChan(&chan_list); cp != NULL; cp = nextChan(cp))
	{
		if (cp->db_name != NULL)
		{
			cp->index = nchan;
			nchan++;
		}
	}
	return nchan;
}


/* Given a state name and state set struct, find the corresponding
   state struct and return its index (1-st one is 0) */
state_block_index_from_name(ssp, state_name)
StateSet *ssp;
char	*state_name;
{
	State	*sp;
	int	index;

	index = 0;
	for (sp = firstState(&ssp->S_list); sp != NULL; sp = nextState(sp))
	{
		if (strcmp(state_name, sp->state_name) == 0)
			return index;
		index++;
	}
	return -1; /* State name non-existant */
}

/* Assign bits to event flags */
assign_ef_bits()
{
	extern	LIST var_list;
	Var	*vp;
	int	ef_num;

	ef_num = 0;
	printf("\n/* Event flags */\n");
	for (vp = firstVar(&var_list); vp != NULL; vp = nextVar(vp))
	{
		if (vp->type == V_EVFLAG)
		{
			vp->ef_num = ef_num;
			printf("#define %s (1<<%d)\n", vp->name, ef_num);
			ef_num++;
		}
	}
	return;
}
