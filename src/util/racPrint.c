/* epics/share/src/util $Id$
 *	Author:	Roger A. Cole
 *	Date:	mm-dd-yy
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * 1.1	07/12/89	rac	initial version
 * 1.2	09/15/89	rac	allow multiple input files; change
 *				the calling sequence so -o specifies
 *				output file; print non-printing chars
 *				as <code>.
 * 1.3	11/02/89	rac	change printing of <; allow default
 *				input from stdin.  If first line on page
 *				is FF, don't print empty page.
 * 1.4	11/03/89	rac	fix bug in input from stdin.
 * 1.5	12/04/89	rac	handle long lines without truncation
 * 1.6	02/11/90	rac	handle module name and subr name in heading;
 *				put page number in footing.
 * 1.7				skipped
 * 1.8	08/31/90	rac	handle /subhead text--- divider lines
 * 1.9	09/17/90	rac	fix long line bug; allow more pages
 * 1.10	07-28-91	rac	installed in EPICS
 *
 */
/*+/mod***********************************************************************
* TITLE	racPrint - filter for printing
*
* SYNOPSIS
*	racPrint [-l lm] [-o output] [-t tab] input ...
*	    
*	   lm	is the number of columns for left margin; default 0.
*	   tab	is the tab width, in columns; default 8.
*	   input   is an ascii file; several may be specified; if none
*		is specified, stdin is used.
*	   output  is an ascii file containing the input file broken
*		into pages, with pages in reverse order;
*		default is stdout.
*
* DESCRIPTION
*	The input file is broken into pages and written in reverse page
*	order to the output file.  The following processing is done:
*	o	a 3 line heading is written, containing time and date, file
*		path, file name, modification date, and subroutien name is
*		printed
*	o	a 3 line footing containing file number and page number is
*		printed
*	o	a left margin of 8 columns is added, plus an additional 7
*		columns for line numbering.  (If -l is specified, it
*		controls the left margin.)
*	o	tabs are expanded
*	o	form-feed characters are honored
*	o	long lines are folded
*	o	non-printing characters are printed as <code>, where
*		code is the hexadecimal code for the character.  Where
*		a < occurs, it is printed as a <; hopefully, the context
*		will avoid confusion with <code>.
* EXAMPLE
*	% racPrint source.c | lpr
*
* BUGS
*	o should guard against printing binary files
*	o files which exactly fill the last page result in an extra, blank
*	  page being printed.
*	o Doesn't print info following ^L if <cr> doesn't follow ^L
*
*-*/
/*-----------------------------------------------------------------------------
* APOLOGY:  I offer my humble apologies to whomever comes across this code--
* it was my first program in C, and it shows it.  R. Cole
*----------------------------------------------------------------------------*/

/* WISH LIST:
*  - generate a 'table of contents' from names from divider lines
*  - generate table of contents for file names and extractable entities
*  - add a -help switch
*  - handle the  backspace character
*  - handle "pre-formatted" files, such as man pages output
*  - handle extremely long path names, or at least check for overflow
*  - allow a `-H heading' option to provide a meaningful heading when
*    input is from stdin
*/
 
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#define MAXCHAR 95		/* max characters per line */
#define MAXLINE 66		/* max lines per page */
#define MAXPAGE 500		/* max pages at a time */
#define MAXTOC 10		/* max table of contents pages */
#define lineBufDim 10000	/* dimension for input line and ovf buffers */

#define STRCAT_CHK(str,sub_str) strcat_chk(str,sizeof str,sub_str)

typedef struct
{	int	page_no;	/* page number */
	int	nlines;		/* number of lines on this page */
	char	lines[MAXLINE][MAXCHAR+1];	/* lines of the page */
} PAGE;


struct FILE_INF
{	FILE	*ptr;		/* file pointer from 'fopen' */
	char	path[80];	/* pathname to file */
	char	date_mod[30];	/* time and date last modified */
	short	line_no;	/* line number in file */
	char	name[20];	/* file name */
	char	subr[30];	/* subroutine name from subr heading */
	int	fd;		/* file descriptor for use in 'stat' call */
};

char		date[40];	/* date and time */
char		header[80];	/* heading line */
int		page_no;	/* number of current page within file */
int		file_no = 0;	/* file number being processed */
int		npages = 0;	/* number of pages */
PAGE		pages[MAXPAGE];	/* pages from file */
int		pages_sub = 0;	/* subscript into pages structure */
PAGE		toc[MAXTOC];	/* table of contents */
int		toc_sub = 0;	/* subscript into toc structure */

