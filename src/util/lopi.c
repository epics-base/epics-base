 /*	@(#)lopi_def.h	1.4	2/19/93
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
 * .01	02-26-91         bg     Changed cursor to move only to controls.
 * .02	07-03-91	rac	changed to use "lopi" rather than "bw"
 * .03	08-14-91	 bg	Added error message for no file available
                                for a display. 
 * .04	08-26-91	 bg	Fixed cursor so it will not try to move
                                with arrow keys if a screen has no controls.
 * .05	09-12-91	 bg	Added semDelete for both keyboard and monitor
                                semaphores.
 * .06	10-2-91		 bg	Fixed bug in display_monitors.               
 * .07	12-12-91	 bg	Fixed lopi so if you try to write to a monitor
 * 	                        you will not crash.
 * .08  6-10-91          bg     Fixed allocation subroutines so they return NULL
 *                              if they cannot get enough memory.  
 * .09	07-03-91	rac	changed "bw" to "lopi"
 * .10	08-11-93	mrk	removed ifdef V5_vxWorks
 */

#include <vxWorks.h>
#include <stdioLib.h> 
#include <ioLib.h> 
#include <taskLib.h>
#include <semLib.h> 
#include <strLib.h> 
#include <os_depen.h> 
#include <ctype.h>
#include <db_access.h>
#include <cadef.h>


static VOID lopi_init();
static VOID create_menu();
static VOID get_displays(); 
static struct mon_node *mon_alloc();   /* Allocates memory for an update.*/
static struct txt_node *txt_alloc();  /* Allocates memory for a test label. */
static struct ev_node  *ev_alloc();   /* Allocates memory for an event node. */
static struct except_node *except_alloc(); /* Allocates memory for an exception node. */
static struct window_node *win_alloc(); /* Allocates memory for a window structure. */
static VOID init_monitors ();
static VOID print_menu();  
static VOID mv_cursor();
static VOID get_key();
static char get_esc_seq();
static VOID make_box();
static VOID get_value();
static VOID blank_fill();
static VOID mv_cursor();
static char *skipblanks();
static int getline();
static VOID lopi_conn_handler();
static VOID lopi_exception_handler();
static VOID add_ctl();
static VOID add_mon();
static VOID add_txt();
static char *skip_to_digit();
static int int_to_short();
static VOID read_disp_lst(); 
static VOID display_text();
static VOID display_file();
static VOID display_monitors(); 
static VOID stop_monitors();
static VOID lopi_ev_handler();
static VOID lopi_exception_handler();
static VOID lopi_conn_handler();
static VOID free_mem();
static struct mon_node *find_channel();
static VOID put_value();
static int to_short();

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
#define MINUS '-'
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

#define CLEAR_SCREEN fdprintf(lopi_fd,"%c", '\33');  \
                     fdprintf(lopi_fd,"%s","[2J");   
#define INIT_CURSOR  fdprintf(lopi_fd,"%c", '\33');    \
                     fdprintf(lopi_fd,"%s", "[?25h"); 
#define ERASE_LINE fdprintf(lopi_fd,"%c",ESC); \
                   fdprintf(lopi_fd,"%s","[2K");
#define ESCAPE fdprintf(lopi_fd,"%c",'\33');       
#define REVERSE_VIDEO  fdprintf(lopi_fd,"%s","[7;5m");
#define ATR_OFF fdprintf(lopi_fd,"%s","[0m");

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


struct except_node{
       long status;             /* Channel access standard status code.             */
       char msg[MAX_LIN_LEN]; /* Character string containing context information. */
       short flg;             /* Flag to indicate the arrival of an exception.    */
       };

struct ev_node{
        short data_flg;     /* Flag to indicate that new data has been added.     */
        short status ;      /* Status returned from function calls.               */
        short severity; 
        char str[MAX_STRING_SIZE];
        short prev_size;   /*  Size of string.  */
        };


struct mon_node{          /* Structure to hold update information.               */
        short l_crn_row;      /* Row location of upper left hand corner.         */
        short l_crn_col;      /* Col location of upper left hand corner.         */
        short r_crn_row;         /* Row location of lower right hand corner.     */
        short r_crn_col;         /* Col location of lower right hand corner.     */
        char chan[CHAN_NM_SIZE];   /* Channel name.                              */  
        chid chan_id;                 /* Channel id for channel access.          */
        short chan_type;
        short prev_size;
        struct ev_node *ev_ptr;    /* Pointer to event handler info.             */
        short conn_flg;          /* Pointer to connection handler info.          */
        struct mon_node *next;   /* Pointer to next node in the linked list.     */
        struct mon_node *prev;   /* Pointer to previous node in the linked list. */
        };

struct txt_node{          /* Structure to hold text label information.          */ 
        short l_crn_row;      /* Row location of upper left hand corner.        */
        short l_crn_col;      /* Col location of upper left hand corner.        */
        char txt_str[MAX_LIN_LEN];  /* Text string to be displayed.            */
        struct txt_node *next;      /* Pointer to next node.                    */
        };

struct window_node{      /* Structure which will hold all window information. There 
                            will be a linked list of each kind of structure possible.
                            This structure will contain a pointer to the first node on
                            that list. */
       struct mon_node *c_head;
       struct mon_node *m_head;
       struct txt_node *t_head;
       }; 

static SEM_ID mon_sem,key_sem;      /* Semaphores for protection of monitor and control   
                                 linked lists, the key board and screen, and the
                                 exception handler's structure                   */

static struct window_node *wind_array[MAX_DISP_NUM];  /* Array of windows available. */

static char disp_lst[FN_LEN] = "lopi.dat"; /* Array for name of file containing list of display files. */
static char *d_menu[MXMENU];
static short nxt_ln = 0;   /* Counter. */            
static short data_flg;     /* Global Flag to indicate a monitor has received data. Set in event handler.
                       Reset in display_monitors when data has been displayed.  */
static short m_row;        /* Current row number . */
static short m_col;        /* Current column number. */
static int n_lines = 0;    /* Number of Menu lines read in. */
    
static int tid1,tid2;      /* Task id's */
static short screen = OFF;
static short init = TRUE;  /* Flag which indicates first pass through the menu. */
static char key_buff[BUFF_SIZE],*rd_ptr,*wr_ptr;  /* Buffer which will accumulate keyboard input while
                              waiting for a chance to update the screen. Pointers showing where
                              next available spot is for reading and for writing. */
static char val_in[MAX_STRING_SIZE]; /* Global buffer for user input value to be written to data base. */
static short q_num;        /* Number of items waiting to be output from input queue. */  
static int status; 

static int err_fd;         /* Fd to which logMsg will write during the course of this
                     subroutine. */ 
static int lopi_fd;          /* File descriptor for serial port. */
struct except_node *except_ptr;
static int num_times = 0;;
extern shellTaskId;


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

VOID lopi() 

{

  /* Create and initialize semaphores protecting the keyboard and the 
     monitor linked list. */ 
  mon_sem = semBCreate(SEM_Q_PRIORITY, SEM_FULL);
  key_sem = semBCreate(SEM_Q_PRIORITY, SEM_FULL);
  data_flg = NO_NEW_DATA;;
  except_ptr  = except_alloc();

  /* Initialize buffer for keyboard input. */

  blank_fill(key_buff,BUFF_SIZE);

  /* Open file in which errrors will be recorded. */
  SEVCHK(ca_task_initialize(),"Unable to initialize.");

  q_num = 0;

  tid1 = taskSpawn("lopi_init",210,VX_STDIO||VX_FP_TASK,20000,lopi_init,&screen,&q_num,&data_flg,
                    key_buff,val_in);
  tid2 = taskSpawn("get_key",208,VX_STDIO||VX_FP_TASK,5000,get_key,&q_num,
                    key_buff,&screen,val_in);

  /* Suspend shell to allow lopi the use of the keyboard. */
  taskSuspend(shellTaskId);
  }
 
/**********************************************************************************/
/*    Entry point for local terminal. Reads a file called lopi.dat which contains a */
/* list of display files.  Reads in the display names and the display files and   */
/* makes a menu from which the user can select the display he wants to see.       */
/*    Display information is kept in an array of window structures.  Displays the */
/* selected display.
/**********************************************************************************/

