/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	@(#)snc_main.c	1.1	10/16/90
	DESCRIPTION: Main program and miscellaneous routines for
	State Notation Compiler.

	ENVIRONMENT: UNIX
***************************************************************************/
extern	char *sncVersion;

#include	<stdio.h>

/* SNC Globals: */
int	line_no = 1;	/* input line number */
char	in_file[200];	/* input file name */
char	out_file[200];	/* output file name */

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

	/* Use line buffered output */
	setlinebuf(stdout);
	
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
	printf(
	 "/* %s: %s */\n\n", sncVersion, in_file);

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
	extern	char in_file[], out_file[];
	int	ls;
	char	*s;

	if (argc < 2)
	{
		fprintf(stderr, "%s\n", sncVersion);
		fprintf(stderr, "Usage: snc infile\n");
		exit(1);
	}
	s = argv[1];

	ls = strlen(s);
	bcopy(s, in_file, ls);
	in_file[ls] = 0;
	bcopy(s, out_file, ls);
	if ( strcmp(&in_file[ls-3], ".st") == 0 )
	{
		out_file[ls-2] = 'c';
		out_file[ls-1] = '\0';
	}
	else
	{
		out_file[ls] = '.';
		out_file[ls+1] = 'c';
		ls += 2;
	}
	out_file[ls] = 0;
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
snc_err(err_txt, line, code)
char	*err_txt;
int	line, code;
{
	fprintf(stderr, "Syntax error %d (%s) at line %d\n",
	 code, err_txt, line);
	exit(code);
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
	fprintf(stderr, "%s: line no. %d\n", err, line_no);
	return;
}

/*+************************************************************************
*  NAME: print_line_num
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	---------------------------------------------------
*	int		line_num	I	current line number
*
*  RETURNS: n/a
*
*  FUNCTION: Prints the line number and input file name for use by the
*	C preprocessor.  e.g.: # line 24 "something.st"
*
*  NOTES:
*-*************************************************************************/
print_line_num(line_num)
int	line_num;
{
	printf("# line %d \"%s\"\n", line_num, in_file);
	return;
}