int		left_margin = 0;	/* width of left margin in output */
int		line_numbering = 1;	/* flag to number lines in output */
int		tab_width = 8;	/* columns per 'tab stop' */
char	overflow[lineBufDim];	/* overflow record buffer */
int	overflow_flag = 0;	/* overflow record buffer in use */


/*+*****************************************************************
* NAME	main - racPrint main program
*
*-*/
main(argc, argv)
    int     argc;		/* number of command line args */
    char   *argv[];		/* command line args */
{
    void	get_time_and_date();	/* get time and date string */
    int		i;		/* temp for loops */
    struct FILE_INF in_file;	/* input file */
    struct FILE_INF out_file;	/* output file */

    char	line[MAXCHAR];	/* line of file */
    int		get_next_page();	/* get a page from the file */

    get_time_and_date(date);

/*   -------------------------------------------------------------
*    get the option arguments from the command line
*    -------------------------------------------------------------*/

    get_option_args(argc, argv, &out_file);

/*   -------------------------------------------------------------
*    get the input file names from the command line, processing each
*    in turn.
*    -------------------------------------------------------------*/

    while (get_input_file(argc, argv, &in_file) != NULL) {

/*   -------------------------------------------------------------
*    Read pages from input file.  If 'buffer' becomes full, print
*    the accumulated pages in reverse order.
*    -------------------------------------------------------------*/

	in_file.line_no = 0;
	while (get_next_page(&in_file) != EOF) {
	    if (pages_sub == MAXPAGE) { /* dump and start fresh */
		for (i=pages_sub-1; i>=0; i--)
		print_page(&out_file,i);
		pages_sub = 0;
	    }
	}
	if (in_file.ptr != stdin)	/* don't close stdin */
	    fclose(in_file.ptr);
    }

    for (i=pages_sub-1; i>=0; i--) {
	    print_page(&out_file,i);
    }

    print_banner_page(&in_file,&out_file);

/* flush and close files */
    fflush(out_file.ptr);
    fclose(out_file.ptr);

    exit(0);
}

/*+/subr************************************************************
* NAME	print_banner_page - write a banner page to the output file
*
*-*/
print_banner_page(p_in_file,p_out_file)
    struct FILE_INF *p_in_file;		/* pointer to input file */
    struct FILE_INF *p_out_file;	/* pointer to output file */
{
    char	banner[80];	/* temp for banner line */
    char	*env;		/* pointer to string from getenv */
    char	*getenv();	/* get environment variables */
    int		i;		/* temp for loops */

    for (i=0; i<79; i++)
        banner[i] = '/';
    banner[79] = '\0';

    env = getenv("LOGNAME");

    for (i=0; i<MAXLINE-1; i++) {
        if (i<10)
            fprintf(p_out_file->ptr,
                              "%s\n",banner);
        else if (i==15)
            fprintf(p_out_file->ptr,
                              "//////////   For:    %s\n",env);
        else if (i==17)
            fprintf(p_out_file->ptr,
                              "//////////   Date:   %s\n",date);
        else
            fprintf(p_out_file->ptr,
                              "//////////   \n");
	
	}
	
	return;
}

/*+/subr************************************************************
* NAME	print_page - write a page to the output file
*
*-*/
print_page(p_out_file,page_number)
    struct FILE_INF *p_out_file;	/* pointer to file */
    int	page_number;			/* page number to print 0-n */
{
    int		i;	/* temp for loops */

    for (i=0; i<pages[page_number].nlines; i++)
        fputs(pages[page_number].lines[i],p_out_file->ptr);

    return;
}