static VOID lopi_init(screen,que_num,pdata_flg,k_buff,val_in)
    short *screen;
    short *que_num; /* Number of items in keyboard buffer waiting to be read. */
    short *pdata_flg;  /* Flag to indicate that monitor had received new data. */
    char k_buff[BUFF_SIZE];  /* Buffer for key board entries and pointer
                                to the next empty space in the buffer */
    char val_in[MAX_STRING_SIZE];
   {
    char *read_ptr, *write_ptr;  /* Pointers to next available places for reading and
                                    writing in the keyboard input buffer. */
    short selected;
    char prefix[MAX_LIN_LEN];
    char txt_str[MAX_LIN_LEN]; 
    register short display = ON;
    int disp_nm[FN_LEN];
    short t_row, t_col = 0; 
    int new_fd;  /* Fd to which standard error is now writing.  This is the fd
                    to which logMsg will be reset as this subroutine terminates. */

    read_ptr = k_buff;
    write_ptr = k_buff;
    err_fd = creat("lopi_err",0666);
    if((err_fd = open("lopi_err",UPDATE,0666)) == ERROR){
       printf("Aborting. Unable to open error file.\n");
       taskResume(shellTaskId);
       new_fd = ioGlobalStdGet(0,STD_ERR); 
       ca_task_exit(); 
       semDelete(key_sem);
       semDelete(mon_sem);
       taskDelete(tid2); 
       taskDelete(tid1); 
       exit(1);
    }



    /* Build menu a from file called lopi.dat. Count the lines read in.  This 
       is the number of display files read in. */

   create_menu(d_menu,&n_lines); 
   get_displays(d_menu,n_lines,wind_array);  

      /* Gives user a chance to read error messages generated by get_displays. */


    /* Open a file descriptor for the serial terminal.
       Set it to raw mode. */
    if ((lopi_fd = open("/tyCo/0",UPDATE,0)) == ERROR){
       printf("Aborting. Unable to open serial port file.\n");
       taskResume(shellTaskId);
       ca_task_exit(); 
       status = ioctl(lopi_fd,FIOSETOPTIONS,OPT_TERMINAL);
       new_fd = ioGlobalStdGet(0,STD_ERR); 
       semDelete(key_sem);
       semDelete(mon_sem); 
       taskDelete(tid2); 
       taskDelete(tid1); 
       exit(1);
    }

    logFdSet(err_fd);

    if ((status = ioctl(lopi_fd,FIOSETOPTIONS,OPT_RAW)) == ERROR){
       printf("Aborting. Unable to set I/O mode.\n");
       taskResume(shellTaskId);
       ca_task_exit(); 
       new_fd = ioGlobalStdGet(0,STD_ERR); 
       status = ioctl(lopi_fd,FIOSETOPTIONS,OPT_TERMINAL);
       semTake(key_sem);
       semGive(key_sem);
       semDelete(key_sem);
       semDelete(mon_sem);
       taskDelete(tid2); 
       taskDelete(tid1); 
       exit(1);
    }

    CLEAR_SCREEN
    INIT_CURSOR  
    selected = NOTHING_SELECTED;
  
    while (display == ON) 
      { 
       if (n_lines == 0)
         printf("No displays to open");
       else if (n_lines == 1)
         {
          selected = 0;
          init = FALSE;
          *screen = ON;
          CLEAR_SCREEN   
          display_file(wind_array,selected,screen,que_num,pdata_flg,k_buff,val_in);
          selected = NOTHING_SELECTED;
         }
       
       else if (((*screen) == OFF) && (n_lines >1))
         {  

          /* Task delay allows display_monitors to finish writing
             to screen before clearing the screen and printing the 
             menu. */ 
         
         if (!init)
             taskDelay(100);
          CLEAR_SCREEN   
          print_menu(d_menu,n_lines);  
          m_row = STRT_MENU_ROW; 
          m_col = STRT_MENU_COL-1; 
          *que_num = 0;
          write_ptr = k_buff;

          /* Move cursor upper left hand corner of menu. */
          mv_cursor(&m_row,&m_col,NO_CMD);

        } 

       /* Get cursor movement  from the screen. */

       /* Move cursor only along edge of menu while menu is
          being displayed.  Semaphores are not necessary
          no other task is  competing for screen or linked lists at 
          this time. */

     while ((selected) == NOTHING_SELECTED && (n_lines > 1) )
         {  

          while ((*que_num) > 0) 
            {
             switch (*write_ptr)
               {
                case U_ARROW:
                  m_row++;
                if (m_row > n_lines + STRT_MENU_ROW )
                   m_row = STRT_MENU_ROW;
                 mv_cursor(&m_row,&m_col,NO_CMD); 
                 break;
              case D_ARROW:
                m_row--;
                if ( m_row < STRT_MENU_ROW) 
                   m_row =  n_lines + STRT_MENU_ROW;
                mv_cursor(&m_row,&m_col,NO_CMD); 
                break;
              case RETURN:
                selected = m_row - STRT_MENU_ROW;
                t_row++; 
                break;

              default: 
                break;

             } /* End switch. */
            
             /* If a legal restart occurs reset buffer pointer
                back to the first space in the buffer. and data to be
                processed back to 0.*/

             if ((*write_ptr == F6) || (*write_ptr == RETURN) || (*write_ptr == F7))
               {
                semTake(key_sem);
                (*que_num) = 0;
                 write_ptr = k_buff;
                semGive(key_sem);
               }
             else
               {
                semTake(key_sem);
                write_ptr++;
                (*que_num)--;
                if(*write_ptr == NULL) 
                   write_ptr = k_buff;
                semGive(key_sem);
               }
           } /*end while *que_num > 0 */

    
   
        } /*  End while selected == NOTHING_SELECTED . */  
        

       /* User has chosen to exit the menu and the program. */

       if (( selected == n_lines) || (n_lines == 1) )   
         {
          display = OFF;
          CLEAR_SCREEN   
          status = ioctl(lopi_fd,FIOSETOPTIONS,OPT_TERMINAL);
          taskResume(shellTaskId);
         } 
       
       else if ((selected >= 0) && (selected < n_lines))
         {
          init = FALSE;
          *screen = ON;
          CLEAR_SCREEN   
          display_file(wind_array,selected,screen,que_num,pdata_flg,k_buff,val_in);
          selected = NOTHING_SELECTED;
          semTake(key_sem);
          *screen = OFF;
          semGive(key_sem);
         }  
      
     }  /* End while display. */
    
    /* Reset terminal to terminal mode. */

    close(err_fd);
    new_fd = ioGlobalStdGet(0,STD_ERR); 
    logFdSet(new_fd); 
    semTake(key_sem);
    taskDelete(tid2); 
    taskDelete(tid1); 
    ca_task_exit(); 
    semGive(key_sem);
    taskDelay(100);
    free_mem(wind_array,d_menu,n_lines);  
    semDelete(key_sem);
    semDelete(mon_sem);
    close(lopi_fd); 
   
   }  /* End lopi. */

/******************************************************************************************/
/* Uses NFS to read in a list of displays kept in a file called lopi.dat in the application */
/*      directory.                                                                        */   
/******************************************************************************************/

static VOID create_menu(menu,nlines)
   char *menu[MXMENU];
   int *nlines;
   {      
     FILE *fp;
     int ltr;
     char item[40],*m_ptr; 
     short chr_ctr = 0;  /* Counter. */

     /* Open file. Return error message if that is impossible. */

     if ((fp = fopen(disp_lst, "r")) == NULL) 
       {
        printf("Can't open %s \n", disp_lst);
        exit(1);
       } 
           
     /* Otherwise create a menu of displays for this application. */

     else
       { 
                
        *nlines = 0; 

        /* Read in characters from display file one by one.*/
        while ((ltr = getc(fp)) != EOF)  
          {
           item[chr_ctr] = ltr;
           chr_ctr++; 
          
           /* At the end of each line, enter the display title into the display
              memory array. */
         
           if ((ltr == NEWLINE) && (chr_ctr != 0))
             {
              item[chr_ctr-1] = NULL;
              if (( m_ptr = (char *) malloc(chr_ctr + 1)) == NULL)
                {
                 printf("Not enough memory in create_menu\n");
                 exit(1);
                }
                 
              if (*nlines <= MXMENU) 
                {
                 strcpy(m_ptr,item);
                 d_menu[nxt_ln++] = m_ptr;
                 (*nlines)++; 
                 chr_ctr = 0; /* Reset the character counter. */
                }
              else    /* Error message for too many display names. */
                 printf("Maximum number of displays exceeded.");
             }
          } /* End while. */
         } /* End else. */ 
                 
     fclose(fp);  /* Close the file. */
   } /* End create_menu. */


   /************************************************************************/
   /*  Prints to the screen the menu passed to it in the array menu as     */
   /*  well as the instructions. Positions cursor in front of the first    */
   /*  menu item.                                                          */ 
   /************************************************************************/

   static VOID print_menu(menu,nlines)
      char *menu[MXMENU];      /* Array of menu items.                     */
      int nlines;              /* Number of items in the array.            */
      {
        short mrow;              
        short mcol;
        short b_ctr;
        mrow = STRT_TTLE_ROW; 
        mcol = STRT_TTLE_COL;
        mv_cursor(&mrow,&mcol,"DISPLAY MENU");
        mrow++;
        mcol -= 3;
        mv_cursor(&mrow,&mcol,NO_CMD);
        mrow++;
        mv_cursor(&mrow,&mcol,"Move cursor with arrow keys.");
        mrow++;
        mv_cursor(&mrow,&mcol,"Select with carriage return.");
        mrow = STRT_MENU_ROW;
        mcol = STRT_MENU_COL;  
        for (b_ctr = 0; b_ctr < nlines; b_ctr++)
          {
           mv_cursor(&mrow,&mcol,menu[b_ctr]);
           mrow++;
          }
        mv_cursor (&mrow,&mcol,"EXIT");
        mrow++;
        mrow = STRT_MENU_ROW;
        mcol = STRT_MENU_COL - 1; 
      } 


/***********************************************************************************/
/*    Gets keystrokes from the screen,translates them to lopi opi commands and     */
/* puts them in a buffer which is read by lopi, display_file and display_monitors. */
/* Reads up arrow,down arrow,right arrow, left arrow, F6, F7 and carriage          */
/* return. When it reads an escape it calls get_esc_seq to complete the job of     */
/* getting the escape sequence from the screen and translating it into a lopi      */
/* command.                                                                        */
/***********************************************************************************/

