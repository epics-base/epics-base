/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	@(#)snc_main.c	1.2	4/17/91
	DESCRIPTION: Main program and miscellaneous routines for
	State Notation Compiler.

	ENVIRONMENT: UNIX
***************************************************************************/
extern	char *sncVersion;

#include	<stdio.h>

#ifndef	TRUE
#define	TRUE 1
#define	FALSE 0
#endif

/* SNC Globals: */
char		in_file[200];	/* input file name */
char		out_file[200];	/* output file name */
char		*src_file;	/* ptr to (effective) source file name */
int		line_num;	/* current src file line number */
int		c_line_num;	/* line number for beginning of C code */
/* Flags: */
int		async_flag = FALSE;	/* do pvGet() asynchronously */
int		conn_flag = TRUE;	/* wait for all connections to complete */
int		debug_flag = FALSE;	/* run-time debug */
int		line_flag = TRUE;	/* line numbering */
int		reent_flag = FALSE;	/* reentrant at run-time */
int		warn_flag = TRUE;	/* compiler warnings */

/*+************************************************************************
*  NAME: main
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	-------------------------------------------------------------
*	int		argc		I	arg count
*	char		*argv[]		I	array of ptrs to args
*
*  RETURNS: n/a
*
*  FUNCTION: Program entry.
*
*  NOTES: The streams stdin and stdout are redirected to files named in the
* command parameters.  This accomodates the use by lex of stdin for input
* and permits printf() to be used for output.  Stderr is not redirected.
*
* This routine calls yyparse(), which never returns.
*-*************************************************************************/
main(argc, argv)
int	argc;
char	*argv[];
{
	FILE	*infp, *outfp, *freopen();
	extern	char in_file[], out_file[];

	/* Get command arguments */
	get_args(argc, argv);

	/* Redirect input stream from specified file */
	infp = freopen(in_file, "r", stdin);
	if (infp == NULL)
	{
		perror(in_file);
		exit(1);
	}

#define	REDIRECT
#ifdef	REDIRECT
	/* Redirect output stream to specified file */
	outfp = freopen(out_file, "w", stdout);
	if (outfp == NULL)
	{
		perror(out_file);
		exit(1);
	}
#endif	REDIRECT

	/* src_file is used to mark the output file for snc & cc errors */
	src_file = in_file;

	/* Use line buffered output */
	setlinebuf(stdout);
	setlinebuf(stderr);
	
	printf("/* %s: %s */\n\n", sncVersion, in_file);

	/* Initialize parser */
	init_snc();

	/* Call the SNC parser */
	yyparse();
}
/*+************************************************************************
*  NAME: get_args
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	-----------------------------------------------------------
*	int		argc		I	number of arguments
*	char		*argv[]		I	shell command arguments
*  RETURNS: n/a
*
*  FUNCTION: Get the shell command arguments.
*
*  NOTES: If "*.s" is input file then "*.c" is the output file.  Otherwise,
*  ".c" is appended to the input file to form the output file name.
*  Sets the gloabals in_file[] and out_file[].
*-*************************************************************************/
get_args(argc, argv)
int	argc;
char	*argv[];
{
	char	*s;

	if (argc < 2)
	{
		fprintf(stderr, "%s\n", sncVersion);
		fprintf(stderr, "Usage: snc +/-flags infile\n");
		fprintf(stderr, "  +a - do async. pvGet\n");
		fprintf(stderr, "  -c - don't wait for all connects\n");
		fprintf(stderr, "  +d - turn on debug run-time option\n");
		fprintf(stderr, "  -l - supress line numbering\n");
		fprintf(stderr, "  +r - make reentrant at run-time\n");
		fprintf(stderr, "  -w - supress compiler warnings\n");
		exit(1);
	}

	for (argc--, argv++; argc > 0; argc--, argv++)
	{
		s = *argv;
		if (*s == '+' || *s == '-')
			get_flag(s);
		else
			get_in_file(s);
	}
}

get_flag(s)
char		*s;
{
	int		flag_val;
	extern int	debug_flag, line_flag, reent_flag, warn_flag;

	if (*s == '+')
		flag_val = TRUE;
	else
		flag_val = FALSE;

	switch (s[1])
	{
	case 'a':
		async_flag = flag_val;
		break;

	case 'c':
		conn_flag = flag_val;
		break;

	case 'd':
		debug_flag = flag_val;
		break;

	case 'l':
		line_flag = flag_val;
		break;

	case 'r':
		reent_flag = flag_val;
		break;

	case 'w':
		warn_flag = flag_val;
		break;

	default:
		fprintf(stderr, "Unknown flag: \"%s\"\n", s);
		break;
	}
}

get_in_file(s)
char		*s;
{				
	extern char	in_file[], out_file[];
	int		ls;

	ls = strlen(s);
	bcopy(s, in_file, ls);
	in_file[ls] = 0;
	bcopy(s, out_file, ls);
	if ( strcmp(&in_file[ls-3], ".st") == 0 )
	{
		out_file[ls-2] = 'c';
		out_file[ls-1] = 0;
	}
	else if (in_file[ls-2] == '.')
	{	/* change suffix to 'c' */
		out_file[ls -1] = 'c';
	}
	else
	{	/* append ".c" */
		out_file[ls] = '.';
		out_file[ls+1] = 'c';
		out_file[ls+2] = 0;
	}
	return;
}
/*+************************************************************************
*  NAME: snc_err
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	-------------------------------------------------------------------
*	char		*err_txt	I	Text of error msg.
*	int		line		I	Line no. where error ocurred
*	int		code		I	Error code (see snc.y)
*
*  RETURNS: no return
*
*  FUNCTION: Print the SNC error message and then exit.
*
*  NOTES:
*-*************************************************************************/
snc_err(err_txt)
char	*err_txt;
{
	fprintf(stderr, "     %s\n", err_txt);
	exit(1);
}
/*+************************************************************************
*  NAME: yyerror
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	---------------------------------------------------
*	char		*err		I	yacc error
*
*  RETURNS: n/a
*
*  FUNCTION: Print yacc error msg
*
*  NOTES:
*-*************************************************************************/
yyerror(err)
char	*err;
{
	extern char	*src_file;
	extern int	line_num;

	fprintf(stderr, "%s: line no. %d (%s)\n", err, line_num, src_file);
	return;
}

/*+************************************************************************
*  NAME: print_line_num
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	---------------------------------------------------
*	int		line_num	I	current line number
 *	char		src_file	I	effective source file
*
*  RETURNS: n/a
*
*  FUNCTION: Prints the line number and input file name for use by the
*	C preprocessor.  e.g.: # line 24 "something.st"
*
*  NOTES:
*-*************************************************************************/
print_line_num(line_num, src_file)
int		line_num;
char		*src_file;
{
	extern int	line_flag;

	if (line_flag)
		printf("# line %d \"%s\"\n", line_num, src_file);
	return;
}