/*+/subr************************************************************
* NAME	line_store - store a line into a page
*
* DESCRIPTION
*	The record is stored into the line buffer, expanding tabs.
*
*	If the record isn't an overflow record, left margin and
*	record number are added to the line buffer.  If the
*	record is too long to fit in the line buffer, the overflow
*	flag is set and the excess characters are copied into the
*	overflow record.
*
*	If the record is an overflow record, the left margin and
*	padding for the record number field are added to the line
*	buffer.  If the record is too long to fit in the line buffer,
*	the overflow flag and overflow record will be handled as above.
*
* RETURNS
*	0 if no form-feed encountered
*	1 if form-feed encountered and line printed
*	2 if form-feed encountered and no line printed
*
*-*/
line_store (line, rec, line_no)
    char	line[];		/* line to receive record, with tabs
				   expanded and left margin set */
    char	rec[];		/* record to store */
    int		line_no;	/* line number to attach; if zero,
				   no line number will appear. */
{
    char	c;		/* temp for character */
    int		ff=0;		/* form feed encountered flag */
    char	hex_code[3];	/* temp for hex code for char */
    int		l_col=0;	/* column index in line */
    int		lmar;		/* left margin to store actual record */
    int		r_col=0;	/* column index in record */

/*   -------------------------------------------------------------
*    If the first thing on a line is a ^L (i.e., ff), then don't
*    do anything; just exit with `ff' indicated.
*    -------------------------------------------------------------*/
    if (rec[0] == '\f') {
	ff = 2;
	return ff;
    }

    while (l_col < left_margin)
	line[l_col++] = ' ';	/* blank fill left margin */

    if (!overflow_flag) {
	if (line_no > 0) {	 /* 'print' the line number */
	    sprintf (&line[l_col], "%.5d", line_no);
	    l_col += 5;
	    line[l_col++] = ' ';
	    line[l_col++] = ' ';
	}
    }
    else {
	sprintf(&line[l_col], "     ");
	l_col += 5;
	line[l_col++] = ' ';
	line[l_col++] = ' ';
    }

/*   -------------------------------------------------------------
*    Move the record into the line buffer, expanding tabs.  If record is
*    too long, the overflow is invoked.  Line buffer will always be
*    terminated with \n\0.  Non-printing characters will be shown as
*    <hh>, where hh is the hex code for the character.  Where < occurs
*    in the text, it will be shown as itself.
*    -------------------------------------------------------------*/
#define store_c(c) \
if (l_col < MAXCHAR-1)\
    line[l_col++] = c;\
else\
    line[lmar-2] = '*';

    overflow_flag = 0;	/* reset overflow flag */
    lmar = l_col;	/* save this to make tab expansion 'sane' */
    while ((c=rec[r_col++]) != NULL && !overflow_flag) {
        if (l_col < MAXCHAR-2) {
            switch (c) {
                case '\t':
		    line[l_col++] = ' ';	
		    while (l_col < MAXCHAR-1 &&
					    ((l_col-lmar) % tab_width) != 0)
                        line[l_col++] = ' ';
		    break;
                case '\f':
		    ff = 1;
		    break;
                case '\n':
		    break;
		case '<':
		    store_c(c);
		    break;
                default:
		    if (isprint(c))
			line[l_col++] = c;
		    else {
			store_c('<');
			sprintf(hex_code,"%02.2X",c);
			store_c(hex_code[0]);
			store_c(hex_code[1]);
			store_c('>');
		    }
		    break;
            }
        }
        else {
	    r_col--;
	    strcpy(overflow, &rec[r_col]);
	    overflow_flag = 1;
	}
    }

    line[l_col++] = '\n';
    line[l_col++] = '\0';
	
    return ff;
}

/*+/subr************************************************************
* NAME	get_next_page - format the next page for printing
*
* DESCRIPTION
*
* RETURNS
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
int get_next_page(p_in_file)
    struct FILE_INF *p_in_file;	/* pointer to input file */
{
    char	buf_in[lineBufDim];/* input buffer */
    int		buf_in_len;	/* length of input line */
    int		col = 0;	/* temp for column in line */
    int		eof = 0;	/* end-of-file flag */
    int		ff;		/* form feed flag */
    int		line = 0;	/* temp for line in page */