static VOID get_key(cue_num,k_buff,pScreen,val_in)

    short *cue_num;  /* Contains the number of letters in the queue ends the message 
                        to getkey that the buffer has been read. */
    char k_buff[BUFF_SIZE];
    short *pScreen;
    char val_in[MAX_STRING_SIZE];
    {
     char *in_ptr; /* Pointer to next place for in put and next place
                               for output. */
     short ctr;   /* Counter. */
     char ltr;
     int nmbr;

     int mon_row = 0;  /* For debug only. */ 
     int mon_col = 0;
     int t_row = 0;  /* For debug only. */ 
     int t_col = 0;
     char txt_str[MAX_LIN_LEN];
     char prefix[MAX_LIN_LEN];

     semTake(key_sem);
       *pScreen = OFF;
       in_ptr = k_buff;
       *cue_num = 0;
     semGive(key_sem);
       
     FOREVER
       {

        /* Read  characters from keyboard and put them in a buffer until
           the buffer has been read.If an escape is reached continue to read
           input until one of the supported escape sequences has been completed.
           Convert those keystrokes to a code and return it. */
       nmbr = read(lopi_fd,&ltr,1);

       do
          {
            /* If the letter is an escape sequence, do more reads to get
            the rest of the sequence. Then convert it to a single character
            and put it in the buffer. */
    
            if(ltr ==  ESC) 
                {
                 semTake(key_sem); 
                 ltr =  get_esc_seq();
                 semGive(key_sem); 
                   
                 switch(ltr)
                   {
                    case U_ARROW:
                       semTake(key_sem); 
                       (*in_ptr) =  U_ARROW;
                       (*cue_num)++; 
                       semGive(key_sem); 
                       in_ptr++;
                       break;
                    
                   case D_ARROW:
                      semTake(key_sem); 
                      (*in_ptr) =  D_ARROW;
                      (*cue_num)++; 
                      semGive(key_sem); 
                      in_ptr++;
                      break;

                    case F6:
                       semTake(key_sem);
                       (*in_ptr) = F6;
                        *pScreen = OFF;
                        in_ptr = k_buff; 
                        semGive(key_sem); 
                        break;
      
                    case F7:
                       if (*pScreen == ON)
                         {
                          make_box();  
                          get_value(val_in);
                          taskDelay(30);
                         }
                       semTake(key_sem); 
                       (*in_ptr) =  F7;
                       (*cue_num)++; 
                       in_ptr = k_buff;
                       semGive(key_sem); 
                       ltr = NULL;
                       break;
                    default:
                       break;

                 } /* End switch. */
                } /* End if ltr ==  ESC. */
       if ((ltr == RETURN) || (ltr == CR)) 
         {
          semTake(key_sem); 
          (*in_ptr) =  RETURN;
          (*cue_num)++; 
          semGive(key_sem); 
          in_ptr = k_buff; 
         }

       if ((*in_ptr) == NULL) 
         in_ptr = k_buff; 
       nmbr = read(lopi_fd,&ltr,1); 
       taskDelay(1);
      }  /* End do while. */
     while ((*cue_num) <= (BUFF_SIZE -1) );  /* Exit this loop if buffer is filled. */ 

       
    }  /* End FOREVER */

 } /* End get_key. */


/********************************************************************************/
/* Gets escape sequences from screen and returns their meaning to get key.      */
/* Returns up arrow, down arrow, right arrow, left arrow, F6 and F7.  For       */
/* all other letters returns ILL_CHR .                                          */
/********************************************************************************/

static char get_esc_seq()
   {
     char lttr;
     int nbr;

     nbr = read(lopi_fd,&lttr,1);
     if (lttr ==L_BRACKET)
       {
        nbr = read(lopi_fd,&lttr,1); 
        switch(lttr)
          {
           case 'A':
                    return  D_ARROW;
           case 'B':
                    return U_ARROW;
           case '1':
                    nbr = read(lopi_fd,&lttr,1); 
                    if (lttr == '7')
                      {
                       nbr = read(lopi_fd,&lttr,1); 
                       if (lttr == TILDE)
                          return  F6;
                      }
                    if (lttr == '8')  
                      {
                       nbr = read(lopi_fd,&lttr,1); 
                       if (lttr == TILDE)
                          return F7;
                      }  
           default:
                    /* If an illegal escape sequence has been read
                    then the legal character flag will not be set
                    and buffer write pointer will not be advanced. */

                    return ILL_CHR;
          } /*  End switch */

        } /* End if ltter == L_BRACKET. */
     else
       return ILL_CHR;
   } /* End get_esc_seq. */

/*****************************************************************************/
/* Draws a box on the screen for user to enter the value he wishes to write. */
/*****************************************************************************/
static VOID make_box()
  {
   char str[MAX_STRING_SIZE];
   short row = BEG_BOX; 
   short col = BEG_BOX;

   strcpy(str, "***************************************");
   mv_cursor(&row,&col,str);
   row+=2;
   strcpy(str, "***************************************");
   row = END_BOX;
   mv_cursor(&row,&col,str);
   row--;
   col = BOX_IN_COL;
   mv_cursor(&row,&col,NO_CMD);
  }

/************************************************************************/
/*     Gets value from user input from screen. Echos user's input back. */
/* A null in the first location in the array indicates no value was     */
/* entered.                                                             */
/************************************************************************/

static VOID get_value(val_in)
    char  val_in[MAX_STRING_SIZE];  /* Array into which letters will be placed as they
                               are read in from screen. */
   {
    char numb, *chr_ptr;    /* Variable into which ltrs are read before putting 
                              them into field_array. */
    VOID mv_cursor(); 

    short row = BOX_IN_ROW;
    short col = BEG_BOX;
    int nmbr;
    short ctr; 
    char prefix[40];
    char txt_str[MAX_STRING_SIZE];

    blank_fill(val_in,MAX_STRING_SIZE);
    strcpy(val_in, "* Enter value:                        *");
    mv_cursor(&row,&col,val_in);     
    col = 16;
    blank_fill(val_in,MAX_STRING_SIZE);
    mv_cursor(&row,&col,NO_CMD);     

    /* Initialize pointer to first location in array. */

    chr_ptr = val_in;

    /* Read input from screen until a return is entered. */
    nmbr = read(lopi_fd,&numb,1);

    while ((numb != RETURN) && (numb != CR) && (*chr_ptr != NULL ))
      {
       /* If value is legal enter it in value array. */ 

       if (( isalpha(numb)) || (isdigit(numb)) || (numb == DOT) || (numb == MINUS )) 
         {
          *chr_ptr = numb; 
          chr_ptr++;
          mv_cursor(&row,&col,val_in);     
          nmbr = read(lopi_fd,&numb,1);
         } /* End if isalpha */
       else if ((numb == DELETE) && (chr_ptr != val_in)) 
         {
          chr_ptr--;
          *chr_ptr = BLANK;
          mv_cursor(&row,&col,val_in);
          nmbr = read(lopi_fd,&numb,1);
         }
       else if ((numb != RETURN)  || (numb != CR))
         {
          nmbr = read(lopi_fd,&numb,1);
         }
         
      }  /* End while numb != return. */

    *chr_ptr =  NULL;
    col = BEG_BOX; 

    /* Erase the box and data. */ 

    for (ctr = BEG_BOX;  ctr <= END_BOX; ctr++)              
      {
       row = ctr;                     
       mv_cursor(&row,&col,NO_CMD);   
       ERASE_LINE;
      }  
   }

/**************************************************************************************/ 
/*  Takes the character array passed to it and fills it with blanks and terminates it */
/* with a NULL.                                                                       */
/**************************************************************************************/ 
 
static VOID blank_fill(chr_array,chr_size)
   char chr_array[MAX_LIN_LEN];
   short chr_size;
  {
    short c_ctr = 0; /* Index  to array being filled. */ 
   
   while(c_ctr < (chr_size - 1))
      chr_array[c_ctr++] = BLANK;
   chr_array[chr_size -1] = NULL;
   
  } 

/****************************************************************************/ 
/* Moves cursor to row and column passed to it.  Prints a string after the  */
/* cursor if one is sent to it.                                             */
/****************************************************************************/


static VOID mv_cursor(mrow,mcol,str)
  short *mrow;
  short *mcol;
  char str[100]; /* String to be printed after cursor has a been
            moved. */
  {
    char command[140];

    /* Create a string containing the proper VT220 commands
       to move the cursor to the location desired. */

    bzero(command,sizeof(command));
    sprintf(command,"[%d;%dH %s",*mrow,*mcol,str);  

    /* Send command to the terminal. */

    semTake(mon_sem);

    fdprintf(lopi_fd,"%c",ESC); 
    fdprintf(lopi_fd,"%s",command);

    semGive(mon_sem);
  }


 /****************************************************************************/
 /* Strips away blanks from the string to which it                           */
 /* points.  Returns a pointer to the first non-blank character.             */
 /****************************************************************************/


static char *skipblanks(str)
  char *str;
   {

    while ( (*str == BLANK)) 
        str++;

    return (str); 
   }


/****************************************************************************/
/*     Reads in a line from a file and puts it in an array.                 */
/* Strips off newline character. Terminates string with a NULL.             */
/* Returns the number of characters read in or EOF.                         */ 
/****************************************************************************/


static int fgetline(disp_ln,d_fp)

    FILE *d_fp;    /* File pointer to the file being read in. */
    char disp_ln[MAX_LIN_LEN];
   {
    int c;
    char *lnptr;

    for (lnptr = disp_ln; lnptr - disp_ln < MAX_LIN_LEN - 1 && 
         (c = getc(d_fp)) != EOF && c != NEWLINE; lnptr++)
       *lnptr = (char) c;
 
    *lnptr = NULL;

    if ( c == EOF)
       return EOF;
    else 
       return lnptr - disp_ln;
   }


