 /*	$Id$
 *	Author: Betty Ann Gunther
 *	Date:	02-15-91
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
 * .01	07-03-91	rac	changed "bw" to "lopi"
 * .02	07-03-91	rac	eliminate some gcc warnings
 */

#include <vxWorks.h> 
#include <stdioLib.h>
#include <ioLib.h>
#include <ctype.h>
#include <db_access.h>
#include <cadef.h>
#include <taskLib.h>
#include <semLib.h>
#include <strLib.h>
#include <os_depen.h>


VOID lopi_init();
VOID create_menu();
VOID get_displays(); 
struct mon_node *mon_alloc();   /* Allocates memory for an update.*/
struct txt_node *txt_alloc();  /* Allocates memory for a test label. */
struct ev_node  *ev_alloc();   /* Allocates memory for an event node. */
struct except_node *except_alloc(); /* Allocates memory for an exception node. */
struct window_node *win_alloc(); /* Allocates memory for a window structure. */
VOID init_monitors ();
VOID print_menu();  
VOID mv_cursor();
VOID get_key();
char get_esc_seq();
VOID make_box();
VOID get_value();
VOID blank_fill();
VOID mv_cursor();
char *skipblanks();
int fgetline();
VOID lopi_conn_handler();
VOID lopi_exception_handler();
VOID add_ctl();
VOID add_mon();
VOID add_txt();
char *skip_to_digit();
int int_to_short();
VOID read_disp_lst(); 
VOID display_text();
VOID display_file();
VOID display_monitors(); 
VOID stop_monitors();
VOID lopi_ev_handler();
VOID lopi_exception_handler();
VOID lopi_conn_handler();
VOID free_mem();
struct mon_node *find_channel();
VOID put_value();
int to_short();

#define FN_LEN 24
#define LIST_END -1
#define NEWLINE  '\n'
#define MXMENU  34
#define ESC '\33' 
#define L_BRACKET  '['
#define RETURN '\15'
#define CR  '\13'
#define DELETE '\177'
#define BLANK ' '
#define UNDERSC '_'
#define COLON ':'
#define TILDE '~'
#define ASTER '*'    /* Legal text character. */
#define L_ARROW '$'
#define R_ARROW '%'
#define U_ARROW '!'
#define D_ARROW  '@'
#define ILL_CHR '#'
#define F6   '^'     /* Permits exit back to main menu. */
#define F7 '&'
#define DOT '.'

/* VT220 Escape secquences */

#define CLEAR_SCREEN printf("%c", '\33');  \
                     printf("%s","[2J");   
#define INIT_CURSOR  printf("%c", '\33');    \
                     printf("%s", "[?25h"); 
#define ERASE_LINE printf("%c",ESC); \
                   printf("%s","[2K");
#define ESCAPE printf("%c",'\33');       
#define REVERSE_VIDEO  printf("%s","[7;5m");
#define ATR_OFF printf("%s","[0m");

#define CTL  1
#define MON 0
#define DEBUG 0
#define OFF  0
#define ON   1
#define NO_CMD "\0"
#define NEW_DATA 1
#define NO_NEW_DATA 0
#define NOTHING_SELECTED -1  /* Flag to indicate that lopi is waiting
                                for the user to make a selection. */
#define STRT_TTLE_ROW 1      /* Title row position. */
#define STRT_TTLE_COL 30     /* Title column position. */
#define STRT_MENU_ROW  7     /* Row position of first menu item. */
#define STRT_MENU_COL 20     /* Column position of first menu item. */
#define MAX_DISP_NUM 10      /* Maximum number of displays. */
#define MAX_LIN_LEN 80       /* Maximum number of characters in a line of a 
                                display file. */
#define BUFF_SIZE 10
#define SHELL_ID 7 
#define MAX_SCREEN_HEIGHT 24
#define MAX_SCREEN_WIDTH 78 
#define CHAN_NM_SIZE 40
#define MAX_FIELD_SIZE 8

#define ERR_MSG_ROW 20
#define ERR_MSG_COL 0
#define BOX_IN_ROW 2
#define BOX_IN_COL 43
#define BEG_BOX 1
#define END_BOX 3
#define UNDEF 8
#define FOUND 1
#define NOT_FOUND 0

shell_init() 
{
 ioctl(1,FIOSETOPTIONS,OPT_RAW);
}

/*
 *	CA_SIGNAL()
 *
 *
 */

lopi_signal(ca_status,message)
int		ca_status;
char		*message;
{
  short row = ERR_MSG_ROW; 
  short col = ERR_MSG_COL; 
  char prefix[MAX_LIN_LEN];
  char txt_str[MAX_LIN_LEN];

  static  char  *severity[] = 
		{
		"Warning",
		"Success",
		"Error",
		"Info",
		"Fatal",
		"Fatal",
		"Fatal",
		"Fatal"
		};

/*
  if( CA_EXTRACT_MSG_NO(ca_status) >= NELEMENTS(ca_message_text) ){
    message = "corrupt status";
    ca_status = ECA_INTERNAL;
  }
*/
  strcpy(txt_str, "CA.Diagnostic..........");
  mv_cursor(&row,&col,txt_str); 

  
  if(message){
    strcpy(prefix,"Severity:[%s] Error:[%s]Context: [%s]"); 
    sprintf(txt_str,prefix,severity[CA_EXTRACT_SEVERITY(ca_status)],
         ca_message_text[CA_EXTRACT_MSG_NO(ca_status)],message);
    mv_cursor(&row,&col,txt_str); 
  }
  else{
    strcpy(prefix,"Severity:[%s],Error:[%s]");
    sprintf(txt_str,prefix,severity[CA_EXTRACT_SEVERITY(ca_status)],
         ca_message_text[CA_EXTRACT_MSG_NO(ca_status)]);
    mv_cursor(&row,&col,txt_str); 
  }
  /*
   *
   *
   *	Terminate execution if unsuccessful
   *
   *
   */
  if( !(ca_status & CA_M_SUCCESS) ){
#   ifdef VMS
      lib$signal(0);
#   endif
#   ifdef vxWorks
      ti(VXTHISTASKID);
      tt(VXTHISTASKID);
#   endif
  /*  abort(ca_status); */
  }

}