    line = 3;

/*   -------------------------------------------------------------
*    read the lines from the file for one page, leaving room for 'footing'
*    If the overflow flag is set, input line is taken from overflow
*    buffer instead of file.
*    -------------------------------------------------------------*/
    while (line < MAXLINE-3 && eof == 0) {
        if (overflow_flag) {
	    strcpy(buf_in, overflow);
	}
        else if (fgets(buf_in, lineBufDim, p_in_file->ptr) != NULL ) {
            p_in_file->line_no++;
	}
	else
            eof = EOF;		/* no more to read */
	
	if (!eof) {
	    if (strncmp(buf_in, "* NAME", 6) == 0) {
		sscanf(&buf_in[6], "%29s", p_in_file->subr);
	    }
	    else if (strncmp(buf_in, "/*/subhead ", 11) == 0) {
		int	i;
		int	c;
		i = 0;
		while (i< 29 && (c=buf_in[11+i])!='-')
		    p_in_file->subr[i++] = c;
		p_in_file->subr[i] = '\0';
		
	    }
            ff = line_store(pages[pages_sub].lines[line++], buf_in,
		    		line_numbering ? p_in_file->line_no : 0);
	    if (ff == 2  &&  line == 4) {  /* FF on first line of page */
                line--;		/* throw away the FF; already new page */
	    }
            else if (ff) {  /* if FF encountered, pad rest of page */
		if (ff == 2) /* if no line printed, reset line_no */
	            line--;
                while (line < MAXLINE-3)
                    pages[pages_sub].lines[line++][0] = '\n';
    	    }
        }
    }

/*----------------------------------------------------------------------------
*    set up the footing
*---------------------------------------------------------------------------*/
    for(; line<MAXLINE-2; line++)
        strcpy(pages[pages_sub].lines[line],"\n");
    if (line < MAXLINE-1) {
	for (col=0; col<(MAXCHAR-12)/2; col++)
	    pages[pages_sub].lines[line][col] = ' ';
	sprintf(&(pages[pages_sub].lines[line][col]), 
		"Page %d-%d\n", file_no, page_no);
	line++;
    }
    if (line < MAXLINE) {
        strcpy(pages[pages_sub].lines[line],"\n");
	line++;
    }

    pages[pages_sub].nlines = line;

/*   -------------------------------------------------------------
*    set up the header; if input is from stdin, suppress path and
*    modification date for file.
*    -------------------------------------------------------------*/

    line = 0;	/* line 0 has time, date, mod. date, file name */
    col = 0;
    while (col<left_margin)	/* put left margin into line */
        pages[pages_sub].lines[line][col++] = ' ';
    sprintf(&pages[pages_sub].lines[line][col],"%s  %s  %s",
			date,
	(p_in_file->date_mod[0] != '\0') ? "last modified on " : " ",
			p_in_file->date_mod);
    for (col=strlen(pages[pages_sub].lines[line]);
		col<MAXCHAR-2-strlen(p_in_file->name); col++)
    	pages[pages_sub].lines[line][col] = ' ';
    sprintf(&(pages[pages_sub].lines[line][MAXCHAR-2-strlen(p_in_file->name)]),
		"%s\n", p_in_file->name);

    line = 1;	/* line 1 has path and subroutine name */
    col = 0;
    while (col<left_margin)	/* put left margin into line */
        pages[pages_sub].lines[line][col++] = ' ';
    sprintf(&pages[pages_sub].lines[line][col],"%s",
			p_in_file->path);
    for (col=strlen(pages[pages_sub].lines[line]);
		col<MAXCHAR-2-strlen(p_in_file->subr); col++)
    	pages[pages_sub].lines[line][col] = ' ';
    sprintf(&(pages[pages_sub].lines[line][MAXCHAR-2-strlen(p_in_file->subr)]),
		"%s\n", p_in_file->subr);

    line++;	/* line 2 is blank */
    strcpy(pages[pages_sub].lines[line],"\n");

    pages[pages_sub].page_no = ++npages;
    pages_sub++;
    page_no++;

    return eof;

}

/*+/subr************************************************************
* NAME	get_option_args - get option args from command line
*
* DESCRIPTION
*	Gets the command line options, except for input file.  If
*	an error is encountered, exit(1) is done.
*
*-*/
get_option_args(argc, argv, p_out_file)
    int     argc;		/* number of command line args */
    char   *argv[];		/* command line args */
    struct FILE_INF *p_out_file;	/* pointer to output file */
{
    int		c;		/* option letter */
    int		errflg = 0;	/* error flag for options */
    int		i;		/* temp for string length */
    extern char *optarg;	/* argument for an option */
    extern int optind;		/* index in argv of next option */

/*   -------------------------------------------------------------
*    Get any options from the command line and process them.  If the
*    output file is specified, open it; abort if can't open it.
*    -------------------------------------------------------------*/

    p_out_file->ptr = stdout;	/* default for output is stdout */
    while ((c = getopt (argc, argv, "l:t:o:")) != -1) {
     	switch(c) {
            case'l':
                left_margin = atoi(optarg);	/* -l left_margin */
            case't':
                tab_width = atoi(optarg);	/* -t tab_width */
            	break;
	    case'o':
		if ((p_out_file->ptr = fopen (optarg, "w")) == NULL) {
		    fprintf(stderr, "racPrint: can't open %s\n", optarg);
		    exit(1);
		}
		break;
            case'?':
                errflg++;
        }	
    }