/****************************************************************************/
/*   Reads in the files listed in disp_files one by one.  It builds a       */
/* structure called a window for each display file it reads in.  It builds  */
/* linked lists of controls, monitors and texts.  These lists will later be */
/* used to display the dynamic information as it comes in.                  */
/****************************************************************************/


 static VOID get_displays(disp_files,nfiles,disp_array)

  char *disp_files[FN_LEN];  /* Array for list  of filenames to be selected by the user. */
  int nfiles;              /* Number of files read in. */
  struct window_node *disp_array[MAX_DISP_NUM];  /* Array of windows available. */
  
  {
   FILE *disp_fp;
   char *lptr,dline[MAX_LIN_LEN];  /* Array to hold the contents of a line from the
                                      display file as it is read in. */
   struct mon_node *ctl_ptr;  
   int flg;       /* Flag for EOF. */
   int disp_ctr;  /* Counters. */
   for (disp_ctr = 0; disp_ctr < nfiles; disp_ctr++) 
     { 
      if ((disp_fp = fopen(disp_files[disp_ctr], "r")) == NULL){
         printf("Can't open %s \n", disp_files[disp_ctr]);
         disp_array[disp_ctr] = NULL;
      }
      else
        {
         disp_array[disp_ctr] = win_alloc();
         disp_array[disp_ctr]->c_head = NULL;
         disp_array[disp_ctr]->m_head = NULL;
         disp_array[disp_ctr]->t_head = NULL;
         flg = 0;
         do
           {
            flg = fgetline(dline,disp_fp); 
            lptr = skipblanks(dline);  /* Skip any leading blanks if any. */
            switch (*lptr)
              {
                case 'c':
                    add_ctl(disp_array,disp_ctr,lptr); 
                  break;

                case 'm':
                    add_mon(disp_array,disp_ctr,lptr); 
                   break;

                case 't':
                    add_txt(disp_array,disp_ctr,lptr);
                   break;

                default:
                   break;
              } /* End switch. */
            bzero(dline,sizeof(dline));

           } while(flg != EOF); 

          status = fclose(disp_fp);

        } /*  End else. */

     }  /* End for.*/ 

     /* it(DEBUG)
        read_disp_lst(wind_array,n_lines); */     

  }         /* End get_displays */



/****************************************************************************/
/* Allocates memory for a monitor.                                          */
/****************************************************************************/

static struct mon_node *mon_alloc() 
  {
   struct mon_node *monptr;

     if (( monptr = (struct mon_node *)malloc(sizeof(struct mon_node))) == NULL)
       {
        logMsg("Insufficient memory in mon_alloc.\n ");
        close(err_fd);
        abort();
        return NULL;
       }
     else 
       return monptr;
  }

/****************************************************************************/
/* Allocates memory for a text label.                                       */
/****************************************************************************/

static struct txt_node *txt_alloc() 
  {
   struct txt_node *txtptr;

     if (( txtptr = (struct txt_node *)malloc(sizeof(struct txt_node))) == NULL)
       {
        logMsg("Insufficient memory in tx_alloc.\n ");
        close(err_fd);
        abort();
        return NULL;
       }
     else 
       return txtptr;
  }

/****************************************************************************/
/* Allocates memory for an event node.                                      */
/****************************************************************************/

static struct ev_node *ev_alloc() 
  {
   struct ev_node *evptr;

     if (( evptr = (struct ev_node *)malloc(sizeof(struct ev_node))) == NULL)
       {
        logMsg("Insufficient memory in ev_alloc.\n ");
        close(err_fd);
        abort();
        return NULL;
       }
     else 
       return evptr;
  }

/****************************************************************************/
/* Allocates memory for an exception node.                                  */
/****************************************************************************/

static struct except_node *except_alloc() 
  {
   struct except_node *ex_ptr;

     if (( ex_ptr = (struct except_node *)malloc(sizeof(struct except_node))) == NULL)
       {
        logMsg("Insufficient memory in except_alloc.\n ");
        close(err_fd);
        abort();
        return NULL;
       }
     else 
       return ex_ptr;
  }

/****************************************************************************/
/* Allocates memory for a window structure.                                 */
/****************************************************************************/

static struct window_node *win_alloc()
  {
   struct window_node *winptr;

     if (( winptr = (struct window_node *)malloc(sizeof(struct window_node))) == NULL)
       {
        logMsg("Insufficient memory in win_alloc.\n ");
        close(err_fd);
        abort();
        return NULL;
       }
     else 
       return winptr;
  }

/*****************************************************************************/
/* This function attaches a control node to the linked list headed by        */
/*  win_array->ctl. It returns the pointer to the head of the list which now */
/*  points to the new node.                                                  */
/*****************************************************************************/

static void add_ctl(win_array,win_ctr,c_line)
    struct window_node *win_array[MAX_DISP_NUM]; 
    short win_ctr; /* Counters. */
    char *c_line;
  {
   struct mon_node *c_ptr,*new_ptr; 
   char chan_nm[CHAN_NM_SIZE],*chan_ptr; /* Array to hold channel name as it is parsed 
                                            from line and a pointer to it. */ 
   register short l_crn_row = 0,l_crn_col = 0;
   register short r_crn_row = 0,r_crn_col = 0;
   short r_loc_flg = OFF;  /* Flag which is set when location is found in element file. */
   short c_loc_flg = OFF;  /* Flag which is set when location is found in element file. */
   
   chan_ptr = chan_nm;
   bzero(chan_nm,sizeof(chan_nm));

   *chan_ptr =  NULL;

   /* Move past any characters at beginning of display element. */

   while (isalpha(*c_line))
     c_line++;      
   c_line = skip_to_digit(c_line);

    /* If there is no channel location, omit the channel from the linked list. */

   if ((*c_line) && (isdigit(*c_line)))
     {
      l_crn_row = to_short(c_line);
      while(isdigit(*c_line))
        c_line++;
      r_loc_flg =  ON;
      c_line = skip_to_digit(c_line);

      if (*c_line)
        {
         l_crn_col = to_short(c_line);
         c_loc_flg = ON;
         while  (isdigit(*c_line) )
           c_line++;
         }
     }

   /* Advance pointer past the left corner location in element array. */

   while  ((*c_line)&& (isdigit(*c_line)))
      c_line++;

   /* Advance pointer to next digit or alpha character. */

   while ((*c_line) && (!isdigit(*c_line)) && (!isalpha(*c_line)))
      c_line ++;

   /* Return a pointer to the next non-blank character in the element array. */ 

   if ((*c_line) && (isdigit(*c_line)))
     {
      r_crn_row =  to_short(c_line);
      while(isdigit(*c_line))
          c_line++;
      c_line = skip_to_digit(c_line);
      r_crn_col =  to_short(c_line);
      while ((!isalpha(*c_line)) && (*c_line))
         c_line++;
      }

   /*  If there is no channel name give appropriate error message. */


   if (!(*c_line))
      printf("No channel name for control. Channel omitted.\n");

   /* Otherwise copy channel name to the channel name array. */
   if (isalpha(*c_line))
   {
       while( (isalpha(*c_line)) || (isdigit(*c_line)) || (*c_line == UNDERSC) ||(*c_line == COLON)
              || (*c_line == DOT))
        *chan_ptr++ = *c_line++;   
       *chan_ptr = NULL;
     }

   /* Allocate and initialize header for doubly linked list. Initialize header's
      l_crn_row field to -1. */

   if (win_array[win_ctr]->c_head == NULL)
    {
     win_array[win_ctr]->c_head = mon_alloc(); 
     win_array[win_ctr]->c_head->next = win_array[win_ctr]->c_head; 
     win_array[win_ctr]->c_head->prev = win_array[win_ctr]->c_head;
     strcpy(win_array[win_ctr]->c_head->chan,"head");
     win_array[win_ctr]->c_head->l_crn_row = -1; 
     c_ptr = win_array[win_ctr]->c_head;  
    }
   
   
     /* If the channel has a name and locations for the left corner and the right
        corner, add the node immediately after the head of the list. */

    c_ptr = win_array[win_ctr]->c_head->next;
   
   if ((*chan_nm)  && (r_loc_flg) && (c_loc_flg))
      {
       while((c_ptr->next->l_crn_row != -1) && (c_ptr->l_crn_row < l_crn_row))
         {
          c_ptr = c_ptr->next;
         }
       c_ptr = win_array[win_ctr]->c_head->next;
       while((c_ptr->next->l_crn_row != -1) && (c_ptr->l_crn_row == l_crn_row) && 
            (c_ptr->l_crn_col < l_crn_col)) 
         {   
          c_ptr = c_ptr->next;
         }
       new_ptr = mon_alloc(); 
       new_ptr->next= c_ptr->next; /* Set pointer to the head to point at the new node. */
       new_ptr->prev = c_ptr;
       c_ptr->next = new_ptr;
       new_ptr->next->prev = new_ptr;
       strcpy(new_ptr->chan,chan_nm);
       new_ptr->l_crn_row = l_crn_row; 
       new_ptr->l_crn_col = l_crn_col; 
       new_ptr->r_crn_row = r_crn_row; 
       new_ptr->r_crn_col = r_crn_col; 
      }
      
   return;

  }

/************************************************************************************/
/*   This function attaches a monitor node to the linked list headed by win_array-> */
/* mon. It returns the pointer to the head of the list which now points to the new  */
/* node.                                                                            */
/************************************************************************************/

static VOID add_mon(win_array,win_ctr,m_line)
    struct window_node *win_array[MAX_DISP_NUM]; 
    short win_ctr; /* Counters. */
    char *m_line;
  {
   struct mon_node *node_ptr; 
   char chan_nm[CHAN_NM_SIZE],*chan_ptr; /* Array to hold channel name as it is parsed 
                                            from line and a pointer to it. */ 
   register short l_crn_row = 0,l_crn_col = 0;
   short r_loc_flg = OFF;  /* Flag which is set when location is found in element file. */
   short c_loc_flg = OFF;  /* Flag which is set when location is found in element file. */
   
   chan_ptr = chan_nm;
   bzero(chan_nm,sizeof(chan_nm));

   *chan_ptr =  NULL;

   /* Move past any characters at beginning of display element. */

   while (isalpha(*m_line))
     m_line++;      
   
   m_line = skip_to_digit(m_line);
   
    /* If there is no channel location, omit the channel from the linked list. */


   if ((*m_line) && (isdigit(*m_line)))
     {
      l_crn_row = to_short(m_line);
      r_loc_flg =  ON;
      /* Move past left row number. */

      while(isdigit(*m_line))
        m_line++;

     /*  Move to column location. */

      m_line  = skip_to_digit(m_line);
 
      if  ((*m_line) && (isdigit(*m_line)))
        {
         l_crn_col = to_short(m_line);
         c_loc_flg =  ON;
         while (isdigit (*m_line))   
           m_line++;
        }
       
     }

   while ( (*m_line) && (!isalpha(*m_line))) 
      m_line++;

   if (isalpha(*m_line))
     {
       while( (isalpha(*m_line)) || (isdigit(*m_line)) || (*m_line == UNDERSC) || (*m_line == COLON))
            *chan_ptr++ = *m_line++;   
       *chan_ptr = NULL;
     }
    chan_ptr = chan_nm;

   /* Enter channel name and location for monitor in monitor linked list. */

   if (win_array[win_ctr]->m_head == NULL)
      {
       if ((*chan_nm)  && (c_loc_flg) && (r_loc_flg))
         {
          win_array[win_ctr]->m_head = mon_alloc(); 
          win_array[win_ctr]->m_head->next = NULL; 
          if (*chan_nm)
             strcpy(win_array[win_ctr]->m_head->chan,chan_nm);
          win_array[win_ctr]->m_head->l_crn_row = l_crn_row; 
          win_array[win_ctr]->m_head->l_crn_col = l_crn_col; 
         }
       else if (!(*chan_nm)) 
         printf("No channel name for monitor. Channel omitted.\n");
       else if ((! (c_loc_flg)) || (! (r_loc_flg)) )
         printf("Incomplete location information for display.\n");
      }
   
   
     /* Otherwise, add the node immediately after the head of the list. */

   else
     {
        if ((*chan_nm)  && (c_loc_flg) && (r_loc_flg))
          {
           node_ptr = mon_alloc(); /* Set the new node's next pointer to point at 
                                              the head of the list. */
           node_ptr->next= win_array[win_ctr]->m_head; /* Set pointer to the head to point at the new node. */
           win_array[win_ctr]->m_head = node_ptr;
           if (*chan_nm)
              strcpy(win_array[win_ctr]->m_head->chan,chan_nm);
           win_array[win_ctr]->m_head->l_crn_row = l_crn_row; 
           win_array[win_ctr]->m_head->l_crn_col = l_crn_col; 
          }
        else if (!(*chan_nm)) 
          printf("No channel name for monitor. Channel omitted.\n"); 
        else if ((! (c_loc_flg)) || (! (r_loc_flg)) )
          printf("Incomplete location information for display. Channel omitted.\n");
      }
   return; 
  }

/************************************************************************************/
/* This function attaches a text node to the linked list headed by win_array->      */
/* txt. It returns the pointer to the head of the list which now points to the new  */
/* node.                                                                            */
/************************************************************************************/

static VOID add_txt(win_array,win_ctr,t_line)
    struct window_node *win_array[MAX_DISP_NUM]; 
    short win_ctr; /* Counters. */
    char *t_line;
  {
   struct txt_node *node_ptr; 
   char txt_str[MAX_LIN_LEN],*txt_ptr; /* Array to hold text string as it is parsed 
                                            from line and a pointer to it. */ 

   register short l_crn_row = 0,l_crn_col = 0;
   short r_loc_flg = OFF;  /* Flag which is set when location is found in element file. */
   short c_loc_flg = OFF;  /* Flag which is set when location is found in element file. */

   txt_ptr = txt_str;
   bzero(txt_str,sizeof(txt_str));

   *txt_ptr =  NULL;

   /* Move past any characters at beginning of display element. */

   while (isalpha(*t_line))
     t_line++;      
   
   /* Move to next digit in display element. */

   t_line = skip_to_digit(t_line);


    /* If there is no text location, omit the string from the linked list. */

   if (*t_line)
     {
      l_crn_row = to_short(t_line);
      r_loc_flg =  ON;
      while (isdigit(*t_line))
        t_line++;
      t_line = skip_to_digit(t_line);
   
      if ((*t_line) && (isdigit (*t_line)))
        {
         l_crn_col = to_short(t_line);
         c_loc_flg = ON;

         /* Skip past digit. */

         while (isdigit(*t_line))
           t_line++;
         
        }
      
     }


   /* Skip digits and blanks and illegal characters then read in text string. */ 

   while ( (*t_line) && (!isalpha(*t_line)) && (!isdigit(*t_line)) && (*t_line != UNDERSC) 
         && (*t_line != COLON) && (*t_line != ASTER))
      t_line++;

   /* Text string must begin with an alphanumeric character, and underscore,a colon, or an
      asterisk. After the first character blanks will be legal for text strings. */

   if ((isalpha(*t_line)) || (isdigit(*t_line)) || (*t_line == UNDERSC) || (*t_line == COLON) || 
       (*t_line == ASTER))
     {
       while( (isalpha(*t_line)) || (isdigit(*t_line)) || (*t_line == UNDERSC) || (*t_line == BLANK)
               || (*t_line == COLON) || (*t_line == ASTER) ||(*t_line == DOT))
            *txt_ptr++ = *t_line++;   
       *txt_ptr = NULL;
     }
    
   if (win_array[win_ctr]->t_head == NULL)
      {
       if ((*txt_str)  && (r_loc_flg) && (c_loc_flg))
         {
          win_array[win_ctr]->t_head = txt_alloc(); 
          win_array[win_ctr]->t_head->next = NULL; 
          if (*txt_str)
             strcpy(win_array[win_ctr]->t_head->txt_str,txt_str);
          win_array[win_ctr]->t_head->l_crn_row = l_crn_row; 
          win_array[win_ctr]->t_head->l_crn_col = l_crn_col; 
         }
        else if (! (txt_str))
          printf("No text string. Text node omitted.\n");
        else if ((!(r_loc_flg)) ||  (! (c_loc_flg)))
          printf("Incomplete location information. Text node omitted.\n");
      }
   
   
     /* Otherwise, add the node immediately after the head of the list. */

   else
     {
        if ((*txt_str)  && (r_loc_flg) && (c_loc_flg))
          {
           node_ptr = txt_alloc(); /* Set the new node's next pointer to point at 
                                              the head of the list. */
           node_ptr->next= win_array[win_ctr]->t_head; /* Set pointer to the head to point at the new node. */
           win_array[win_ctr]->t_head = node_ptr;
           if (*txt_str)
              strcpy(win_array[win_ctr]->t_head->txt_str,txt_str);
           win_array[win_ctr]->t_head->l_crn_row = l_crn_row; 
           win_array[win_ctr]->t_head->l_crn_col = l_crn_col; 
          }
        else if (! (txt_str))
          printf("No text string. Text node omitted.\n");
        else if ((!(r_loc_flg)) ||  (! (c_loc_flg)))
          printf("Incomplete location information. Text node omitted.\n");
      }
   return;   
  }

/************************************************************************************/
/* This function throws away all characters and returns a pointer                   */
/* to the first digit.                                                              */
/************************************************************************************/
   
static char *skip_to_digit(pstr)
  char *pstr;
   {

    while (*pstr) 
      {
       if( (isdigit(*pstr)) )
         return (pstr);
       else if (pstr == NULL)
         {
          return (NULL);
         }
       else
        pstr++;
      }

   return (NULL);
   }

/************************************************************************************/
/*   Get a number from display element line, convert it to register short.          */
/* This function was stolen from Bob Dalesio with a slight modification.            */
/************************************************************************************/

static int to_short(pstr)
  char *pstr;
  { 
  short num;  
  
   num = 0;
   while(isdigit(*pstr))
     {
      num = (num * 10) + (*pstr -'0');
      pstr++;
     }
   return num;
  }
  

/*************************************************************************************/
/* This subroutine will be used for debugging only.  It reads out the linked list of */
/* control nodes. It is best to comment out print_menu when using this.              */
/*************************************************************************************/

static VOID read_disp_lst(wnd_array,nfiles)
struct window_node *wnd_array[MAX_DISP_NUM];
int nfiles;
{
 struct mon_node *ctl_ptr;
 struct mon_node *mon_ptr;  
 struct txt_node *txt_ptr;  
 short lopi_i,lopi_j;  /* Counters. */
  
 lopi_j = 1;
 for (lopi_i = 0; lopi_i < nfiles; lopi_i++)
    {
       if (wind_array[lopi_i] != NULL)
         {
          ctl_ptr = wnd_array[lopi_i]->c_head;
          if ((ctl_ptr == NULL) || (ctl_ptr->next->l_crn_row == LIST_END ))
            printf("Ctl list for display %d is empty.\n",lopi_i);
          else
           {
            ctl_ptr = ctl_ptr->next;
            while (ctl_ptr->l_crn_row != LIST_END)
             {
              printf("Display %d: Node %d\n",lopi_i,lopi_j);
              printf("wnd_array[%d]->c_head->chan = %s \n",lopi_i,ctl_ptr->chan); 
              printf("wnd_array[%d]->c_head->l_crn_row = %d \n",lopi_i,ctl_ptr->l_crn_row); 
              printf("wnd_array[%d]->c_head->l_crn_col = %d \n",lopi_i,ctl_ptr->l_crn_col); 
              lopi_j++; 
              ctl_ptr = ctl_ptr->next;
             } 
           }  
        
           lopi_j = 1; 
           mon_ptr = wnd_array[lopi_i]->m_head;
           if (mon_ptr == NULL)
             printf("Mon list for display %d is empty.\n",lopi_i);
           else
            {
             while (mon_ptr != NULL)
              {
               printf("Display %d: Node %d\n",lopi_i,lopi_j);
               printf("wnd_array[%d]->m_head->chan = %s \n",lopi_i,mon_ptr->chan); 
               printf("wnd_array[%d]->m_head->l_crn_row = %d \n",lopi_i,mon_ptr->l_crn_row); 
               printf("wnd_array[%d]->m_head->l_crn_col = %d \n",lopi_i,mon_ptr->l_crn_col); 
               lopi_j++; 
               mon_ptr = mon_ptr->next;
              } 
            }
           lopi_j = 1;
          txt_ptr = wnd_array[lopi_i]->t_head;
          if (txt_ptr == NULL)
            printf("Txt list for display %d is empty.\n",lopi_i);
          else
            {
          while (txt_ptr != NULL)
            {
             printf("Display %d: Node %d\n",lopi_i,lopi_j);
             printf("wnd_array[%d]->t_head->txt_str = %s \n",lopi_i,txt_ptr->txt_str); 
             printf("wnd_array[%d]->t_head->l_crn_row = %d \n",lopi_i,txt_ptr->l_crn_row); 
             printf("wnd_array[%d]->t_head->l_crn_col = %d \n",lopi_i,txt_ptr->l_crn_col); 
           lopi_j++; 
           txt_ptr = txt_ptr->next;
          } 
        }
     }
      lopi_j = 1; 
   }
 return;
}  
 