    if (errflg)
	usage();

    return;
}
usage()
{
    fprintf(stderr,
	  "Usage: racPrint [-l l_mar] [-t tabwid] [-o out_file]\
input_file\n");
    exit(2);
}

/*+/subr************************************************************
* NAME	get_input_file - get next input file from command line
*
* DESCRIPTION
*	Gets the next input file argument from the command line.  If
*	an error is encountered, exit(1) is done.
*
* RETURNS
*	0 if no input file arg found
*	1 if there is an input file
*
*-*/
get_input_file(argc, argv, p_in_file)
    int     argc;		/* number of command line args */
    char   *argv[];		/* command line args */
    struct FILE_INF *p_in_file;		/* pointer to input file */
{
    char	*ctime();	/* convert 'time_t' to 'ascii' */
    char	*getenv();	/* get environment variable value */
    int		i;		/* temp for string length */
    extern char *optarg;	/* argument for an option */
    extern int optind;		/* index in argv of next option */
    struct stat p_fstat;	/* pointer to status buffer */
    char	*p_ftime;	/* pointer to date and time of last mod */

/*   -------------------------------------------------------------
*    Set up pointers and do the opens for the next input file.
*    The file descriptor for the file is also captured and stored.
*    -------------------------------------------------------------*/

    if (optind>=argc && file_no==0) {
        p_in_file->ptr = stdin;
	strcpy(p_in_file->name, "stdin");
	strcpy(p_in_file->path, " ");
	p_in_file->date_mod[0] = '\0';

	page_no=1;
	file_no++;
	strcpy(p_in_file->subr, " ");
	return(1);
    }
    else if (optind<argc) {
        if ((p_in_file->ptr = fopen(argv[optind], "r")) == NULL) {
            fprintf(stderr, "racPrint: can't open %s\n", argv[optind]);
            exit(1);
        }
        strcpy(p_in_file->name,argv[optind]);
        strcpy(p_in_file->path,getenv("PWD"));	/* get working directory */
        STRCAT_CHK(p_in_file->path,"/");	/* append /filename to it */
        STRCAT_CHK(p_in_file->path,p_in_file->name);
        p_in_file->fd = fileno(p_in_file->ptr);	/* get file descriptor */
	i = fstat(p_in_file->fd,&p_fstat);	/* get file stat info */
	p_ftime = ctime(&p_fstat.st_mtime);	/* convert to ascii \n\0 */
	p_ftime[strlen(p_ftime)-1] = '\0';	/* remove the \n */
	strcpy(p_in_file->date_mod,p_ftime);	/* date last modified */

        optind++;

	page_no=1;
	file_no++;
	strcpy(p_in_file->subr, " ");
	return(1);
    }
    else
	return(0);
}

/*+/subr************************************************************
* NAME	strcat_chk - concatenate strings, checking bounds
*
* DESCRIPTION
*	The sub_string is concatenated to the end of string.  If
*	string isn't large enough, only the part of sub_string which
*	will fit is concatenated.  Strings are \0 terminated.
*
*-*/
strcat_chk (string, size_of_string, sub_string)
    char	string[];        /* 'master' string */
    int		size_of_string;  /* declared length of string */
    char	sub_string[];    /* substring to concatenate */
{
    int		i;

    i = 0;
    while (string[i] && i<size_of_string) /* find the \0 on string */
        i++;

    if (i<size_of_string) {	/* see if there's room for more */
     	while (i<size_of_string-1 && *sub_string) /* untill full or \0 */
            string[i++] = *sub_string++;
        string[i] = '\0';	/* put in null following last char */
    }
    return;
}


/*+/subr************************************************************
* get_time_and_date - get time and date ascii string
*
* DESCRIPTION
*	The date and time are returned in the following format:
*		hh:mm Mon dd yyyy\0
*	At least 18 character positions must be available.
*
* RETURNS
*	none
*
*-*/
void
get_time_and_date(mytime)
    char	mytime[];	/* time and date as ascii string */
{
	char	*ptime;
	char	*ctime();
	long	timeofday;

	time(&timeofday);
		
	ptime = ctime(&timeofday);

	strncpy(&mytime[0],&ptime[11],5);
	mytime[5]=' ';
	strncpy(&mytime[6],&ptime[4],6);
	mytime[12]=' ';
	strncpy(&mytime[13],&ptime[20],4);
	mytime[17]='\0';

	return;	
}