/***************************************************************************************/
/*  This function puts text specified by the user in the display file on the screen.   */
/***************************************************************************************/

static VOID display_text(disp_array,selected)
   struct window_node *disp_array[MAX_DISP_NUM];
   short selected;
   {
    short mrow,mcol; 
    struct txt_node *txt_ptr; 
    char txt_str[MAX_LIN_LEN];
    mrow = 0; 
    mcol = 0;
    mv_cursor(&mrow,&mcol,NO_CMD);
    txt_ptr =  disp_array[selected]->t_head;
   
    while (txt_ptr != NULL)
      { 
       mrow = txt_ptr->l_crn_row;
       mcol = txt_ptr->l_crn_col;
       strcpy(txt_str,txt_ptr->txt_str);
       mv_cursor(&mrow,&mcol,txt_str);
       txt_ptr = txt_ptr->next;
      }
   
    return;
   }

/***************************************************************************************/
/*  Displays a file chosen by the user from the display file which was created when    */
/* the program initialized. Also calls put_value() to write to controls if the user    */
/* selects and F7.                                                                      */
/***************************************************************************************/

static VOID display_file(disp_array,selected,screen_up,data_num,pdata_flg,k_buff,val_in)
   struct window_node *disp_array[MAX_DISP_NUM];
   short selected;
   short *screen_up;
   short *data_num;  /* Flag indicating that there is data waiting to 
                       be read from the input buffer. */
   short *pdata_flg; /* Flag indicating that a monitor has brought in new data. */
   char k_buff[BUFF_SIZE]; /* Buffer for characters entered. */
   char val_in[MAX_STRING_SIZE];
   {
    char *w_ptr; /* Pointer to next available space for writing. */
    struct mon_node *c_ptr = NULL; 
    struct mon_node *m_ptr = NULL;
    struct txt_node *t_ptr = NULL;
    char txt_str[MAX_LIN_LEN];
    char prefix[MAX_LIN_LEN];  
    int nmbr; 
    short krow,kcol;  /* Variables keeping track of where cursor is at all times.  */
    short trow,tcol;  /* Variables keeping track of where cursor is at all times.  */
    short init;  /* Flag to indicate display monitors whether to initialize monitors or
                    not. */
    

    /* Initialize buffers into which channel names and values are
       written for a put. */
    pdata_flg = OFF;
    k_buff[BUFF_SIZE - 1] = NULL;
    w_ptr = k_buff;
    *data_num = 0;
    num_times = 0;
    krow = 0; /*Krow and kcol store cursor position. */
    kcol = 0;

    trow = 0; /* trow and tcol store cursor position for diagnostic messages only. */
    trow = 0; 
 
    /* If a non existant screen has been selected, print error message. */

    if ( disp_array[selected] == NULL )
      {
       trow++;
       tcol = 0;
       mv_cursor(&trow,&tcol,"Found no display file for this display.");
       trow++;
       taskDelay(30); 
       selected = NOTHING_SELECTED;
       *screen_up = OFF;
       w_ptr = k_buff;
       (*data_num) = 0; 
      }
    else 
      {
       trow++;
       tcol = 0;
       m_ptr = disp_array[selected]->m_head;
       c_ptr = disp_array[selected]->c_head;
       t_ptr = disp_array[selected]->t_head;
       /* If a screen with no data has been selected, exit. */

       if ((c_ptr == NULL) && (m_ptr == NULL) 
         && ( t_ptr == NULL))
         {
          trow++;
          tcol = 0;
          mv_cursor(&trow,&tcol,"Found nothing in this file.");
          taskDelay(60);
          selected = NOTHING_SELECTED;
          *screen_up = OFF;
          w_ptr = k_buff;
          (*data_num) = 0; 
         }
       else
         {
          /* Otherwise put up text and initialize the monitors.  */ 
           if(t_ptr != NULL);
              display_text(disp_array,selected);
           init = ON;
           if ((m_ptr != NULL) || (c_ptr != NULL))
             {
              display_monitors(disp_array,selected,screen_up,pdata_flg,&init,except_ptr);
             }
           init = OFF;
    
          /* Place cursor on position of first control . */
           
           if ((c_ptr != NULL) && (c_ptr->next->l_crn_row != LIST_END)) 
            {
              c_ptr = c_ptr->next; 
              krow = c_ptr->l_crn_row;
              kcol = c_ptr->l_crn_col;
              mv_cursor(&krow,&kcol,NO_CMD); 
            }
         }
      }

    while (*screen_up == ON)  
      {

       /* If there is something in the buffer it will lopi consumed 
          until it is used up. */  
       

       while (*data_num > 0)
         {
          switch (*w_ptr)
            {
             case F6:   /* F6 will cause an exit.  Everything else in buffer 
                           will be lost. */
               semTake(key_sem);
               w_ptr = k_buff;
               *data_num = 0;
               *screen_up = OFF; 
               semGive(key_sem);
               break;
             case F7:
               display_text(disp_array,selected);
               semTake(key_sem);
               w_ptr = k_buff;
               *data_num = 0;
               semGive(key_sem);
               trow = BEG_BOX;
               tcol = BEG_BOX;
               if(c_ptr != NULL)
                 put_value(c_ptr,val_in); 
               break;
             case U_ARROW:
               if (c_ptr != NULL)
                 {
                  if (c_ptr->prev->l_crn_row != LIST_END)
                   { 
                    c_ptr = c_ptr->prev;
                    krow = c_ptr->l_crn_row;
                    kcol = c_ptr->l_crn_col;
                    mv_cursor(&krow,&kcol,NO_CMD);
                   }
                  else
                   {
                    c_ptr = c_ptr->prev->prev;
                    krow = c_ptr->l_crn_row;
                    kcol = c_ptr->l_crn_col;
                    mv_cursor(&krow,&kcol,NO_CMD);
                   }
                  w_ptr++;
                  semTake(key_sem);
                  (*data_num)--;
                  if ((*w_ptr) == NULL)
                     w_ptr = k_buff; 
                  semGive(key_sem);
                 }
               break;
             case D_ARROW:
               if (c_ptr != NULL)
                 {
                  if (c_ptr->next->l_crn_row != LIST_END)
                    { 
                     c_ptr = c_ptr->next;
                     krow = c_ptr->l_crn_row;
                     kcol = c_ptr->l_crn_col;
                     mv_cursor(&krow,&kcol,NO_CMD);
                    }
                  else
                   {
                    c_ptr = c_ptr->next->next;
                    krow = c_ptr->l_crn_row;
                    kcol = c_ptr->l_crn_col;
                    mv_cursor(&krow,&kcol,NO_CMD);
                   }
                  semTake(key_sem);
                  w_ptr++;
                  (*data_num)--;
                  if ((*w_ptr) == NULL)
                    w_ptr = k_buff; 
                  semGive(key_sem);
                 }
               break;
             case RETURN:
               semTake(key_sem);
               (*data_num) = 0;
               w_ptr =  k_buff;
               semGive(key_sem);
               break;
             default: 
               w_ptr++;
               semTake(key_sem);
               if ((*w_ptr) == NULL)
                  w_ptr = k_buff; 
               semGive(key_sem);
               break;
           }  /* End switch. */
            

      } /* End while data_num > 0 */
    
      if (*pdata_flg)
        {
         display_monitors(disp_array,selected,screen_up,pdata_flg,&init,except_ptr); 
         taskDelay(5);
         mv_cursor(&krow,&kcol,NO_CMD);
        } 
   

  } /*  end while screen up. */

  if (disp_array[selected]  != NULL) 
   {
       stop_monitors(disp_array,selected); 
   } 
 
 return;
}

/********************************************************************************/
/*  VOID display_monitors                                                       */
/*      Uses a linked list of monitors and a doubly linked circular list of     */
/*  controls. Controls and monitors are treated the same as far as this         */
/*  subroutine is concerned. That is both are monitored.                        */
/********************************************************************************/

static VOID display_monitors(disp_array,m_select,screen,pdata_flg,init,except_ptr)
   struct window_node *disp_array[MAX_DISP_NUM];
   short m_select; /* Pointer to number of screen selected. */
   short *screen;  /* Pointer to variable which tells when to begin desplay. */
   short *pdata_flg; /* Flag to indicate that new data had been received. */
   short *init;  /* Flag to indicate that monitors need to be initialized. */
   struct except_node *except_ptr;
   {
    short mon_row,mon_col; /* Local variable used to move cursor to position where update is
                            required.  After update, cursor will always be returned to the
                            esition where keyboard entry cursor is located.  */
    short trow = 1;short tcol = 0;
    int status;
    chtype type = TYPENOTCONN;
    char txt_str[120];
    char prefix[80];
    struct mon_node *mon_ptr, *ctl_ptr;

    mon_row = 0;
    mon_col = 0;
    mon_ptr =  disp_array[m_select]->m_head;
    
    if (*init)     
      {
       SEVCHK(ca_task_initialize(),"Unable to initialize.");

       status = ca_add_exception_event(lopi_exception_handler,except_ptr); 
       if (status != ECA_NORMAL)
         logMsg(" Exception handler error message:%d\n",ca_message_text[CA_EXTRACT_MSG_NO(status)]);
       except_ptr->flg = NO_NEW_DATA;
       bzero(except_ptr->msg,(sizeof(except_ptr->msg))); 


       ctl_ptr =  disp_array[m_select]->c_head;
       if ((ctl_ptr != NULL) && (ctl_ptr->next->l_crn_row != LIST_END)){
          ctl_ptr = ctl_ptr->next;
          while (ctl_ptr->l_crn_row != LIST_END)
           { 
            ctl_ptr->ev_ptr = ev_alloc();
            ctl_ptr->ev_ptr->data_flg = NO_NEW_DATA;
            ctl_ptr->conn_flg = CA_OP_CONN_DOWN;
            mon_row = ctl_ptr->l_crn_row;
            mon_col = ctl_ptr->l_crn_col;
            ctl_ptr->prev_size = 4;
      
           /* Put up a row of x's where monitor is to be displayed. */

           ESCAPE
           REVERSE_VIDEO
           strcpy(ctl_ptr->ev_ptr->str,"   "); 
           mv_cursor(&mon_row,&mon_col,ctl_ptr->ev_ptr->str); 
           ESCAPE
           ATR_OFF
           status = ca_build_and_connect(ctl_ptr->chan,TYPENOTCONN,0,&ctl_ptr->chan_id,
                  NULL,lopi_conn_handler,ctl_ptr);
          if (status != ECA_NORMAL)
            logMsg(" ca_build_and_connect:%d\n",ca_message_text[CA_EXTRACT_MSG_NO(status)]);
           ctl_ptr = ctl_ptr->next;
          }
        }


        while (mon_ptr != NULL)
          { 
           mon_ptr->ev_ptr = ev_alloc();
           mon_ptr->ev_ptr->data_flg = NO_NEW_DATA;
           mon_ptr->conn_flg = CA_OP_CONN_DOWN;
           mon_row = mon_ptr->l_crn_row;
           mon_col = mon_ptr->l_crn_col;
           mon_ptr->prev_size = 4;
      
           /* Put up a row of x's where monitor is to be displayed. */

          ESCAPE
          REVERSE_VIDEO
          strcpy(mon_ptr->ev_ptr->str,"   "); 
          mv_cursor(&mon_row,&mon_col,mon_ptr->ev_ptr->str); 
          ESCAPE
          ATR_OFF
          status = ca_build_and_connect(mon_ptr->chan,TYPENOTCONN,0,&mon_ptr->chan_id,
                    NULL,lopi_conn_handler,mon_ptr);
          if (status != ECA_NORMAL)
            logMsg(" ca_build_and_connect:%d\n",ca_message_text[CA_EXTRACT_MSG_NO(status)]);
          mon_ptr = mon_ptr->next;
         }    


       mon_ptr =  disp_array[m_select]->m_head;

        while (mon_ptr != NULL)
         { 
          status = ca_add_event(DBR_STS_STRING,mon_ptr->chan_id,lopi_ev_handler,mon_ptr->ev_ptr,NULL);        
          if (status != ECA_NORMAL)
             logMsg("Message:%d\n",ca_message_text[CA_EXTRACT_MSG_NO(status)]);
          type = ca_field_type(mon_ptr->chan_id);

          if (DEBUG)
            logMsg("Added event for %s,status = %d,type = %d\n",mon_ptr->chan,status,type);   

         /* If channel is found display data if channel is up. Display error messae if channel 
          is down. */  


        if ((type = (ca_field_type(mon_ptr->chan_id))) !=  TYPENOTCONN)
 

        /*  If channel is connected display error message if it is down.  
            Print data if it is up. */ 
          {
           if (mon_ptr->conn_flg == CA_OP_CONN_UP)
              {
               mon_row = mon_ptr->l_crn_row; 
               mon_col = mon_ptr->l_crn_col;
               if (mon_ptr->prev_size > (strlen(mon_ptr->ev_ptr->str))) 
                 {
                  blank_fill(txt_str,mon_ptr->prev_size);
                  mv_cursor(&mon_row,&mon_col,txt_str); 
                 }  
               mon_ptr->prev_size = strlen(mon_ptr->ev_ptr->str);
               mv_cursor(&mon_row,&mon_col,mon_ptr->ev_ptr->str);
              } 
           else if (mon_ptr->conn_flg == CA_OP_CONN_DOWN) 
             {
              ESCAPE
              REVERSE_VIDEO
              strcpy(txt_str,"CHAN DOWN");
              mv_cursor(&mon_row,&mon_col,txt_str); 
              ESCAPE
              ATR_OFF
             }
           else
             {
              ESCAPE
              REVERSE_VIDEO
              strcpy(txt_str,"UNDEF");
              mv_cursor(&mon_row,&mon_col,txt_str); 
              ESCAPE
              ATR_OFF
             }
          }
   
       mon_ptr = mon_ptr->next;

      }   
 

        if((ctl_ptr != NULL) && (disp_array[m_select]->c_head->next->l_crn_row != LIST_END))
          {  
           ctl_ptr = disp_array[m_select]->c_head->next;  

           while (ctl_ptr->l_crn_row  != LIST_END)
            { 
             status = ca_add_event(DBR_STS_STRING,ctl_ptr->chan_id,lopi_ev_handler, ctl_ptr->ev_ptr,NULL);      
             if (status != ECA_NORMAL)
                logMsg("Message:%s\n",ca_message_text[CA_EXTRACT_MSG_NO(status)]);
             type = ca_field_type(ctl_ptr->chan_id);
             if (DEBUG)
               logMsg("Added event for %s,status = %d,type = %d\n",ctl_ptr->chan,status,type);  

         /* If channel is connected, display data if channel is up.  Display error message if
                channel is down. */


          if ((type = (ca_field_type(ctl_ptr->chan_id))) !=  TYPENOTCONN)
               {
                if (ctl_ptr->conn_flg == CA_OP_CONN_UP)
                 {
                  mon_row = ctl_ptr->l_crn_row; 
                  mon_col = ctl_ptr->l_crn_col;
                  if (ctl_ptr->prev_size > strlen(ctl_ptr->ev_ptr->str)) 
                    {
                     blank_fill(txt_str,ctl_ptr->prev_size);
                     mv_cursor(&mon_row,&mon_col,txt_str);
                    }  
                  mv_cursor(&mon_row,&mon_col,ctl_ptr->ev_ptr->str);
                  ctl_ptr->prev_size = strlen(ctl_ptr->ev_ptr->str); 
                 }
                else if (ctl_ptr->conn_flg == CA_OP_CONN_DOWN)
                 {
                  ESCAPE
                  REVERSE_VIDEO
                  strcpy(txt_str,"CHAN DOWN");
                  mv_cursor(&mon_row,&mon_col,txt_str); 
                  ESCAPE
                  ATR_OFF
                 }
                else
                 {
                  ESCAPE
                  REVERSE_VIDEO
                  strcpy(txt_str,"UNDEF");
                  mv_cursor(&mon_row,&mon_col,txt_str); 
                  ESCAPE
                  ATR_OFF
                 }
          }  /* If type is not TYPENOTCONN. */
      
            /* If channel was never connected, display appropriate message. */ 


           ctl_ptr = ctl_ptr->next;
            }  /* End while ctl_ptr not equal to LIST END */    

    } /* End if ctl_ptr != NULL etc. */


         status = ca_flush_io(); 
         SEVCHK(status,NULL);
    
         if  (except_ptr->flg)
           {
            lopi_signal(except_ptr->status,except_ptr->msg);
            if (status != ECA_NORMAL)
              logMsg("Status abnormal: %d\n",ca_message_text[CA_EXTRACT_MSG_NO(status)]);
            else
              logMsg("Normal status:%d\n",ca_message_text[CA_EXTRACT_MSG_NO(status)]);
            except_ptr->flg = NO_NEW_DATA; 
           }   
        
       } /* End if init */
    else
       {
        mon_ptr =  disp_array[m_select]->m_head;
        while (mon_ptr != NULL) 
          { 
           if ((mon_ptr->ev_ptr->data_flg) && (mon_ptr->conn_flg == CA_OP_CONN_UP))
             {
              mon_row = mon_ptr->l_crn_row; 
              mon_col = mon_ptr->l_crn_col;
              mon_ptr->ev_ptr->data_flg = NO_NEW_DATA;
              if (mon_ptr->prev_size > strlen(mon_ptr->ev_ptr->str)) 
                {
                 blank_fill(txt_str,mon_ptr->prev_size); 
                 mv_cursor(&mon_row,&mon_col,txt_str);
                }
              mon_ptr->prev_size = strlen(mon_ptr->ev_ptr->str + 2); 
              mv_cursor(&mon_row,&mon_col,mon_ptr->ev_ptr->str); 
              mon_ptr = mon_ptr->next;
             } 
           else if (mon_ptr->conn_flg == CA_OP_CONN_DOWN)
            {
              mon_row = mon_ptr->l_crn_row; 
              mon_col = mon_ptr->l_crn_col;
              ESCAPE
              REVERSE_VIDEO
              strcpy(txt_str,"CHAN DOWN");
              mv_cursor(&mon_row,&mon_col,txt_str); 
              ESCAPE
              ATR_OFF
              mon_ptr->prev_size = 10;
              mon_ptr = mon_ptr->next;
            }
           else
             mon_ptr = mon_ptr->next;
    
          }   
        ctl_ptr =  disp_array[m_select]->c_head;

        if ((ctl_ptr != NULL) && (disp_array[m_select]->c_head->next->l_crn_row != LIST_END))
          {
           ctl_ptr = disp_array[m_select]->c_head->next;

           while (ctl_ptr->l_crn_row != LIST_END)
             { 
             if ((ctl_ptr->ev_ptr->data_flg) && (ctl_ptr->conn_flg == CA_OP_CONN_UP))
               {
                mon_row = ctl_ptr->l_crn_row; 
                mon_col = ctl_ptr->l_crn_col;
                ctl_ptr->ev_ptr->data_flg = NO_NEW_DATA;
                if (ctl_ptr->prev_size > strlen(ctl_ptr->ev_ptr->str)) 
                  {
                   blank_fill(txt_str,ctl_ptr->prev_size );
                   mv_cursor(&mon_row,&mon_col,txt_str);
                  } 
                ctl_ptr->prev_size =(strlen(ctl_ptr->ev_ptr->str)) + 2; 
                mv_cursor(&mon_row,&mon_col,ctl_ptr->ev_ptr->str); 
                ctl_ptr = ctl_ptr->next;
               }
             else if (ctl_ptr->conn_flg == CA_OP_CONN_DOWN)
               {
                mon_row = ctl_ptr->l_crn_row; 
                mon_col = ctl_ptr->l_crn_col;
                ESCAPE
                REVERSE_VIDEO
                strcpy(txt_str,"CHAN DOWN");
                mv_cursor(&mon_row,&mon_col,txt_str); 
                ESCAPE
                ATR_OFF
                ctl_ptr->prev_size = 10;
                ctl_ptr = ctl_ptr->next;
               }
             else
               ctl_ptr = ctl_ptr->next; 
              }  /* End while ctl_ptr != LIST_END. */
           }  /* End if ctl_ptr != NULL etc. */

      semTake(mon_sem);
      *pdata_flg = NO_NEW_DATA ;  
      semGive(mon_sem); 

      if  (except_ptr->flg)
        {
         lopi_signal(except_ptr->status,except_ptr->msg); 
         mon_col++;
         if (status != ECA_NORMAL)
            logMsg("Status abnormal: %d\n",ca_message_text[CA_EXTRACT_MSG_NO(status)]);
         else
            logMsg("Normal status:%d\n",ca_message_text[CA_EXTRACT_MSG_NO(status)]);
         except_ptr->flg = NO_NEW_DATA; 
        }  

      } /* End else */
   }

/*************************************************************************************/
/* VOID stop_monitors                                                                */  
/*************************************************************************************/
static VOID stop_monitors(disp_array,m_select)
   struct window_node *disp_array[MAX_DISP_NUM];
   short m_select;  /* Pointer to number of screen selected. */
  {
   struct mon_node *mon_ptr, *ctl_ptr;



     /* Free channel access resources. */

    mon_ptr =  disp_array[m_select]->m_head;
    semTake(mon_sem);        

    while (mon_ptr != NULL) 
      {
       ca_clear_channel(mon_ptr->chan_id);           
       mon_ptr = mon_ptr->next; 
      } 

   ctl_ptr =  disp_array[m_select]->c_head;

   if ((ctl_ptr != NULL) && (disp_array[m_select]->c_head->next->l_crn_row != LIST_END))
    {
     ctl_ptr = disp_array[m_select]->c_head->next;
     while (ctl_ptr->l_crn_row != LIST_END)
      {
       ca_clear_channel(ctl_ptr->chan_id);           
       ctl_ptr = ctl_ptr->next;
      } 
    } 

     /* Exit channel access. */ 

     ca_flush_io(); 

     semGive(mon_sem);    

     /* Initialize monitor pointer to the head of the monitor linked list
             in order to move through linked list freeing memory. */
 

     semTake(mon_sem);        
     mon_ptr =  disp_array[m_select]->m_head;
     while (mon_ptr != NULL)
       {
        free(mon_ptr->ev_ptr);
        mon_ptr->ev_ptr = NULL;
        mon_ptr = mon_ptr->next;
       } 
 
     /* Initialize control pointer to the head of the control linked list
       in order to move through the list freeing memory. */ 


  ctl_ptr =  disp_array[m_select]->c_head;

    if ((ctl_ptr != NULL) && (disp_array[m_select]->c_head->next->l_crn_row != LIST_END)){
      ctl_ptr = disp_array[m_select]->c_head->next;

     while (ctl_ptr->l_crn_row != LIST_END)
        {
         free(ctl_ptr->ev_ptr);
         ctl_ptr->ev_ptr = NULL;
         ctl_ptr = ctl_ptr->next;
        }
    }
      semGive(mon_sem);
 
 
 }

/*************************************************************************************/
/*      Monitor event handler. Sets the global data_flg when an event is received    */  
/* asynchronously. The flag is reset in display_monitors.                            */  
/*************************************************************************************/



VOID lopi_ev_handler(lopi_arg)
  struct event_handler_args lopi_arg; 
   {
    struct dbr_sts_string *sts_str_ptr;
    struct ev_node *ev_ptr;
    char txt_str[40];
    static short mon_row = 0;
    static short mon_col = 0;
    char name[15],*nm_ptr;
    struct mon_node *mptr; 
    
    sts_str_ptr = (struct dbr_sts_string *)lopi_arg.dbr;
    ev_ptr = ((struct ev_node *)lopi_arg.usr);
    
    /* If new data has arrived, copy it to the data_string and 
       set a flag indicating new data. */
    
   mptr = (struct mon_node *)ca_puser(lopi_arg.chid); 
   nm_ptr =(char *)(ca_name(lopi_arg.chid));
   semTake(mon_sem);
   if (DEBUG)   
     logMsg("Ev hand for CA:%s, BW:%s\n",nm_ptr,mptr->chan); 
   if ((data_flg = strncmp(ev_ptr->str,sts_str_ptr->value,12) != 0))
      {
        strncpy(ev_ptr->str, sts_str_ptr->value,12); 
        ev_ptr->str[12] = NULL;
        ev_ptr->data_flg = NEW_DATA;   
      } 
   semGive(mon_sem);
   }

/*************************************************************************************/
/*  Exception handler. This routine is called by channel access for asychronous      */  
/*  events in the server.                                                            */  
/*************************************************************************************/

  VOID lopi_exception_handler(lopi_except_arg)
  struct exception_handler_args lopi_except_arg; 
   {
    struct except_node *except_ptr;
    except_ptr = (struct except_node  *)lopi_except_arg.usr;
    if (DEBUG)
       logMsg("Exception handler got exception. \n");
    strncpy(except_ptr->msg,lopi_except_arg.ctx, MAX_LIN_LEN - 2); 
    except_ptr->msg[MAX_LIN_LEN - 1] = NULL;
    except_ptr->status = lopi_except_arg.stat;
    except_ptr->flg = NEW_DATA; 
   } 

/*************************************************************************************/
/*  Monitor connection handler. This routine is called by channel access to report   */  
/*  changes in the connection status channels.                                       */  
/*************************************************************************************/

VOID lopi_conn_handler(lopi_connect_arg)
  struct connection_handler_args lopi_connect_arg; 
   {
    struct mon_node *pmon;
    int status; 
    char txt_str[40];
    static short mon_row = 0;
    static short mon_col = 0;
    char name[15],*nm_ptr;
     
    pmon = (struct mon_node *)ca_puser(lopi_connect_arg.chid); 
    nm_ptr =  (char *)ca_name(lopi_connect_arg.chid);
    if (lopi_connect_arg.op == CA_OP_CONN_DOWN)
        {
         pmon->conn_flg = CA_OP_CONN_DOWN;  
        }
    else if  (lopi_connect_arg.op == CA_OP_CONN_UP)
        {
         pmon->conn_flg = CA_OP_CONN_UP;
        }
    else 
         pmon->conn_flg = UNDEF;

    if (DEBUG)
      logMsg("Conn hand for CA:%s, BW:%s\n",nm_ptr,pmon->chan);  
    
   }

/*************************************************************************************/
/*  Frees memory allocated by lopi.c                                                   */
/*************************************************************************************/

void free_mem(disp_array,dmenu,nlines,except_ptr)
   struct window_node *disp_array[MAX_DISP_NUM];
   int nlines;
   char *dmenu[MXMENU];    
  {
    struct txt_node *txt_ptr;
    struct mon_node *ctl_ptr;
    struct mon_node *mon_ptr;
    char *mptr;    
    register short lopi_i; 
      for (lopi_i = 0; lopi_i < nlines; lopi_i++)
        {
          while (disp_array[lopi_i]->t_head != NULL)
            {
             txt_ptr = disp_array[lopi_i]->t_head;  
             disp_array[lopi_i]->t_head = disp_array[lopi_i]->t_head->next; 
             free(txt_ptr);
            } 

          while(disp_array[lopi_i]->c_head != NULL)
            {
             ctl_ptr = disp_array[lopi_i]->c_head;  
             disp_array[lopi_i]->c_head = disp_array[lopi_i]->c_head->next; 
             free(ctl_ptr);
            }

          while(disp_array[lopi_i]->m_head != NULL)
            {
             mon_ptr = disp_array[lopi_i]->m_head;  
             disp_array[lopi_i]->m_head = disp_array[lopi_i]->m_head->next; 
             free(mon_ptr);
            }
          
           mptr =  dmenu[lopi_i];
           free(mptr); 
           dmenu[lopi_i] = NULL; 

        }
        free(except_ptr);
   return;
  }

/********************************************************************************/
/*  Puts the value passed to it as a string to the data base channel which cptr */
/* points to.                                                                   */
/********************************************************************************/

void put_value(c_ptr,val_in)
  char val_in[MAX_STRING_SIZE];  /* Array containing value to be put. */
  struct mon_node *c_ptr;     /* Pointer to record of channel to which value is to
                                 be passes. */
  {
   status =  ca_put(DBR_STRING,c_ptr->chan_id,((void *)val_in)); 
   if (status != ECA_NORMAL)
      logMsg("Message:%s\n",ca_message_text[CA_EXTRACT_MSG_NO(status)]);
   if (DEBUG)
     logMsg("Putting %s to %s\n",val_in,c_ptr->chan);
   status = ca_flush_io(); 
   if (status != ECA_NORMAL)
      logMsg("Message:%s\n",ca_message_text[CA_EXTRACT_MSG_NO(status)]);
   return; 
  }
