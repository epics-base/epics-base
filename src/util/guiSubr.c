/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	02-08-91
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991-92, the Regents of the University of California,
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
 * .00	02-08-91	rac	initial version
 * .01	07-30-91	rac	installed in SCCS
 * .02	08-14-91	rac	add guiNoticeName; add more documentation
 * .03	09-06-91	rac	add GuiCheckbox..., guiGetNameFromSeln
 * .04	10-23-91	rac	allow window manager to position command
 *				frames; add file selector routine; add
 *				guiInit, guiFileSelect, guiShellCmd; add
 *				guiTextswXxx; don't specify position for
 *				frames
 * .05	02-25-92	rac	add a text editor command frame; guiInit
 *				now gets host name and user name; add a
 *				printer command frame and print routine;
 *				add "puts" for text subwindows
 *
 * make options
 *	-DXWINDOWS	to use xview/X Window System
 */
#if 0	/* allow embedding comments in the header below */
/*+/mod***********************************************************************
* TITLE	guiSubr.c - general gui routines for xview
*
* DESCRIPTION
*	These routines comprise an initial attempt to provide gui capabilities
*	in a generic way.  At present, the routines are heavily xview
*	oriented.  The routines will probably evolve, especially with
*	regard to xview item typedef usage (e.g., Panel, Xv_opaque, etc.).
*
*	For more information, see USAGE, below.
*
* QUICK REFERENCE
*
*     Textsw guiBrowser(parent, panel, nCol, nLines, fileName)
* Panel_item guiButton(label, panel, <>pX, pY, <>pHt, proc, menu,
*                                      key1, val1, key2, val2)
*       void guiButtonCenter(button1, button2, panel)
*       void guiCFdismiss_xvo(item)
*       void guiCFshow_mi(menu, menuItem)
*       void guiCFshow_xvo(item)
*       void guiCFshowPin_mi(menu, menuItem)
*       void guiCFshowPin_xvo(item)
* Panel_item guiCheckbox(label, panel, nItems, <>pX, pY, <>pHt, proc,
*                                      key1, val1, key2, val2)
*       void guiCheckboxItem(label,checkbox,itemNum,dfltFlag,<>pX,pY,<>pHt)
*  GUI_EDIT *guiEditor(pGuiCtx, title, dirName, fileName, pPrtCtx,
*                                        addExtrasFn, addExtrasArg)
*            void addExtrasFn(pGuiCtx, pEdit, panel, pX,pY,pHt, addExtrasArg)
*       void guiEditorNewEntry_pb(item)
*       void guiEditorShowCF(pEditor)
*       void guiEditorShowCF_mi(menu, menuItem)
*       void guiEditorShowCF_xvo(item)
*      char *guiFileSelect(pGuiCtx, title, dir, file, dim, callbackFn, pArg)
*               void callbackFn(pArg, newPath, newDir, newFileName);
*      Frame guiFrame(label, x, y, width, height, >ppDisp, >pServer)
*      Frame guiFrameCmd(label, parentFrame, pPanel, resizeProc)
*      char *guiGetNameFromSeln(pGuiCtx, textSw, headFlag, tailFlag)
*       Icon guiIconCreate(frame, iconBits)
*      Panel guiInit(pGuiCtx, pArgc, argv, label, x, y, width, height)
*       Menu guiMenu(proc, key1, val1, key2, val2)
*  Menu_item guiMenuItem(label, menu, proc, inact, dflt,
*                                      key1, val1, key2, val2)
* Panel_item guiMessage(label, panel, <>pX, pY, <>pHt)
*       void guiNotice(pGuiCtx, msg)
*       void guiNoticeFile(pGuiCtx, msg, fName)
*       void guiNoticeName(pGuiCtx, msg, name)
*   GUI_PRT *guiPrinter(pGuiCtx, parentFrame, title, useEnscript)
*       void guiPrinterShowCF_mi(menu, menuItem)
*       void guiPrinterShowCF_xvo(Xv_opaque)
*       void guiPrintFile(pPrtCtx, path)
*       void guiShellCmd(pGuiCtx, cmdBuf, clientNum, callbackFn, callbackArg)
*               void callbackFn(callbackArg, resultBuf)
* Panel_item guiTextField(label, panel, <>pX, pY, <>pHt, proc, nStr, nDsp,
*                                      key1, val1, key2, val2)
*      char *guiTextswGets(textsw, pBuf, dim, <>pCharPos)
*        int guiTextswPuts(textsw, pBuf, <>pCharPos)
*       void guiTextswReset(textsw)
*
* NOTES
* 1.	Many guiXxx routines have <>pX,pY,<>pHt as arguments.  pX,pY specifies
*	the desired position, in pixels, of the item within the panel.  pX
*	is modified to be a suitable x position for an item following on
*	the same line.  pHt is modified to be the height, in pixels, of the
*	item, if its height exceeds the present value of pHt.
*
* BUGS
* o	although they provide some isolation, these routines are still heavily
*	XView specific
* o	there is no "signal handler" routine to allow trapping a window
*	manager forced exit
*
* USAGE
*	In the synopsis for each routine, the `pGuiCtx' is intended to be
*	&guiContext, (or whatever name is chosen for the GUI_CTX in the
*	program).
*
* EXAMPLE PROGRAM
*	This example shows some of the features of these routines.  It
*	produces a program which can be used as a computerized logbook.
*	In addition to an editor and a "directory lister", the program
*	has a button which starts a new logbook entry with time, date,
*	and user name.
*
*	This example assumes that the icon editor has been used to make
*	an icon for the program.
*
* #include <stdio.h>
* #include <xview/xview.h>
* #include <xview/frame.h>
* #include <xview/panel.h>
* #include <xview/icon.h>
* #include <guiSubr.h>
*
* /*--------------------------------------------------------------------------
* *     This is a "context" structure for the program.  It is used to
* *     organize information about the gui operations.
* *-------------------------------------------------------------------------*/
* typedef struct {
*     GUI_CTX   guiCtx;     /* context block for the gui routines */
*     GUI_EDIT  *pEditor;   /* context block for the guiEditor */
*     GUI_PRT   *pPrt;      /* context block for the guiPrinter */
* } TEST_CTX;
* 
* /*--------------------------------------------------------------------------
* *     Some identifiers for XV_KEY_DATA arguments are usually needed.
* *     The "user" identifiers can start at values of 100 and above.
* *-------------------------------------------------------------------------*/
* typedef enum {
*     PCTX=100,             /* TEST context */
*     PCMD,                 /* TEST shell command string */
* } TEST_XV_KEY_DATA;
* 
* void edit_pb();
* void quit_pb();
* void extra();
* 
* /*--------------------------------------------------------------------------
* *   The main program
* *-------------------------------------------------------------------------*/
* main(argc, argv)
* int   argc;
* char  *argv[];
* {
*     TEST_CTX  testCtx;
*     static short icon_bits[]={
* #include "guiTest.icon"   /* this file was created with the icon editor */
*     };
*     Panel     panel;
*     int       y=5, x=5, ht=0;
* 
*     panel = guiInit(&testCtx.guiCtx,
*                    &argc, argv, "gui test program", 100, 100, 0,0);
*     guiIconCreate(testCtx.guiCtx.baseFrame, icon_bits);
* 
*     testCtx.pEditor = NULL;
*     testCtx.pPrt = guiPrinter(&testCtx.guiCtx,
*                    testCtx.guiCtx.baseFrame, "test: printer setup", 1);
* 
*     guiButton("Edit", panel, &x,&y,&ht, edit_pb, NULL, PCTX,&testCtx,0,NULL);
*     guiButton("Quit", panel, &x,&y,&ht, quit_pb, NULL, PCTX,&testCtx,0,NULL);
* 
*     window_fit(panel);
*     window_fit(testCtx.guiCtx.baseFrame);
*     xv_main_loop(testCtx.guiCtx.baseFrame);
*     return 0;
* }
*
* /*--------------------------------------------------------------------------
* *   subroutine to handle the "Edit" button.  This routine asks guiEditor
* *   to call the extra() function below to add a "New entry" button to
* *   the editor panel.
* *
* *   Common practice is to use suffixes on subroutine names when they
* *   are called as a result of activating various gui items.  This allows
* *   signifying what sort of arguments are needed.
* *       _pb indicates a routine called from a panel button
* *           void subr_pb(item)
* *           Panel_item item;
* *           {
* *               program statements
* *           }
* *       _mi indicates a routine called from a menu item
* *           void subr_mi(menu, item)
* *           Menu      menu;
* *           Menu_item item;
* *           {
* *               program statements
* *           }
* *       _xvo indicates a generic "one argument" callback routine
* *           void subr_xvo(item)
* *           Xv_opaque item;
* *           {
* *               program statements
* *           }
* *-------------------------------------------------------------------------*/
* static void edit_pb(item)
* Panel_item item;
* {   TEST_CTX  *pCtx=(TEST_CTX *)xv_get(item, XV_KEY_DATA, PCTX);
*     if (pCtx->pEditor == NULL) {
*         pCtx->pEditor = guiEditor(&pCtx->guiCtx, "test",
*                        pCtx->guiCtx.pwd, NULL, pCtx->pPrt, extra, pCtx);
*     }
* 
*     guiEditorShowCF(pCtx->pEditor);
* }
*
* void extra(pGuiCtx, pEditor, panel, pX,pY,pHt, pCtx)
* GUI_CTX  *pGuiCtx;
* GUI_EDIT *pEditor;
* Panel    panel;
* int      *pX,*pY,*pHt;
* TEST_CTX *pCtx;
* {
*     *pX = 5; *pY += *pHt + 10; *pHt = 0;
*     guiButton("New entry", panel, pX,pY,pHt, guiEditorNewEntry_pb, NULL,
*                    GUI_PCTX, pGuiCtx, GUI_PEDIT, pEditor);
* }
*
* /*--------------------------------------------------------------------------
* *   subroutine to handle the "Quit" button.  This is where various
* *   cleanup activities would normally occur.
* *-------------------------------------------------------------------------*/
* static void quit_pb(item)
* Panel_item item;
* {   TEST_CTX  *pCtx=(TEST_CTX *)xv_get(item, XV_KEY_DATA, PCTX);
* 
*     xv_destroy_safe(pCtx->guiCtx.baseFrame);
* }
*
*-***************************************************************************/
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>	/* for use with notifier */
#include <sys/wait.h>	/* for use with notifier */
#include <signal.h>	/* for use with notifier */
#include <sys/fcntl.h>

#if defined XWINDOWS
#   include <xview/xview.h>
#   include <xview/frame.h>
#   include <xview/notice.h>
#   include <xview/notify.h>
#   include <xview/openmenu.h>
#   include <xview/panel.h>
#   include <xview/textsw.h>
#   include <xview/seln.h>
#   include <xview/svrimage.h>
#   include <xview/icon.h>
#endif

#include <guiSubr.h>
#include <tsDefs.h>

void guiShellCmd_work();
Notify_value	guiCmdRead(), guiCmdSigchld();

static int	callbackNum=-1;
struct callbackStruct {
    Notify_client clientNum;
    void	(*callbackFn)();
    void	*callbackArg;
    char	result[GUI_TDIM];
    int		readDone;
    int		sigChldDone;
    int		pipe_io;
};
static struct callbackStruct callback[100];

#if GUI_TEST
typedef struct {
    GUI_CTX	guiCtx;
    GUI_EDIT	*pEditor;
    GUI_PRT	*pPrt;
} TEST_CTX;

typedef enum {
    PCTX=100,		/* TEST context */
    PCMD,		/* TEST shell command string */
} TEST_XV_KEY_DATA;

void edit_pb();
void quit_pb();
void extra();

/*+/subr**********************************************************************
* NAME	main - test program
*
*-*/
main(argc, argv)
int	argc;
char	*argv[];
{
    TEST_CTX	testCtx;
    static short icon_bits[]={
#include "guiTest.icon"
    };
    Panel	panel;
    int		y=5, x=5, ht=0;

    panel = guiInit(&testCtx.guiCtx,
			&argc, argv, "gui test program", 100, 100, 0,0);
    guiIconCreate(testCtx.guiCtx.baseFrame, icon_bits);

    testCtx.pEditor = NULL;
    testCtx.pPrt = guiPrinter(&testCtx.guiCtx,
		testCtx.guiCtx.baseFrame, "test: printer setup", 1);

    guiButton("Edit", panel, &x,&y,&ht, edit_pb, NULL, PCTX,&testCtx,0,NULL);
    guiButton("Quit", panel, &x,&y,&ht, quit_pb, NULL, PCTX,&testCtx,0,NULL);

    window_fit(panel);
    window_fit(testCtx.guiCtx.baseFrame);
    xv_main_loop(testCtx.guiCtx.baseFrame);
    return 0;
}
static void edit_pb(item)
Panel_item item;
{   TEST_CTX	*pCtx=(TEST_CTX *)xv_get(item, XV_KEY_DATA, PCTX);
    if (pCtx->pEditor == NULL) {
	pCtx->pEditor = guiEditor(&pCtx->guiCtx,
		"test", pCtx->guiCtx.pwd, "test", pCtx->pPrt, extra, pCtx);
    }

    guiEditorShowCF(pCtx->pEditor);
}

void extra(pGuiCtx, pEditor, panel, pX,pY,pHt, pCtx)
GUI_CTX	*pGuiCtx;
GUI_EDIT *pEditor;
Panel	panel;
int	*pX,*pY,*pHt;
TEST_CTX *pCtx;
{
    *pX = 5; *pY += *pHt + 10; *pHt = 0;
    guiButton("New entry", panel, pX,pY,pHt, guiEditorNewEntry_pb, NULL,
    GUI_PCTX, pGuiCtx, GUI_PEDIT, pEditor);
}
static void quit_pb(item)
Panel_item item;
{   TEST_CTX	*pCtx=(TEST_CTX *)xv_get(item, XV_KEY_DATA, PCTX);

    xv_destroy_safe(pCtx->guiCtx.baseFrame);
}
#endif


/*+/subr**********************************************************************
* NAME	guiBrowser - create a browsing text subwindow
*
* DESCRIPTION
*
* RETURNS
*
*
*-*/
Textsw
guiBrowser(parentFrame, panel, nCol, nLines, fileName)
Frame	parentFrame;	/* I parent of text subwindow */
Panel	panel;		/* I panel preceding textsw */
int	nCol;		/* I number of columns wide for text */
int	nLines;		/* I number of lines long for text */
char	*fileName;	/* I file to load, or NULL */
{
    Textsw	textsw;

    textsw = (Textsw)xv_create(parentFrame, TEXTSW,
	XV_X, 0, WIN_BELOW, panel,
	WIN_COLUMNS, nCol, WIN_ROWS, nLines,
	TEXTSW_BROWSING,	TRUE,
	TEXTSW_DISABLE_CD,	TRUE,
	TEXTSW_DISABLE_LOAD,	TRUE,
	TEXTSW_FILE,		fileName,
	NULL);
    if (textsw == NULL) {
	(void)printf("error creating browser text subwindow\n");
	exit(1);
    }
    xv_set(textsw, WIN_WIDTH, WIN_EXTEND_TO_EDGE, NULL);

    return textsw;
}

/*+/subr**********************************************************************
* NAME	guiButton - create a button on a panel
*
* DESCRIPTION
*
* RETURNS
*	Panel_item handle
*
*-*/
Panel_item
guiButton(label, panel, pX, pY, pHt, proc, menu, key1, val1, key2, val2)
char	*label;		/* I label for button */
Panel	panel;		/* I handle of panel containing button */
int	*pX;		/* IO pointer to x position in panel, in pixels */
int	*pY;		/* I pointer to y position in panel, in pixels */
int	*pHt;		/* IO ptr to height used, in pixels, or NULL */
void	(*proc)();	/* I pointer to procedure to handle button push */
Menu	menu;		/* I handle of button menu, or NULL */
enum	key1;		/* I key for context information for object */
void	*val1;		/* I value associated with key */
enum	key2;		/* I key for context information for object */
void	*val2;		/* I value associated with key */
{
    Panel_item	button;
    int		height;

    button = (Panel_item)xv_create(panel, PANEL_BUTTON,
		PANEL_LABEL_STRING, label, XV_X, *pX, XV_Y, *pY, NULL);
    if (button == NULL) {
	(void)printf("error creating \"%s\" button\n", label);
	exit(1);
    }
    if (proc != NULL) {
	if (xv_set(button, PANEL_NOTIFY_PROC, proc, NULL) != XV_OK) {
	    (void)printf("error adding proc to \"%s\" button\n", label);
	    exit(1);
	}
    }
    if (menu != NULL) {
	if (xv_set(button, PANEL_ITEM_MENU, menu, NULL) != XV_OK) {
	    (void)printf("error adding menu to \"%s\" button\n", label);
	    exit(1);
	}
    }
    if (key1 != 0) {
	if (xv_set(button, XV_KEY_DATA, key1, val1, NULL) != XV_OK) {
	    (void)printf("error adding key1 to \"%s\" button\n", label);
	    exit(1);
	}
    }
    if (key2 != 0) {
	if (xv_set(button, XV_KEY_DATA, key2, val2, NULL) != XV_OK) {
	    (void)printf("error adding key2 to \"%s\" button\n", label);
	    exit(1);
	}
    }
    *pX += (int)xv_get(button, XV_WIDTH) + 10;
    if (pHt != NULL) {
	height = xv_get(button, XV_HEIGHT);
	if (height > *pHt)
	    *pHt = height;
    }

    return button;
}

/*+/subr**********************************************************************
* NAME	guiButtonCenter - center one or two buttons within a panel
*
* DESCRIPTION
*
* RETURNS
*	void
*
* EXAMPLE
*	Panel_item button1, button2;
*
*	button1 = guiButton(...
*	button2 = guiButton(...
*
*	window_fit(panel)
*	guiButtonCenter(button1, button2, panel);
*
*-*/
void
guiButtonCenter(button1, button2, panel)
Panel_item button1;	/* I first button */
Panel_item button2;	/* I second button, or NULL */
Panel	panel;		/* I panel containing button(s) */
{
    int		width1, width2, x, panelWidth, y;
    panelWidth = (int)xv_get(panel, XV_WIDTH);
    width1 = (int)xv_get(button1, XV_WIDTH);
    y = (int)xv_get(button1, XV_Y);
    if (button2 != NULL) {
	width2 = (int)xv_get(button2, XV_WIDTH);
	if ((x = (panelWidth - width1 - width2 - 20) / 2) > 0) {
	    xv_set(button1, XV_X, x, XV_Y, y, NULL);
	    x += width1 + 20;
	    xv_set(button2, XV_X, x, XV_Y, y, NULL);
	}
    }
    else if ((x = (panelWidth - width1) / 2) > 0)
	xv_set(button1, XV_X, x, XV_Y, y, NULL);
}

/*+/subr**********************************************************************
* NAME	guiCF - command frame dismiss and show routines
*
* DESCRIPTION
*
* RETURNS
*
* BUGS
* o	text
*
* SEE ALSO
*	guiFrameCmd
*
* EXAMPLES
*	guiButton("Dismiss", panel, ...
*			guiCFdismiss_xvo, NULL, GUI_CF, cmdFrame, 0, NULL);
*
*	menu = guiMenu(..., GUI_PCTX, pGuiCtx...);
*	menuItem = guiMenuItem("label", menu, guiCFshow_mi, ...,
*							GUI_CF, frame...);
*
*	guiButton("Show...", panel, ..., guiCFshow_xvo, ...,
*					 GUI_PCTX, pGuiCtx, GUI_CF, frame);
*
*	menu = guiMenu(..., GUI_PCTX, pGuiCtx...);
*	menuItem = guiMenuItem("label", menu, guiCFshowPin_mi, ...,
*							GUI_CF, frame...);
*
*	guiButton("Show...", panel, ..., guiCFshowPin_xvo, ...,
*					 GUI_PCTX, pGuiCtx, GUI_CF, frame);
*
*-*/
void
guiCFdismiss_xvo(item)
Xv_opaque item;
{   Frame	frame=(Frame)xv_get(item, XV_KEY_DATA, GUI_CF);
    xv_set(frame, XV_SHOW, FALSE, FRAME_CMD_PUSHPIN_IN, FALSE, NULL);
}
void
guiCFshow_mi(menu, menuItem)
Menu	menu;
Menu_item menuItem;
{   GUI_CTX	*pGuiCtx=(GUI_CTX *)xv_get(menu, XV_KEY_DATA, GUI_PCTX);
    Frame	frame=(Frame)xv_get(menuItem, XV_KEY_DATA, GUI_CF);
    xv_set(frame, XV_SHOW, TRUE, NULL);
}
void
guiCFshowPin_mi(menu, menuItem)
Menu	menu;
Menu_item menuItem;
{   GUI_CTX	*pGuiCtx=(GUI_CTX *)xv_get(menu, XV_KEY_DATA, GUI_PCTX);
    Frame	frame=(Frame)xv_get(menuItem, XV_KEY_DATA, GUI_CF);
    xv_set(frame, XV_SHOW, TRUE, FRAME_CMD_PUSHPIN_IN, TRUE, NULL);
}
void
guiCFshow_xvo(item)
Xv_opaque item;
{   GUI_CTX	*pGuiCtx=(GUI_CTX *)xv_get(item, XV_KEY_DATA, GUI_PCTX);
    Frame	frame=(Frame)xv_get(item, XV_KEY_DATA, GUI_CF);
    xv_set(frame, XV_SHOW, TRUE, NULL);
}
void
guiCFshowPin_xvo(item)
Xv_opaque item;
{   GUI_CTX	*pGuiCtx=(GUI_CTX *)xv_get(item, XV_KEY_DATA, GUI_PCTX);
    Frame	frame=(Frame)xv_get(item, XV_KEY_DATA, GUI_CF);
    xv_set(frame, XV_SHOW, TRUE, FRAME_CMD_PUSHPIN_IN, TRUE, NULL);
}

/*+/subr**********************************************************************
* NAME	guiCheckbox - create an empty checkbox list
*
* DESCRIPTION
*	Creates the framework for an empty checkbox selection list.  The
*	actual checkbox items must be filled in by calls to
*	guiCheckboxItem.
*
*	Checkbox lists have a fixed number of items--the number of items
*	can't be increased or decreased after the empty list is created.
*	Individual items on a list can be changed at any time, however.
*
* RETURNS
*	Panel_item for the checkbox
*
* BUGS
* o	list can't dynamically grow and shrink
* o	the maximum size for a checkbox list is 10 items
*
* SEE ALSO
*	guiCheckboxItem
*
* EXAMPLE
*	Set up a 3-item checkbox.  Each of the first two items is to have
*	an associated status message.  The appearance of the setup will
*	resemble the following:
*
* Retrieve from:
* [] text file       ________status message________
* [] binary file     ________status message________
* [] Channel Access
*
*	The third item will be the default.  When any of the items are
*	selected, choose_xvo() is to be called; that routine uses a pointer
*	to a `myStruct' structure for its operations.
*
*    struct myStruct {
*	Panel_item	textStatus;	status for text file
*	Panel_item	binStatus;	status for binary file
*    } context;
*#define PCTX 101                 (a unique integer for identifying myStruct)
*    Panel_item	ckbx;
*    int	x=5, y=5, ht=0;
*    int	x1, x2, y1, y2;   (for aligning buttons and status messages)
*
*    ckbx = guiCheckbox("Retrieve from:", panel, 3, &x, &y, &ht,
*					choose_xvo, PCTX, &context, 0, NULL);
*    y += ht + 5; x = 5; ht = 0;
*    guiCheckboxItem("text file", ckbx, 0, 0, &x, &y, &ht);
*    x1 = x; y1 =y;
*    y += ht + 5; x = 5; ht = 0;
*    guiCheckboxItem("binary file", ckbx, 1, 0, &x, &y, &ht);
*    x2 = x; y2 =y;
*    y += ht + 5; x = 5; ht = 0;
*    guiCheckboxItem("Channel Access", ckbx, 2, 1, &x, &y, &ht);
*    y += ht + 5; x = 5; ht = 0;
*    if (x1 < x2)	x1 = x2;	(which extends further right?)
*    else		x2 = x1;
*    context.textStatus = guiMessage("", panel, &x1, &y1, &ht);
*    context.binStatus = guiMessage("", panel, &x2, &y2, &ht);
*
*-*/
Panel_item
guiCheckbox(label, panel, nItems, pX, pY, pHt, proc, key1, val1, key2, val2)
char	*label;		/* I label for checkbox list */
Panel	panel;		/* I handle of panel containing checkbox list */
int	nItems;		/* I the number of items to be held by the list */
int	*pX;		/* IO pointer to x position in panel, in pixels */
int	*pY;		/* I pointer to y position in panel, in pixels */
int	*pHt;		/* IO ptr to height used by label, in pixels, or NULL */
void	(*proc)();	/* I pointer to procedure to handle making a choice */
enum	key1;		/* I key for context information for object */
void	*val1;		/* I value associated with key */
enum	key2;		/* I key for context information for object */
void	*val2;		/* I value associated with key */
{
    Panel_item	checkbox;
    int		height;

/*-----------------------------------------------------------------------------
* NOTE:  the kludgey way this is done is because XView in SunOS 4.1 can't
*	seem to handle defining the number of items except at compile time
*----------------------------------------------------------------------------*/
#define GUI_CHK checkbox=xv_create(panel,PANEL_CHECK_BOX,PANEL_CHOICE_STRINGS,
    if (nItems == 1)	   GUI_CHK "", NULL,NULL);
    else if (nItems == 2)  GUI_CHK "","", NULL,NULL);
    else if (nItems == 3)  GUI_CHK "","","", NULL,NULL);
    else if (nItems == 4)  GUI_CHK "","","","", NULL,NULL);
    else if (nItems == 5)  GUI_CHK "","","","","", NULL,NULL);
    else if (nItems == 6)  GUI_CHK "","","","","","", NULL,NULL);
    else if (nItems == 7)  GUI_CHK "","","","","","","", NULL,NULL);
    else if (nItems == 8)  GUI_CHK "","","","","","","","", NULL,NULL);
    else if (nItems == 9)  GUI_CHK "","","","","","","","","", NULL,NULL);
    else if (nItems == 10) GUI_CHK "","","","","","","","","","", NULL,NULL);
    else {
	(void)printf("guiCheckbox can only handle up to 10 items\n");
	exit(1);
    }
    if (checkbox == NULL) {
	(void)printf("error creating \"%s\"s checkbox list\n", label);
	exit(1);
    }
    xv_set(checkbox,
	PANEL_LAYOUT,		PANEL_VERTICAL,
	PANEL_CHOOSE_ONE,	TRUE,
	PANEL_CHOOSE_NONE,	TRUE,
	NULL);
    xv_set(checkbox, PANEL_VALUE, -1, NULL);
    if (proc != NULL) {
	if (xv_set(checkbox, PANEL_NOTIFY_PROC, proc, NULL) != XV_OK) {
	    (void)printf("error adding proc to \"%s\"s checkbox list\n", label);
	    exit(1);
	}
    }
    if (key1 != 0) {
	if (xv_set(checkbox, XV_KEY_DATA, key1, val1, NULL) != XV_OK) {
	    (void)printf("error adding key1 to \"%s\" checkbox list\n", label);
	    exit(1);
	}
    }
    if (key2 != 0) {
	if (xv_set(checkbox, XV_KEY_DATA, key2, val2, NULL) != XV_OK) {
	    (void)printf("error adding key2 to \"%s\" checkbox list\n", label);
	    exit(1);
	}
    }
    guiMessage(label, panel, pX, pY, pHt);
    return checkbox;
}

/*+/subr**********************************************************************
* NAME	guiCheckboxItem - create an item in a checkbox list
*
* DESCRIPTION
*	Change one of the items in a checkbox list.  This may be either
*	setting the item following the creation of the list with guiCheckbox,
*	or a subsequent change to an item.
*
* RETURNS
*	void
*
* SEE ALSO
*	guiCheckbox
*
*-*/
void
guiCheckboxItem(label, checkbox, itemNum, dfltFlag, pX, pY, pHt)
char	*label;		/* I label for item */
Panel_item checkbox;	/* I handle of checkbox list */
int	itemNum;	/* I number (starting with 0) within checkbox list */
int	dfltFlag;	/* I if 1, then this item will be pre-selected */
int	*pX;		/* IO pointer to x position in panel, in pixels */
int	*pY;		/* I pointer to y position in panel, in pixels */
int	*pHt;		/* IO ptr to height used by label, in pixels, or NULL */
{
    Panel	panel=(Panel)xv_get(checkbox, PANEL_PARENT_PANEL);

    xv_set(checkbox,
	PANEL_CHOICE_X,		itemNum, *pX,
	PANEL_CHOICE_Y,		itemNum, *pY-5,
	NULL);
    *pX += 40;
    *pHt = 20;
    guiMessage(label, panel, pX, pY, pHt);
    if (dfltFlag)
	xv_set(checkbox, PANEL_VALUE, itemNum, NULL);
}

void guiEditorCancelFile_pb();
void guiEditorCreateFile_pb();
char *guiEditorGetPath();
void guiEditorLoadFile();
void guiEditorLoadFile_xvo();
void guiEditorPickFile_pb();
void guiEditorPick_callback();
void guiEditorPrintFile_mi();
void guiEditorSaveFile_pb();
void guiEditorUpdateSet();
void guiEditorUpdateRst();
void guiEditorUpdate_xvo();

/*+/subr**********************************************************************
* NAME	guiEditor - create a text editor command frame
*
* DESCRIPTION
*	Creates a text editor command frame with amenities to support good
*	editing practice:
*	o   allow specifying directory and file
*	o   support easy access to a list of directory contents
*	o   easy choice of file name from directory list
*	o   "locking" of file under edit, with corresponding notification
*	    to users who try to edit a locked file
*	o   support for caller to add items to the text editor panel
*	o   check for write permission before allowing editing
*
*	If the caller specifies a file name, then that file will be loaded
*	automatically into the text subwindow.  (If the file doesn't
*	exist, a notice will pop up.)
*
*	If the caller specifies a function to be called to add items to
*	the editor panel, the function will be called as follows:
*
*          void addExtrasFn(pGuiCtx, pEdit, panel, pX,pY,pHt, addExtrasArg)
*
* RETURNS
*	GUI_EDIT *, or
*	NULL
*
* BUGS
* o	doesn't allow printing "memory" copy of file, and insists that user
*	save the file if it has been modified
* o	doesn't provide a way to save updates if the program aborts
*
*-*/
GUI_EDIT *
guiEditor(pGuiCtx, title, dir, file, pPrtCtx, addExtrasFn, arg)
GUI_CTX	*pGuiCtx;	/* I pointer to gui context */
char	*title;		/* I text for command frame title bar */
char	*dir;		/* I directory of file to load, or NULL */
char	*file;		/* I file to load, or NULL */
GUI_PRT	*pPrtCtx;	/* I pointer to printer context, or NULL */
void	(*addExtrasFn)();/* I pointer to function to add extra stuff to
				editor panel, or NULL*/
void	*arg;		/* I argument to pass to addExtrasFn */
{
    GUI_EDIT	*pEdit;
    Panel	panel;
    Textsw	textsw;
    Panel_item	ck;
    int		y=5, x=5, ht=0, ht1;

/*-----------------------------------------------------------------------------
* Dir: [<]___________[>]  File: [<]_____________________[>]  List dir
* Create  Load  [] read  []edit  Save  Cancel  Print v
*                                                    print
*                                                    setup...
* ..................status message..............................
*----------------------------------------------------------------------------*/

    if ((pEdit=(GUI_EDIT *)malloc(sizeof(GUI_EDIT))) == NULL) {
	return NULL;
    }
    bzero((char *)pEdit, sizeof(*pEdit));
    pEdit->pGuiCtx = pGuiCtx;
    pEdit->pPrtCtx = pPrtCtx;
    pEdit->edit_CF = guiFrameCmd(title, pGuiCtx->baseFrame, &panel, NULL);
    pEdit->loaded = 0;
    pEdit->updFlag = 0;
    strcpy(pEdit->title, title);

    pEdit->dir_PT = guiTextField("Dir:", panel, &x,&y,&ht,
		NULL, GUI_TDIM-1, 30, 0, NULL, 0, NULL);
    pEdit->file_PT = guiTextField("File:", panel, &x,&y,&ht,
		NULL, GUI_TDIM-1, 20, 0, NULL, 0, NULL);
    guiButton("List dir...", panel, &x, &y, &ht, guiEditorPickFile_pb, NULL,
		    GUI_PCTX, pGuiCtx, GUI_PEDIT, pEdit);
    xv_set(pEdit->dir_PT, PANEL_VALUE, dir, NULL);
    xv_set(pEdit->file_PT, PANEL_VALUE, file, NULL);
    y += ht + 5; x = 5; ht = 0;

    guiButton("Create", panel, &x, &y, &ht, guiEditorCreateFile_pb, NULL,
		    GUI_PCTX, pGuiCtx, GUI_PEDIT, pEdit);
    guiButton("Load", panel, &x, &y, &ht, guiEditorLoadFile_xvo, NULL,
		    GUI_PCTX, pGuiCtx, GUI_PEDIT, pEdit);
    ck = pEdit->update_PCB = xv_create(panel, PANEL_CHECK_BOX,
		XV_X, x, XV_Y, y,
		PANEL_LAYOUT,		PANEL_HORIZONTAL,
		PANEL_CHOICE_STRINGS,	"read    ", "edit", NULL,
		PANEL_VALUE,		0,
		PANEL_CHOOSE_ONE,	TRUE,
		PANEL_NOTIFY_PROC,	guiEditorUpdate_xvo,
		XV_KEY_DATA,		GUI_PCTX, pGuiCtx,
		XV_KEY_DATA,		GUI_PEDIT, pEdit,
		NULL);
    x += (int)xv_get(ck, XV_WIDTH) + 10;
    ht1 = xv_get(ck, XV_HEIGHT);
    if (ht1 > ht)
	ht = ht1;
    guiButton("Save", panel, &x, &y, &ht, guiEditorSaveFile_pb, NULL,
		    GUI_PCTX, pGuiCtx, GUI_PEDIT, pEdit);
    guiButton("Cancel", panel, &x, &y, &ht, guiEditorCancelFile_pb, NULL,
		    GUI_PCTX, pGuiCtx, GUI_PEDIT, pEdit);
    if (pPrtCtx != NULL) {
	Menu	menu;
	menu = guiMenu(NULL, 0, NULL, 0, NULL);
	guiMenuItem("Print file", menu, guiEditorPrintFile_mi, 0,1,
		    GUI_PEDIT, pEdit, GUI_PPRT, pEdit->pPrtCtx);
	guiMenuItem("Printer setup...", menu, guiPrinterShowCF_mi, 0,0,
		    GUI_PPRT, pEdit->pPrtCtx, 0, NULL);
	guiButton("Print", panel, &x, &y, &ht, NULL, menu, 0, NULL, 0, NULL);
    }

    if (addExtrasFn != NULL)
	addExtrasFn(pGuiCtx, pEdit, panel, &x,&y,&ht, arg);

    y += ht + 5; x = 5; ht = 0;

    window_fit(panel);
    xv_set(panel, WIN_WIDTH, WIN_EXTEND_TO_EDGE, NULL);
    pEdit->text_TSW = (Textsw)xv_create(pEdit->edit_CF, TEXTSW,
	XV_X, 0, WIN_BELOW, panel,
	WIN_COLUMNS, 80, WIN_ROWS, 40,
	TEXTSW_BROWSING,	TRUE,
	TEXTSW_DISABLE_CD,	TRUE,
	TEXTSW_DISABLE_LOAD,	TRUE,
	NULL);
    if (pEdit->text_TSW == NULL) {
	(void)printf("error creating editor text subwindow\n");
	exit(1);
    }

    guiEditorGetPath(pEdit, 0);
    xv_set(pEdit->text_TSW, WIN_WIDTH, WIN_EXTEND_TO_EDGE, NULL);
    window_fit(pEdit->edit_CF);
    if (file != NULL && file[0] != '\0')
	guiEditorLoadFile(pEdit);
    return pEdit;
}

/*+/internal******************************************************************
* NAME	guiEditorCancelFile_pb
*
*-*/
static void
guiEditorCancelFile_pb(item)
Panel_item item;
{   GUI_EDIT	*pEdit=(GUI_EDIT *)xv_get(item, XV_KEY_DATA, GUI_PEDIT);

    if (pEdit->loaded == 0)
	return;
    textsw_reset(pEdit->text_TSW, 0, 0);
    guiEditorUpdateRst(pEdit);
    guiEditorLoadFile(pEdit);
}

/*+/internal******************************************************************
* NAME	guiEditorCreateFile_pb
*
*-*/
static void
guiEditorCreateFile_pb(item)
Panel_item item;
{   GUI_EDIT	*pEdit=(GUI_EDIT *)xv_get(item, XV_KEY_DATA, GUI_PEDIT);
    FILE	*pFile;
    int		fd;

    if (pEdit->loaded)
	guiEditorUpdateRst(pEdit);
    if (pEdit->updFlag)
	return;
    if (guiEditorGetPath(pEdit, 1) == NULL)
	return;
    if ((pFile = fopen(pEdit->path, "r")) != NULL) {
	fclose(pFile);
	guiNoticeFile(pEdit->pGuiCtx, "file already exists:", pEdit->path);
	return;
    }
    if ((fd = open(pEdit->path, O_WRONLY|O_CREAT, 0664)) < 0) {
	guiNoticeFile(pEdit->pGuiCtx, "couldn't create file:", pEdit->path);
	return;
    }
    close(fd);
    textsw_reset(pEdit->text_TSW, 0, 0);
    pEdit->loaded = 1;
    guiEditorUpdateSet(pEdit);
}

/*+/internal******************************************************************
* NAME	guiEditorGetPath
*
*-*/
static char *
guiEditorGetPath(pEdit, notice)
GUI_EDIT *pEdit;
int	notice;		/* 1 if want you want a notice issued on problems */
{
    char	*name, *dir;

    dir = (char *)xv_get(pEdit->dir_PT, PANEL_VALUE);
    name = (char *)xv_get(pEdit->file_PT, PANEL_VALUE);
    strcpy(pEdit->file, name);
    strcpy(pEdit->dir, dir);
    if (name[0] == '\0') {
	if (notice)
	    guiNotice(pEdit->pGuiCtx, "no file name specified");
	return NULL;
    }
    strcpy(pEdit->path, dir);
    if (strlen(pEdit->path) > 0)
	strcat(pEdit->path, "/");
    strcat(pEdit->path, name);
    strcpy(pEdit->file, name);
    strcpy(pEdit->dir, dir);
    return pEdit->path;
}

/*+/internal******************************************************************
* NAME	guiEditorLoadFile_xvo
*
*-*/
static void
guiEditorLoadFile_xvo(item)
Xv_opaque item;
{   GUI_EDIT	*pEdit=(GUI_EDIT *)xv_get(item, XV_KEY_DATA, GUI_PEDIT);
    guiEditorLoadFile(pEdit);
}
/*+/internal******************************************************************
* NAME	guiEditorLoadFile
*
*-*/
static void
guiEditorLoadFile(pEdit)
GUI_EDIT *pEdit;
{
    Textsw_status status;

    if ((int)xv_get(pEdit->text_TSW, TEXTSW_MODIFIED)) {
	xv_set(pEdit->update_PCB, PANEL_VALUE, 1, NULL);
	guiNotice(pEdit->pGuiCtx, "text has been modified; use Save or Cancel");
	return;
    }
    if (guiEditorGetPath(pEdit, 1) == NULL)
	return;
    if (pEdit->updFlag) {
	guiEditorUpdateRst(pEdit);
	pEdit->updFlag = 0;
    }
    xv_set(pEdit->text_TSW, TEXTSW_STATUS, &status,
				TEXTSW_FILE, pEdit->path, NULL);
    if (status == TEXTSW_STATUS_OKAY) {
	guiEditorUpdateRst(pEdit);
	pEdit->loaded = 1;
    }
    else {
	guiNoticeFile(pEdit->pGuiCtx, "couldn't open file", pEdit->path);
	pEdit->loaded = 0;
    }
}

/*+/subr**********************************************************************
* NAME	guiEditorNewEntry_pb
*
* DESCRIPTION
*
* RETURNS
*	void
*
*               void addExtrasFn(pGuiCtx, panel, pX,pY,pHt, addExtrasArg)
*
* EXAMPLE
*	Add an extra button to a text editor to allow adding a new entry
*	(as for a "log book") to a file.  The new entry will have the form:
*
*		== Mar 06, 1992 07:27:31  userName ==
*
*
*	TEST_CTX	*pCtx;
*	GUI_CTX		*pGui;
*	GUI_EDIT	*pEdit;
*	void		extra();
*
*	pEdit = guiEditor(pGui, ".", NULL, NULL, extra, pCtx);
*
*
*
* void extra(pGuiCtx, pEditor, panel, pX,pY,pHt, pCtx)
* GUI_CTX  *pGuiCtx;
* GUI_EDIT *pEditor;
* Panel    panel;
* int      *pX,*pY,*pHt;
* TEST_CTX *pCtx;
* {
*     *pX = 5; *pY += *pHt + 10; *pHt = 0;
*     guiButton("New entry", panel, pX,pY,pHt, guiEditorNewEntry_pb, NULL,
*                    GUI_PCTX, pGuiCtx, GUI_PEDIT, pEditor);
* }
*-*/
void
guiEditorNewEntry_pb(item)
Panel_item item;
{   GUI_EDIT	*pEdit=(GUI_EDIT *)xv_get(item, XV_KEY_DATA, GUI_PEDIT);
    Textsw	tsw=pEdit->text_TSW;
    TS_STAMP	nowTs;
    char	now[32];
    int		i;

    if (!pEdit->loaded) {
	guiNotice(pEdit->pGuiCtx, "no file has been loaded");
	return;
    }
    if (xv_get(pEdit->update_PCB, PANEL_VALUE) == 0) {
	guiNotice(pEdit->pGuiCtx, "edit mode must be selected");
	return;
    }
    xv_set(tsw, TEXTSW_INSERTION_POINT, TEXTSW_INFINITY, NULL);
    textsw_possibly_normalize(tsw,
		(Textsw_index)xv_get(tsw, TEXTSW_INSERTION_POINT));
    tsLocalTime(&nowTs);
    tsStampToText(&nowTs, TS_TEXT_MONDDYYYY, now);
    textsw_insert(tsw, "\n== ", 4);
    textsw_insert(tsw, now, 21);	/* don't print fractions of sec */
    textsw_insert(tsw, "  ", 2);
    textsw_insert(tsw, pEdit->pGuiCtx->user, strlen(pEdit->pGuiCtx->user));
    textsw_insert(tsw, " ==", 3);
}

/*+/internal******************************************************************
* NAME	guiEditorPickFile_pb
*
*-*/
static void
guiEditorPickFile_pb(item)
Panel_item item;
{   GUI_CTX	*pGuiCtx=(GUI_CTX *)xv_get(item, XV_KEY_DATA, GUI_PCTX);
    GUI_EDIT	*pEdit=(GUI_EDIT *)xv_get(item, XV_KEY_DATA, GUI_PEDIT);
    char	title[GUI_TDIM*2];

    sprintf(title, "%s: %s", pEdit->title, "List dir");
    guiEditorGetPath(pEdit, 0);
    guiFileSelect(pGuiCtx, title, pEdit->dir, pEdit->file,
		GUI_TDIM, guiEditorPick_callback, pEdit);
}

/*+/internal******************************************************************
* NAME	guiEditorPick_callback
*
*-*/
static void
guiEditorPick_callback(pEdit, newPath, newDir, newFileName)
GUI_EDIT *pEdit;
char	*newPath;
char	*newDir;
char	*newFileName;
{
    xv_set(pEdit->dir_PT, PANEL_VALUE, newDir, NULL);
    xv_set(pEdit->file_PT, PANEL_VALUE, newFileName, NULL);
    guiEditorLoadFile(pEdit);
}

/*+/internal******************************************************************
* NAME	guiEditorPrintFile_mi
*
*-*/
static void
guiEditorPrintFile_mi(menu, item)
Menu	menu;
Menu_item item;
{   GUI_EDIT	*pEdit=(GUI_EDIT *)xv_get(item, XV_KEY_DATA, GUI_PEDIT);

    if (!pEdit->loaded) {
	guiNotice(pEdit->pGuiCtx, "no file has been loaded");
	return;
    }
    if ((int)xv_get(pEdit->text_TSW, TEXTSW_MODIFIED)) {
	guiNotice(pEdit->pGuiCtx, "file is modified; save before printing");
	return;
    }
    guiPrintFile(pEdit->pPrtCtx, pEdit->path);
}

/*+/internal******************************************************************
* NAME	guiEditorSaveFile_pb
*
*-*/
static void
guiEditorSaveFile_pb(item)
Panel_item item;
{   GUI_EDIT	*pEdit=(GUI_EDIT *)xv_get(item, XV_KEY_DATA, GUI_PEDIT);
    unsigned	stat;

    if (!pEdit->loaded) {
	guiNotice(pEdit->pGuiCtx, "no file has been loaded");
	return;
    }
    if ((int)xv_get(pEdit->text_TSW, TEXTSW_MODIFIED)) {
	if (guiEditorGetPath(pEdit, 1) != NULL) {
	    stat = textsw_store_file(pEdit->text_TSW, pEdit->path, 0, 0);
	    if (stat != 0) {
		guiNoticeFile(pEdit->pGuiCtx,"unable to save in:",pEdit->path);
		return;
	    }
	}
	guiEditorUpdateRst(pEdit);
    }
}

#if 0
/*+/macros********************************************************************
* NAME	guiEditorShowCF - show the command frame containing the editor
*
* DESCRIPTION
*	Make an editor command frame visible.  There are several versions
*	of this routine available, to accomodate the different methods of
*	calling it:
*
*	void guiEditorShowCF(pEdit)		call directly
*		GUI_EDIT *pEdit;	/* I pointer to editor context */
*
*	void guiEditorShowCF_mi(menu, item)	call from a menu item
*		Menu	menu;		/* I menu handle */
*		Menu_item item;		/* I menu item handle--must have
*					XV_KEY_DATA of GUI_PEDIT, pEdit */
*
*	void guiEditorShowCF_xvo(item)	call from a button item
*		Xv_opaque item;		/* I item handle--must have
*					XV_KEY_DATA of GUI_PEDIT, pEdit */
*-*/
#endif
void
guiEditorShowCF(pEdit)
GUI_EDIT *pEdit;	/* I pointer to editor context */
{
    xv_set(pEdit->edit_CF, XV_SHOW, TRUE, FRAME_CMD_PUSHPIN_IN, TRUE, NULL);
}
void
guiEditorShowCF_mi(menu, menuItem)
Menu	menu;
Menu_item menuItem;
{   GUI_EDIT *pEdit=(GUI_EDIT *)xv_get(menuItem, XV_KEY_DATA, GUI_PEDIT);
    guiEditorShowCF(pEdit);
}
void
guiEditorShowCF_xvo(item)
Xv_opaque item;
{   GUI_EDIT *pEdit=(GUI_EDIT *)xv_get(item, XV_KEY_DATA, GUI_PEDIT);
    guiEditorShowCF(pEdit);
}

/*+/internal******************************************************************
* NAME	guiEditorUpdate_xvo
*
*-*/
void guiEditorUpdate_xvo(item, value)
Xv_opaque item;
int	value;
{   GUI_EDIT	*pEdit=(GUI_EDIT *)xv_get(item, XV_KEY_DATA, GUI_PEDIT);
    if (value < 0)
	return;
    else if (value == 0)
	guiEditorUpdateRst(pEdit);
    else if (value == 1) {
	guiEditorUpdateSet(pEdit);
    }
}

/*+/internal******************************************************************
* NAME	guiEditorUpdateRst
*
*-*/
void guiEditorUpdateRst(pEdit)
GUI_EDIT *pEdit;
{
    FILE	*pFile;

    if ((int)xv_get(pEdit->text_TSW, TEXTSW_MODIFIED)) {
	xv_set(pEdit->update_PCB, PANEL_VALUE, 1, NULL);
	guiNotice(pEdit->pGuiCtx, "text has been modified; use Save or Cancel");
	return;
    }
    xv_set(pEdit->text_TSW, TEXTSW_BROWSING, TRUE, NULL);
    xv_set(pEdit->update_PCB, PANEL_VALUE, 0, NULL);
    if (pEdit->updFlag == 1) {
	pEdit->updFlag = 0;
	if ((pFile = fopen(pEdit->lockFile, "w+")) == NULL) {
	    guiNoticeFile(pEdit->pGuiCtx, "couldn't open lock file",
			pEdit->lockFile);
	    xv_set(pEdit->update_PCB, PANEL_VALUE, 0, NULL);
	}
	fwrite("unlocked", 9, 1, pFile);
	fclose(pFile);
	unlink(pEdit->lockFile);
    }
}

/*+/internal******************************************************************
* NAME	guiEditorUpdateSet
*
*-*/
void static
guiEditorUpdateSet(pEdit)
GUI_EDIT *pEdit;
{
    Textsw	tsw=pEdit->text_TSW;
    int		top;
    char	lockFile[2*GUI_TDIM + 5];
    FILE	*pFile;
    char	msgLock[2*GUI_TDIM];
    char	msgWho[2*GUI_TDIM];
    int		noticeVal;
    TS_STAMP	ts;
    char	tsText[32];

    if (pEdit->updFlag == 1) {
	xv_set(pEdit->update_PCB, PANEL_VALUE, 1, NULL);
	guiEditorUpdateRst(pEdit);
	return;
    }
    if (!pEdit->loaded) {
	guiNotice(pEdit->pGuiCtx, "no file has been loaded");
	guiEditorUpdateRst(pEdit);
	return;
    }

    if ((pFile = fopen(pEdit->path, "r+")) == NULL) {
	guiNoticeFile(pEdit->pGuiCtx, "file isn't writeable:", pEdit->file);
	guiEditorUpdateRst(pEdit);
	return;
    }
    fclose(pFile);

    top = (int)xv_get(tsw, TEXTSW_FIRST_LINE);
    top += 2;
    strcpy(lockFile, pEdit->path);
    strcat(lockFile, ".lock");
    if ((pFile = fopen(lockFile, "r+")) != NULL) {
	fread(msgWho, GUI_TDIM, 1, pFile);
	fclose(pFile);
	if (strcmp(msgWho, "unlocked") != 0) {
	    sprintf(msgLock, "%s is locked:", pEdit->file);
	    noticeVal = notice_prompt(pEdit->pGuiCtx->baseFrame, NULL,
		    NOTICE_MESSAGE_STRINGS,	msgLock, msgWho,
			"you can cancel your update or force an unlock", NULL,
		    NOTICE_BUTTON_YES,	"Cancel",
		    NOTICE_BUTTON_NO,	"Unlock",
		    NULL);
	    if (noticeVal == NOTICE_YES) {
		guiEditorUpdateRst(pEdit);
		return;
	    }
	}
    }
    if ((pFile = fopen(lockFile, "w+")) == NULL) {
	guiNoticeFile(pEdit->pGuiCtx, "couldn't open lock file", lockFile);
	guiEditorUpdateRst(pEdit);
	return;
    }
    tsLocalTime(&ts);
    tsStampToText(&ts, TS_TEXT_MONDDYYYY, tsText);
    sprintf(msgWho, "by:%s  on:%s  at:%.21s",
			pEdit->pGuiCtx->user, pEdit->pGuiCtx->host, tsText);
    fwrite(msgWho, strlen(msgWho)+1, 1, pFile);
    fclose(pFile);
    xv_set(tsw, TEXTSW_FIRST_LINE, top);
    xv_set(tsw, TEXTSW_BROWSING, FALSE, NULL);
    xv_set(pEdit->update_PCB, PANEL_VALUE, 1, NULL);
    pEdit->updFlag = 1;
    strcpy(pEdit->lockFile, lockFile);
}


void guiFileSelCom_pb();
void guiFileSelLs_pb();
void guiFileSelLsDone();

/*+/subr**********************************************************************
* NAME	guiFileSelect - select a file from a group
*
* DESCRIPTION
*	Present a list of files in a directory and allow the user to
*	choose one of them.  Support is given for changing directory.
*
*	Only one file select command frame can be active at a given time--
*	each call to this routine will result in wrapping up any
*	previous call to it.
*
*	The callback routine will be called if the user requests a change
*	in file path.  The callback routine is called with 4 arguments:
*
*		callback(pArg, newPath, newDir, newFileName);
*
* BUGS
* o	doesn't support wildcard lists (e.g., ls *.c)
*
*-*/
void guiFileSelect(pGuiCtx, title, dir, file, dim, callback, pArg)
GUI_CTX	*pGuiCtx;	/* I pointer to gui context */
char	*title;		/* I title for command frame title bar */
char	*dir;		/* I directory */
char	*file;		/* I file name */
int	dim;		/* I dimension of the dir & file arrays */
void	(*callback)();	/* I routine to call if user changes file/dir */
void	*pArg;		/* I argument to pass to callback routine */
{
    Frame	frame;
    Panel	panel;
    int		x=5, y=5, ht=0;
    char	message[GUI_TDIM*3];

    pGuiCtx->fsCallFn = callback;
    pGuiCtx->fsCallArg = pArg;

    if (pGuiCtx->fsFrame != NULL) {
	frame = pGuiCtx->fsFrame;
	xv_set(frame, FRAME_LABEL, title, NULL);
    }
    else {
	frame = guiFrameCmd(title, pGuiCtx->baseFrame, &panel, NULL);
	xv_set(frame, XV_KEY_DATA, GUI_PCTX, pGuiCtx, NULL);
	pGuiCtx->fsFrame = frame;
	guiButton("List", panel, &x,&y,&ht, guiFileSelLs_pb, NULL,
			GUI_PCTX, pGuiCtx, 0, NULL);
	pGuiCtx->fsDir_PT = guiTextField("Directory:",
	    panel, &x,&y,&ht, NULL, dim-1, 59, GUI_PCTX, pGuiCtx, 0, NULL);
	y += ht + 10; x = 5; ht = 0;
	guiButton("Load file", panel, &x,&y,&ht,
			guiFileSelCom_pb, NULL, GUI_PCTX, pGuiCtx, 0, NULL);
	pGuiCtx->fsFile_PT = guiTextField("File:",
	    panel, &x,&y,&ht, NULL, dim-1, 59, GUI_PCTX, pGuiCtx, 0, NULL);
	y += ht + 10; x = 5; ht = 0;
	pGuiCtx->fsStatus_PM = guiMessage(" ", panel, &x, &y, &ht);
	y += ht + 5; x = 5; ht = 0;

	window_fit(panel);
	pGuiCtx->fsTextsw = guiBrowser(frame, panel, 60, 30, NULL);
    }
    xv_set(pGuiCtx->fsDir_PT, PANEL_VALUE, dir, NULL);
    xv_set(pGuiCtx->fsFile_PT, PANEL_VALUE, file, NULL);
    window_fit(frame);
    xv_set(frame, XV_SHOW, TRUE, FRAME_CMD_PUSHPIN_IN, TRUE, NULL);
    if (dir[0] == '\0') {
	guiNotice(pGuiCtx, "directory must be specified");
	return;
    }
    sprintf(pGuiCtx->fsLsFile, "/tmp/guiFileSel%d.ls.tmp", getpid());
    unlink(pGuiCtx->fsLsFile);
    sprintf(message, "cd %s; ls -l >%s", dir, pGuiCtx->fsLsFile);
    guiShellCmd_work(pGuiCtx, message, GUI_FILE_SEL_CLIENT,
					guiFileSelLsDone, pGuiCtx, 0);
    sprintf(message, "processing names in %s", dir);
    xv_set(pGuiCtx->fsStatus_PM, PANEL_LABEL_STRING, message, NULL);
}

/*+/internal******************************************************************
* NAME	guiFileSelCom_pb
*
*-*/
static void
guiFileSelCom_pb(item)
Panel_item item;
{   GUI_CTX	*pGuiCtx=(GUI_CTX *)xv_get(item, XV_KEY_DATA, GUI_PCTX);
    char	*pSel, *dir, *file, path[240];

/*-----------------------------------------------------------------------------
*	if there's selected text, use it for file name.  If it's in the
*	window listing the files, get the file name from the end of the
*	line containing the selected text.
*----------------------------------------------------------------------------*/
    pSel = guiGetNameFromSeln(pGuiCtx, pGuiCtx->fsTextsw, 0, 1);
    if (pSel != NULL)
	xv_set(pGuiCtx->fsFile_PT, PANEL_VALUE, pSel, NULL);

    dir = (char *)xv_get(pGuiCtx->fsDir_PT, PANEL_VALUE);
    file = (char *)xv_get(pGuiCtx->fsFile_PT, PANEL_VALUE);
    if (strlen(file) > 0) {
	sprintf(path, "%s/%s", dir, file);
	pGuiCtx->fsCallFn(pGuiCtx->fsCallArg, path, dir, file);
#if 0
	xv_set(pGuiCtx->fsFrame,
		XV_SHOW, FALSE, FRAME_CMD_PUSHPIN_IN, FALSE, NULL);
#endif
    }
    else
	guiNotice(pGuiCtx, "file name is blank; select file or use Cancel");
}

/*+/internal******************************************************************
* NAME	guiFileSelLs_pb
*
*-*/
static void
guiFileSelLs_pb(item)
Panel_item item;
{   GUI_CTX	*pGuiCtx=(GUI_CTX *)xv_get(item, XV_KEY_DATA, GUI_PCTX);
    char	message[GUI_TDIM];
    char	*dir=(char *)xv_get(pGuiCtx->fsDir_PT, PANEL_VALUE);
    unlink(pGuiCtx->fsLsFile);
    sprintf(message, "cd %s; ls -l >%s", dir, pGuiCtx->fsLsFile);
    guiShellCmd_work(pGuiCtx, message, GUI_FILE_SEL_CLIENT,
					guiFileSelLsDone, pGuiCtx, 0);
    sprintf(message, "processing names in %s", dir);
    xv_set(pGuiCtx->fsStatus_PM, PANEL_LABEL_STRING, message, NULL);
}

/*+/internal******************************************************************
* NAME	guiFileSelLsDone
*
*-*/
static void
guiFileSelLsDone(pGuiCtx)
GUI_CTX	*pGuiCtx;
{
    char	*dir=(char *)xv_get(pGuiCtx->fsDir_PT, PANEL_VALUE);
    char	message[GUI_TDIM];
    xv_set(pGuiCtx->fsTextsw, TEXTSW_FILE, pGuiCtx->fsLsFile, NULL);
    unlink(pGuiCtx->fsLsFile);
    if ((int)xv_get(pGuiCtx->fsTextsw, TEXTSW_LENGTH) <= 0)
	sprintf(message, "there are no files in %s", dir);
    else
	sprintf(message, "size and modification date are shown");
    xv_set(pGuiCtx->fsStatus_PM, PANEL_LABEL_STRING, message, NULL);
}

/*+/subr**********************************************************************
* NAME	guiFrame - create a base frame
*
* DESCRIPTION
*	Create a base frame.  The frame will have have the specified label
*	in its title bar, as well as for its icon.  The frame will have
*	resize corners.  If height and width are specified, they will
*	determine the size of the frame; otherwise, it will have a
*	default size.
*
*	If pointers are specified in the call, then the X11 display and
*	the XView server are passed back to the caller.
*
* RETURNS
*	Frame handle
*
* SEE ALSO
*	guiFrameCmd, guiIconCreate
*-*/
Frame
guiFrame(label, x, y, width, height,ppDisp, pServer)
char	*label;		/* I label for frame and icon */
int	x;		/* I x coordinate for frame, in pixels */
int	y;		/* I y coordinate for frame, in pixels */
int	width;		/* I width of frame, in pixels, or 0 */
int	height;		/* I height of frame, in pixels, or 0 */
Display **ppDisp;	/* O pointer to X display pointer, or NULL */
Xv_server *pServer;	/* O pointer to xview server handle, or NULL */
{
    Icon	icon;
    Frame	frame;

    icon = (Icon)xv_create(NULL, ICON, ICON_LABEL, label, NULL);
    if (icon == NULL) {
	(void)printf("error creating \"%s\" icon\n", label);
	exit(1);
    }
    frame = (Frame)xv_create(NULL, FRAME, FRAME_LABEL, label, FRAME_ICON, icon,
	WIN_X, x, WIN_Y, y, NULL);
    if (frame == NULL) {
	(void)printf("error creating \"%s\" frame\n", label);
	exit(1);
    }
    if (width > 0)	xv_set(frame, XV_WIDTH, width, NULL);
    if (height > 0)	xv_set(frame, XV_HEIGHT, height, NULL);
    if (ppDisp != NULL)
	*ppDisp = (Display *)xv_get(frame, XV_DISPLAY);
    if (pServer != NULL)
	*pServer = (Xv_server)xv_get(xv_get(frame, XV_SCREEN), SCREEN_SERVER);
    
    return frame;
}

/*+/subr**********************************************************************
* NAME	guiFrameCmd - create a command frame
*
* DESCRIPTION
*	Create a command frame and an associated panel.
*
*	If a procedure is specified for handling resize, then the frame
*	is set to receive resize events.  (The frame will have resize
*	corners regardless of whether a procedure is specified.)
*
* RETURNS
*	Frame handle
*
* NOTES
* 1.	For command frames with text subwindows, the resize procedure can
*	be omitted, and the text subwindow will be automatically resized
*	when the command frame is resized.
*
*-*/
Frame
guiFrameCmd(label, parentFrame, pPanel, resizeProc)
char	*label;		/* I label for command frame */
Frame	parentFrame;	/* I parent frame for command frame */
Panel	*pPanel;	/* O ptr to default panel of command frame, or NULL */
void	(*resizeProc)();/* I function to handle resize, or NULL */
{
    Frame	frame;

    if (resizeProc == NULL) {
	frame = (Frame)xv_create(parentFrame, FRAME_CMD, FRAME_LABEL, label,
				FRAME_SHOW_RESIZE_CORNER, TRUE,
				XV_SHOW, FALSE, NULL);
    }
    else {
	frame = (Frame)xv_create(parentFrame, FRAME_CMD, FRAME_LABEL, label,
				FRAME_SHOW_RESIZE_CORNER, TRUE,
				WIN_EVENT_PROC,		resizeProc,
				WIN_CONSUME_EVENTS,	WIN_RESIZE, NULL,
				XV_SHOW, FALSE, NULL);
    }
    if (frame == NULL) {
	(void)printf("error creating \"%s\" command frame\n", label);
	exit(1);
    }
    if (pPanel != NULL) {
	*pPanel = (Panel)xv_get(frame, FRAME_CMD_PANEL);
	if (*pPanel == NULL) {
	    (void)printf("error getting default panel for \"%s\"\n", label);
	    exit(1);
	}
	xv_set(*pPanel, WIN_WIDTH, WIN_EXTEND_TO_EDGE, NULL);
    }
    return frame;
}

/*+/subr**********************************************************************
* NAME	guiGetNameFromSeln - get a name from a line of selected text
*
* DESCRIPTION
*	Examines the selected text to extract a name to return to the caller.
*	A `name' is considered to be a string of characters delimited by
*	white space.
*
*	If the selected text is within the caller's text subwindow, then
*	the entire line containing the selection is treated as being
*	selected.
*
*	The actual action taken depends on the arguments and whether the
*	selected text is in the caller's text subwindow:
*
*	in textsw  head  tail            pointer is returned to:
*	---------  ----  ----  -----------------------------------------------
*	  yes       0     0    entire line containing selection
*	  yes       1     0    first word of line containing selection
*	  yes       0     1    last word of line containing selection
*	   no       0     0    selected text
*	   no       1     0    selected text
*	   no       0     1    selected text
*
* RETURNS
*	char * to name
*
* BUGS
* o	this routine isn't reentrant
* o	if the caller wants a permanent copy of the name, it must be copied
*	prior to the next call to guiGetNameFromSeln
*
*-*/
char *
guiGetNameFromSeln(pGuiCtx, textSw, headFlag, tailFlag)
GUI_CTX	*pGuiCtx;	/* I pointer to gui context */
Textsw	textSw;		/* I handle to text subwindow to treat special */
int	headFlag;	/* I 1 to return only the head of the line */
int	tailFlag;	/* I 1 to return only the tail of the line */
{
    Seln_holder	holder;
    Seln_request *pResponse;
    static char	buf[GUI_TDIM*2];
    int		bufLen, i;
    char	*pSel;

    holder = selection_inquire(pGuiCtx->server, SELN_PRIMARY);
    pResponse = selection_ask(pGuiCtx->server, &holder,
	SELN_REQ_CONTENTS_ASCII, NULL,
	NULL);
    pSel = pResponse->data + sizeof(SELN_REQ_CONTENTS_ASCII);
    bufLen = strlen(pSel);
    if (bufLen == 0)
	pSel = NULL;	/* no selection */
    else {
	if (seln_holder_same_client(&holder, textSw)) {
	    pResponse = selection_ask(pGuiCtx->server, &holder,
		SELN_REQ_FAKE_LEVEL,	SELN_LEVEL_LINE,
		SELN_REQ_CONTENTS_ASCII, NULL,
		NULL);
	    pSel = pResponse->data;
	    pSel += sizeof(SELN_REQ_FAKE_LEVEL);
	    pSel += sizeof(SELN_LEVEL_LINE);
	    pSel += sizeof(SELN_REQ_CONTENTS_ASCII);
	    bufLen = strlen(pSel);
	    if (bufLen > sizeof(buf)-1) {
		guiNotice(pGuiCtx, "too much text selected");
		return (char *)NULL;
	    }
	    strcpy(buf, pSel);

	    if (headFlag) {
/*-----------------------------------------------------------------------------
*	skip leading blanks, then look for the first white space; return
*	what's in between to the caller
*----------------------------------------------------------------------------*/
		pSel = buf;
		while (*pSel == ' ') {
		    pSel++;		/* skip leading blanks */
		    bufLen--;
		}
		i = 0;
		while (i < bufLen-1) {
		    if (isspace(pSel[i]))
			break;
		    i++;
		}
		pSel[i] = '\0';
	    }
	    else if (tailFlag) {
/*-----------------------------------------------------------------------------
*	looking backward from end, find the first white space; return the
*	"tail" of the line to the caller
*----------------------------------------------------------------------------*/
		i = bufLen - 1;
		while (i > 0) {
		    if (buf[i] == '\n')
			buf[i] = '\0';
		    else if (isspace(buf[i])) {
			i++;
			break;
		    }
		    i--;
		}
		pSel = &buf[i];
	    }
	}
    }
    return pSel;
}

/*+/subr**********************************************************************
* NAME	guiIconCreate - create an icon to attach to a frame
*
* DESCRIPTION
*	Using the bit image for an icon, creates the internal structure
*	needed to draw the icon.  The icon is then associated with the
*	specified frame.
*
* RETURNS
*	Icon handle
*
* BUGS
* o	assumes an icon size of 64x64
*
* EXAMPLE
*	Associate the icon in "../src/myIcon.icon" with the base frame.
*	(myIcon.icon was produced with the OpenLook "iconedit" program.)
*
* #include <xview/icon.h>
*
*	myIcon_bits[]={
* #include "../src/myIcon.icon"
*	};
*
*	Icon	myIcon;
*
*	myIcon = (Icon)guiIconCreate(pGuiCtx->baseFrame, myIcon_bits);
*
*-*/
Icon
guiIconCreate(frame, iconBits)
Frame	frame;		/* I handle for frame to receive the icon */
short	*iconBits;	/* I array of bits for the icon (64 by 64 assuemd) */
{
    Server_image icon_image;
    Icon	icon;

    icon_image = (Server_image)xv_create(NULL, SERVER_IMAGE,
		XV_WIDTH,		64,
		XV_HEIGHT,		64,
		SERVER_IMAGE_BITS, iconBits,
		NULL);
    icon = (Icon)xv_create(frame, ICON,
		ICON_IMAGE, icon_image, NULL);
    xv_set(frame, FRAME_ICON, icon, NULL);

    return icon;
}

/*+/subr**********************************************************************
* NAME	guiInit - initialize the gui routines
*
* DESCRIPTION
*	Sets up for gui operations, including:
*	o   connecting to X11
*	o   assimilating X11 options from the command line
*	o   creating a base frame
*	o   creating a panel within the base frame
*	o   initializing the gui context block with:
*		.user		user name
*		.host		host name (or X server name if different)
*		.pwd		working directory
*		.baseFrame	Xview Frame of main program window
*		.pDisp		the X Display pointer
*		.server		the Xview server handle
*
* RETURNS
*	Panel handle
*
*-*/
Panel
guiInit(pGuiCtx, pArgc, argv, label, x, y, width, height)
GUI_CTX	*pGuiCtx;	/* I pointer to gui context */
int     *pArgc;		/* I pointer to number of command line args */
char	*argv[];	/* I command line args */
char	*label;		/* I label for frame and icon */
int	x;		/* I x coordinate for frame, in pixels */
int	y;		/* I y coordinate for frame, in pixels */
int	width;		/* I width of frame, in pixels, or 0 */
int	height;		/* I height of frame, in pixels, or 0 */
{
    void guiInitHostname();
    char	*user, *pwd;

    xv_init(XV_INIT_ARGC_PTR_ARGV, pArgc, argv, 0);
    pGuiCtx->baseFrame = guiFrame(label, x, y, width, height,
		&pGuiCtx->pDisp, &pGuiCtx->server);
    pGuiCtx->fsFrame = NULL;
    strcpy(pGuiCtx->host, pGuiCtx->pDisp->display_name);
    pwd = getenv("PWD");
    strncpy(pGuiCtx->pwd, pwd, GUI_TDIM-1);
    user = getenv("USER");
    strncpy(pGuiCtx->user, user, GUI_TDIM-1);
    if (pGuiCtx->host[0] == '\0' || pGuiCtx->host[0] == ':' ||
			strncmp(pGuiCtx->host, "unix", 4) == 0) {
	guiShellCmd_work(pGuiCtx, "hostname", GUI_HOSTNAME_CLIENT,
			guiInitHostname, pGuiCtx, 1);
    }

    return (Panel)xv_create(pGuiCtx->baseFrame, PANEL,
	    XV_X, 0, XV_Y, 0, PANEL_LAYOUT, PANEL_HORIZONTAL, NULL);
}
void guiInitHostname(pGuiCtx, result)
GUI_CTX	*pGuiCtx;
char	*result;
{
    int		i;
    if (result[0] != '\0') {
	strcpy(pGuiCtx->host, result);
	i = strlen(result);
	if (result[i-1] == '\n')
	    pGuiCtx->host[i-1] = '\0';
    }
}

/*+/subr**********************************************************************
* NAME	guiMenu - create a menu
*
* DESCRIPTION
*
* RETURNS
*	Menu handle
*
*-*/
Menu
guiMenu(proc, key1, val1, key2, val2)
void	(*proc)();	/* I pointer to procedure to handle menu selection */
enum	key1;		/* I key for context information for object */
void	*val1;		/* I value associated with key */
enum	key2;		/* I key for context information for object */
void	*val2;		/* I value associated with key */
{
    Menu	menu;

    menu = (Menu)xv_create(NULL, MENU, NULL);
    if (menu == NULL) {
	(void)printf("error creating menu\n");
	exit(1);
    }
    if (proc != NULL) {
	if (xv_set(menu, MENU_NOTIFY_PROC, proc, NULL) != XV_OK) {
	    (void)printf("error adding proc to menu\n");
	    exit(1);
	}
    }
    if (key1 != 0) {
	if (xv_set(menu, XV_KEY_DATA, key1, val1, NULL) != XV_OK) {
	    (void)printf("error adding key1 to menu\n");
	    exit(1);
	}
    }
    if (key2 != 0) {
	if (xv_set(menu, XV_KEY_DATA, key2, val2, NULL) != XV_OK) {
	    (void)printf("error adding key2 to menu\n");
	    exit(1);
	}
    }

    return menu;
}

/*+/subr**********************************************************************
* NAME	guiMenuItem - add a menu item to a menu
*
* DESCRIPTION
*
* RETURNS
*	Menu_item handle
*
*-*/
Menu_item
guiMenuItem(label, menu, proc, inact, dflt, key1, val1, key2, val2)
char	*label;		/* I label for menu item */
Menu	menu;		/* I menu on which to add item */
void	(*proc)();	/* I pointer to procedure to handle menu selection */
int	inact;		/* I 0,1 for item active,inactive */
int	dflt;		/* I 0,1 for item isn't,is the default */
enum	key1;		/* I key for context information for object */
void	*val1;		/* I value associated with key */
enum	key2;		/* I key for context information for object */
void	*val2;		/* I value associated with key */
{
    Menu_item	menuItem;

    menuItem = (Menu_item)xv_create(NULL, MENUITEM,
				    MENU_STRING, label, MENU_RELEASE, NULL);
    if (menuItem == NULL) {
	(void)printf("error creating \"%s\" menu item\n", label);
	exit(1);
    }
    if (xv_set(menu, MENU_APPEND_ITEM, menuItem, NULL) != XV_OK) {
	(void)printf("error adding \"%s\" menu item to menu\n", label);
	exit(1);
    }
    if (proc != NULL) {
	if (xv_set(menuItem, MENU_NOTIFY_PROC, proc, NULL) != XV_OK) {
	    (void)printf("error adding proc to \"%s\" menu item\n", label);
	    exit(1);
	}
    }
    if (inact) {
	if (xv_set(menuItem, MENU_INACTIVE, TRUE, NULL) != XV_OK) {
	    (void)printf("error setting \"%s\" menu item inactive\n", label);
	    exit(1);
	}
    }
    if (dflt) {
	if (xv_set(menu, MENU_DEFAULT_ITEM, menuItem, NULL) != XV_OK) {
	    (void)printf("error setting \"%s\" menu item default\n", label);
	    exit(1);
	}
    }
    if (key1 != 0) {
	if (xv_set(menuItem, XV_KEY_DATA, key1, val1, NULL) != XV_OK) {
	    (void)printf("error adding key1 to \"%s\" menu item\n", label);
	    exit(1);
	}
    }
    if (key2 != 0) {
	if (xv_set(menuItem, XV_KEY_DATA, key2, val2, NULL) != XV_OK) {
	    (void)printf("error adding key2 to \"%s\" menu item\n", label);
	    exit(1);
	}
    }

    return menuItem;
}

/*+/subr**********************************************************************
* NAME	guiMessage - create a text message object
*
* DESCRIPTION
*
* RETURNS
*	Panel_item handle to message
*
* BUGS
* o	text
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
Panel_item
guiMessage(label, panel, pX, pY, pHt)
char	*label;		/* I label for sageutton */
Panel	panel;		/* I handle of panel containing message */
int	*pX;		/* IO pointer to x position in panel, in pixels */
int	*pY;		/* I pointer to y position in panel, in pixels */
int	*pHt;		/* IO ptr to height of message, in pixels, or NULL */
{
    Panel_item	item;
    int		height;

    item = (Panel_item)xv_create(panel, PANEL_MESSAGE,
		PANEL_LABEL_STRING, label, XV_X, *pX, XV_Y, *pY, NULL);
    if (item == NULL) {
	(void)printf("error creating message object\n");
	exit(1);
    }
    *pX += (int)xv_get(item, XV_WIDTH) + 10;
    if (pHt != NULL) {
	height = xv_get(item, XV_HEIGHT);
	if (height > *pHt)
	    *pHt = height;
    }

    return item;
}

/*+/macro*********************************************************************
* NAME	guiNotice - display a pop-up notice
*
* DESCRIPTION
*	Display a pop-up notice with a "continue" button.  Several different
*	routines are available:
*
*	guiNotice(pGuiCtx, message)   simply displays the message
*
*	guiNoticeFile(pGuiCtx, message, filePath)   calls "perror()" to print
*			the Unix error message and file name to stdout, then
*			displays the message and file path in the pop-up
*
*	guiNoticeName(pGuiCtx, message, name)   displays the message and the
*			name (usually for a channel or file) in the pop-up
*
*	In each case, the first argument is a GUI_CTX * pointing to a
*	gui context block.  The other arguments are char * pointing to
*	text strings.
*
* RETURNS
*	void
*
*-*/
void
guiNotice(pGuiCtx, msg)
GUI_CTX	*pGuiCtx;	/* I pointer to gui context */
char	*msg;
{
    notice_prompt(pGuiCtx->baseFrame, NULL, NOTICE_MESSAGE_STRINGS, msg, NULL,
	    NOTICE_BUTTON_YES,	"Continue", NULL);
    return;
}
void
guiNoticeFile(pGuiCtx, msg, fName)
GUI_CTX	*pGuiCtx;	/* I pointer to gui context */
char	*msg;
char	*fName;
{
    perror(fName);
    notice_prompt(pGuiCtx->baseFrame, NULL,
	    NOTICE_MESSAGE_STRINGS, msg, fName, NULL,
	    NOTICE_BUTTON_YES,	"Continue", NULL);
    return;
}
void
guiNoticeName(pGuiCtx, msg, name)
GUI_CTX	*pGuiCtx;	/* I pointer to gui context */
char	*msg;
char	*name;
{
    char	myName[GUI_TDIM], *pName;
    if (strlen(name) < GUI_TDIM-3){
	sprintf(myName, "\"%s\"", name);
	pName = myName;
    }
    else
	pName = name;
    notice_prompt(pGuiCtx->baseFrame, NULL,
	    NOTICE_MESSAGE_STRINGS, msg, pName, NULL,
	    NOTICE_BUTTON_YES,	"Continue", NULL);
    return;
}

/*+/subr**********************************************************************
* NAME	guiPrinter - create a printer context and command frame
*
* DESCRIPTION
*	Create a printer context block and command frame for handling
*	printer operations.  The amenities provided include:
*
*	o   grabbing the PRINTER environment variable to set the default
*	    printer to use
*	o   allowing user to choose point size of font
*	o   allowing user to choose page orientation
*	o   allowing user to choose whether to use enscript or lpr for
*	    the printing operation
*
*	The printer context block can be used in the guiEditor call to
*	enable the "Print" button for the editor.
*
* RETURNS
*	GUI_PRT *, or
*	NULL
*
* SEE ALSO
*	guiPrintFile, guiPrinterShowCF_mi, guiPrinterShowCF_xvo
*	guiEditor
*
*-*/
GUI_PRT *
guiPrinter(pGuiCtx, parentFrame, title, useEnscript)
GUI_CTX	*pGuiCtx;	/* I pointer to gui context */
Frame	parentFrame;	/* I parent of editor command frame */
char	*title;		/* I text for command frame title bar */
int	useEnscript;	/* I 0,1 to use lpr,enscript for printing */
{
    GUI_PRT	*pPrt;
    Panel	panel;
    Panel_item	item;
    char	*printer;
    Panel_item	ck;
    int		y=10, x=5, ht=0;

/*-----------------------------------------------------------------------------
* Printer: ______ point size: __  v portrait   v enscript
*                                   landscape    lpr
* ..................status message..............................
*----------------------------------------------------------------------------*/

    if ((pPrt=(GUI_PRT *)malloc(sizeof(GUI_PRT))) == NULL) {
	return NULL;
    }
    bzero((char *)pPrt, sizeof(*pPrt));
    pPrt->pGuiCtx = pGuiCtx;
    pPrt->print_CF = guiFrameCmd(title, parentFrame, &panel, NULL);

    pPrt->printer_PT = guiTextField("Printer:", panel, &x,&y,&ht,
		NULL, 19, 8, 0, NULL, 0, NULL);
    xv_set(pPrt->printer_PT, PANEL_VALUE, getenv("PRINTER"), NULL);
    pPrt->point_PT = guiTextField("point size:", panel, &x,&y,&ht,
		NULL, 2, 2, 0, NULL, 0, NULL);
    xv_set(pPrt->point_PT, PANEL_VALUE, "8", NULL);
    pPrt->landscape_PCS = item = xv_create(panel, PANEL_CHOICE_STACK,
	PANEL_LAYOUT,		PANEL_VERTICAL,
	PANEL_CHOICE_STRINGS,	"portrait","landscape",NULL,
	PANEL_VALUE,	0,
	XV_X, x, XV_Y, y-5, NULL);
    x += (int)xv_get(item, XV_WIDTH) + 5;
    pPrt->prog_PCS = item = xv_create(panel, PANEL_CHOICE_STACK,
	PANEL_LAYOUT,		PANEL_VERTICAL,
	PANEL_CHOICE_STRINGS,	"enscript","lpr",NULL,
	PANEL_VALUE,	0,
	XV_X, x, XV_Y, y-5, NULL);
    x += (int)xv_get(item, XV_WIDTH) + 5;
    y += ht + 5; x = 5; ht = 0;

    window_fit(panel);
    window_fit(pPrt->print_CF);
    return pPrt;
}

/*+/subr**********************************************************************
* NAME	guiPrintFile
*
* DESCRIPTION
*	Print a file using the settings in the printer context block.
*
* RETURNS
*	void
*
* SEE ALSO
*	guiPrinter
*
*-*/
void
guiPrintFile(pPrtCtx, path)
GUI_PRT	*pPrtCtx;	/* I pointer to printer context */
char	*path;		/* I pathname of file to be printed */
{
    char	command[GUI_TDIM*3];
    char	mode[8];

    
    if ((int)xv_get(pPrtCtx->landscape_PCS, PANEL_VALUE) == 0)
	strcpy(mode, "");	/* portrait mode */
    else
	strcpy(mode, "-r");	/* rotate to landscape mode */
    if ((int)xv_get(pPrtCtx->prog_PCS, PANEL_VALUE) == 0) {
	sprintf(command, "enscript -fCourier%s -q -P%s %s %s",
		(char *)xv_get(pPrtCtx->point_PT, PANEL_VALUE),
		(char *)xv_get(pPrtCtx->printer_PT, PANEL_VALUE),
		mode, path);
	guiShellCmd(pPrtCtx->pGuiCtx, command, GUI_PRINT_CLIENT, NULL, NULL);
    }
    else {
	if (mode[0] != '\0')
	    guiNotice(pPrtCtx->pGuiCtx, "landscape is ignored when using lpr");
	sprintf(command, "lpr -P%s %s",
		(char *)xv_get(pPrtCtx->printer_PT, PANEL_VALUE), path);
	guiShellCmd(pPrtCtx->pGuiCtx, command, GUI_PRINT_CLIENT, NULL, NULL);
    }
}

/*+/macros********************************************************************
* NAME	guiPrinterShowCF_... - make printer command frame visible
*
* DESCRIPTION
*	Make the printer setup command frame visible, so that the user
*	can modify print parameters.
*
*	There are two forms of this routine.  The first is to be called
*	from a menu; the second is to be called from other objects, such
*	as a button.
*		guiPrinterShowCF_mi(menu, item);
*		guiPrinterShowCF_xvo(item);
*
* RETURNS
*	void
*
* SEE ALSO
*	guiPrinter, guiPrintFile
*
* EXAMPLE
*	Set up a button with a two-item menu--the first item will print
*	a file (whose name is in the program's context block), and the
*	second item will call this routine.  Make the first item the
*	default.
*
*    Menu    menu;
*    void    print_mi();
*
*    menu = guiMenu(NULL, 0, NULL, 0, NULL);
*    guiMenuItem("Print file", menu, print_mi, 0,1,
*                   PCTX, pCtx, GUI_PPRT, pEdit->pPrtCtx);
*    guiMenuItem("Printer setup...", menu, guiPrinterShowCF_mi, 0,0,
*                   GUI_PPRT, pEdit->pPrtCtx, 0, NULL);
*    guiButton("Print", panel, &x, &y, &ht, NULL, menu, 0, NULL, 0, NULL);
*
*
* void print_mi(menu, item)
* Menu        menu;
* Menu_item   item;
* {  GUI_PRT    *pPrt=(GUI_PRT *)xv_get(item, XV_KEY_DATA, GUI_PPRT);
*    TEST_CTX   *pCtx=(TEST_CTX *)xv_get(item, XV_KEY_DATA, PCTX);
*
*    guiPrintFile(pPrt, pCtx->file);
* }
*
*-*/
void
guiPrinterShowCF_mi(menu, item)
Menu	menu;
Menu_item item;
{   GUI_PRT	*pPrtCtx=(GUI_PRT *)xv_get(item, XV_KEY_DATA, GUI_PPRT);
    xv_set(pPrtCtx->print_CF, XV_SHOW, TRUE, NULL);
}

/*+/internal******************************************************************
* NAME	guiPrinterShowCF_xvo
*
*-*/
void
guiPrinterShowCF_xvo(item)
Xv_opaque item;
{   GUI_PRT	*pPrtCtx=(GUI_PRT *)xv_get(item, XV_KEY_DATA, GUI_PPRT);
    xv_set(pPrtCtx->print_CF, XV_SHOW, TRUE, NULL);
}

/*+/subr**********************************************************************
* NAME	guiShellCmd - issue a shell command, cooperating with the notifier
*
* DESCRIPTION
*	This routine issues a shell command under the auspices of the
*	xview notifier.  The command is issued following a fork, with a
*	popen call.  The critical issue is that the fork results in a
*	SIGCHLD signal which the notifier must handle.
*
*	The callback routine has the following form:
*		void callback(callbackArg, resultBuf)

*	resultBuf is a pointer to the text produced by the command (limited to
*	a single line)
*
* RETURNS
*	void
*
* BUGS
* o	when running under dbx, the child process always results in a
*	core dump
*
*-*/
void
guiShellCmd(pGuiCtx, cmdBuf, clientNum, callbackFn, callbackArg)
GUI_CTX	*pGuiCtx;	/* I pointer to gui context */
char	*cmdBuf;	/* I the command string to give the shell */
Notify_client clientNum;/* I a unique value in the range 100 to 200 to
				be used by guiCmdSigchld when the child
				is done processing the command */
void	(*callbackFn)();/* I pointer to function for guiCmdSigchld to call */
void	*callbackArg;	/* I arg to pass to callbackFn */
{
    guiShellCmd_work(pGuiCtx, cmdBuf, clientNum, callbackFn, callbackArg, 0);
}
void
guiShellCmd_execv_argv(pGuiCtx, argv, clientNum, callbackFn, callbackArg)
GUI_CTX	*pGuiCtx;	/* I pointer to gui context */
char	*argv;		/* I arguments to give the shell */
Notify_client clientNum;/* I a unique value in the range 100 to 200 to
				be used by guiCmdSigchld when the child
				is done processing the command */
void	(*callbackFn)();/* I pointer to function for guiCmdSigchld to call */
void	*callbackArg;	/* I arg to pass to callbackFn */
{
    guiShellCmd_work(pGuiCtx, argv, clientNum, callbackFn, callbackArg, 2);
}
void
guiShellCmd_work(pGuiCtx, cmdBuf, clientNum, callbackFn, callbackArg, mode)
GUI_CTX	*pGuiCtx;	/* I pointer to gui context */
char	*cmdBuf;	/* I the command string to give the shell (or
				pointer to argv list if mode==2)*/
Notify_client clientNum;/* I a unique value in the range 100 to 200 to
				be used by guiCmdSigchld when the child
				is done processing the command */
void	(*callbackFn)();/* I pointer to function for guiCmdSigchld to call */
void	*callbackArg;	/* I arg to pass to callbackFn */
int	mode;		/* I 0,1,2 for popen,execvp,execvp_with_argv */
{
    FILE	*fp;
    int		i, pid, cbNum;
    int		pipe_io[2][2];
    char	resultBuf[GUI_TDIM];
    char	*argv[20], *buf, *pBuf, dlm;

    cbNum = callbackNum;
    while (cbNum >= 0 && callback[cbNum].clientNum != clientNum)
	cbNum--;
    if (cbNum < 0) {
	if (++cbNum < 100) {
	    callbackNum = cbNum;
	    callback[cbNum].clientNum = clientNum;
	}
	else {
	    guiNotice(pGuiCtx, "guiShellCmd: overflow of client number table");
	    return;
	}
    }
    callback[cbNum].callbackFn = callbackFn;
    callback[cbNum].callbackArg = callbackArg;
    callback[cbNum].result[0] = '\0';
    callback[cbNum].readDone = 0;
    callback[cbNum].sigChldDone = 0;

/*-----------------------------------------------------------------------------
* blatantly stolen from "XView Programming Manual", Dan Heller, O'Reilly &
*	Associates, Inc., July 1990, pp.386-389; section 19.8.2, Reading and
*	Writing on Pipes; Example 19-4.  The ntfy_pipe.c program
*
*    child reads:  |<=========== pipe_io[0] <===========|  parent writes:
*  pipe_io[0][0]                                           pipe_io[0][1]
*
*   parent reads:  |<=========== pipe_io[1] <===========|  child writes:
*  pipe_io[1][0]                                           pipe_io[1][1]
*----------------------------------------------------------------------------*/

    if (mode == 1) {
	pBuf = buf = (char *)malloc(sizeof(cmdBuf));
	strcpy(pBuf, cmdBuf);
	for (i=0; i<19; i++) {
	    if (nextNonSpaceField(&pBuf, &argv[i], &dlm) <= 1)
		break;
	}
	argv[i] = NULL;
    }

    pipe(pipe_io[0]);
    pipe(pipe_io[1]);
    if ((pid = fork()) == -1) {
	perror("error executing shell command");
	close(pipe_io[0][0]);
	close(pipe_io[0][1]);
	close(pipe_io[1][0]);
	close(pipe_io[1][1]);
	if (mode == 1)
	    free(buf);
	return;
    }
    else if (pid == 0) {				/* child process */
	dup2(pipe_io[0][0], 0);		/* stdin */
	dup2(pipe_io[1][1], 1);		/* stdout */
	dup2(pipe_io[1][1], 2);		/* stderr */
	for (i=getdtablesize(); i>2; i--)
	    close(i);			/* close the pipe fd's */
	for (i=0; i<NSIG; i++)
	    signal(i, SIG_DFL);		/* don't need parent's sig masks */
	if (mode == 1) {
	    if (strncmp(*argv, "/bin/sh", 7) == 0)
		execvp("/bin/sh", argv);
	    else
		execvp(*argv, argv);
	    if (errno == ENOENT)
		printf("%s: command not found.\n", *argv);
	    else
		perror(*argv);
	    perror("execvp");
	    _exit(-1);
	}
	if (mode == 2) {
	    if (strncmp(*cmdBuf, "/bin/sh", 7) == 0)
		execvp("/bin/sh", cmdBuf);
	    else
		execvp(*cmdBuf, cmdBuf);
	    if (errno == ENOENT)
		printf("%s: command not found.\n", *cmdBuf);
	    else
		perror(*cmdBuf);
	    perror("execvp");
	    _exit(-1);
	}
	else {
	    if ((fp = popen(cmdBuf, "r")) == NULL) {
		perror("couldn't do popen to shell");
		return;
	    }
	    while ((i = fread(resultBuf, 1, sizeof(resultBuf), fp)) > 0)
		fwrite(resultBuf, 1, i, stdout);
	    _exit(0);
	}
    }
    if (mode == 1)
	free(buf);
    close(pipe_io[0][0]);
    close(pipe_io[0][1]);
    close(pipe_io[1][1]);
    callback[cbNum].pipe_io = pipe_io[1][0];
    notify_set_input_func(clientNum, guiCmdRead, pipe_io[1][0]);
    notify_set_wait3_func(clientNum, guiCmdSigchld, pid);
}
Notify_value
guiCmdRead(client, fd)
Notify_client client;
int		fd;
{
    char	buf[GUI_TDIM];
    int		nBytes, i;
    int		j, thisNum=-1;


    for (j=0; j<=callbackNum; j++) {
	if (client == callback[j].clientNum) {
	    thisNum = j;
	    break;
	}
    }

    if (ioctl(fd, FIONREAD, &nBytes) == 0) {
	while (nBytes > 0) {
	    if ((i = read(fd, buf, sizeof(buf)-1)) > 0) {
		buf[i] = '\0';
		if (thisNum >= 0)
		    strcpy(callback[thisNum].result, buf);
		nBytes -= i;
	    }
	}
    }
    if (thisNum >= 0) {
	callback[thisNum].readDone = 1;
	notify_set_input_func(client, NOTIFY_FUNC_NULL,
			callback[thisNum].pipe_io);
	close(fd);
	if (callback[thisNum].sigChldDone) {
	    if (callback[thisNum].callbackFn != NULL) {
		callback[thisNum].callbackFn(callback[thisNum].callbackArg,
					callback[thisNum].result);
	    }
	}
    }
    return NOTIFY_DONE;
}

/*/subhead guiCmdSigchld-------------------------------------------------------
*	When the child process gets done creating a file, it will die,
*	producing SIGCHLD.  At that time, call the callback function.
*----------------------------------------------------------------------------*/
Notify_value
guiCmdSigchld(client, pid, status)
Notify_client client;
int	pid;
union wait *status;
{
    Notify_value i=NOTIFY_IGNORED;
    int		j;


    for (j=0; j<=callbackNum; j++) {
	if (client == callback[j].clientNum) {
	    callback[j].sigChldDone = 1;
	    if (WIFEXITED(*status)) {
		if (callback[j].readDone) {
		    if (callback[j].callbackFn != NULL) {
			callback[j].callbackFn(callback[j].callbackArg,
						callback[j].result);
		    }
		}
		i = NOTIFY_DONE;
	    }

	    return i;
	}
    }
    return i;
}

/*+/subr**********************************************************************
* NAME	guiTextField - create a text entry field
*
* DESCRIPTION
*
* RETURNS
*	Panel_item handle for text field
*
*-*/
Panel_item
guiTextField(label, panel, pX, pY, pHt, proc, nStored, nShown,
						key1, val1, key2, val2)
char	*label;		/* I label for text field */
Panel	panel;		/* I handle of panel containing text field */
int	*pX;		/* IO pointer to x position in panel, in pixels */
int	*pY;		/* I pointer to y position in panel, in pixels */
int	*pHt;		/* IO ptr to height used by field, in pixels, or NULL */
void	(*proc)();	/* I pointer to procedure to handle carriage return */
int	nStored;	/* I number of characters to store in field */
int	nShown;		/* I number of characters to show on screen */
enum	key1;		/* I key for context information for object */
void	*val1;		/* I value associated with key */
enum	key2;		/* I key for context information for object */
void	*val2;		/* I value associated with key */
{
    Panel_item	item;
    int		height;

    item = (Panel_item)xv_create(panel, PANEL_TEXT,
	PANEL_LABEL_STRING, label,
	PANEL_VALUE_STORED_LENGTH, nStored,
	PANEL_VALUE_DISPLAY_LENGTH, nShown,
	XV_X, *pX, XV_Y, *pY, NULL);
    if (item == NULL) {
	(void)printf("error creating \"%s\" text field\n", label);
	exit(1);
    }
    if (proc != NULL) {
	if (xv_set(item, PANEL_NOTIFY_LEVEL, PANEL_SPECIFIED,
				    PANEL_NOTIFY_PROC, proc, NULL) != XV_OK) {
	    (void)printf("error setting PANEL_SPECIFIED for \"%s\"\n",label);
	    exit(1);
	}
    }
    if (key1 != 0) {
	if (xv_set(item, XV_KEY_DATA, key1, val1, NULL) != XV_OK) {
	    (void)printf("error setting XV_KEY_DATA for \"%s\"\n",label);
	    exit(1);
	}
    }
    if (key2 != 0) {
	if (xv_set(item, XV_KEY_DATA, key2, val2, NULL) != XV_OK) {
	    (void)printf("error setting XV_KEY_DATA for \"%s\"\n",label);
	    exit(1);
	}
    }
    *pX += (int)xv_get(item, XV_WIDTH) + 10;
    if (pHt != NULL) {
	height = (int)xv_get(item, XV_HEIGHT);
	if (height > *pHt)
	    *pHt = height;
    }

    return item;
}

/*+/subr**********************************************************************
* NAME	guiTextswGets - get the next '\n' terminated string from textsw
*
* DESCRIPTION
*	Reads the next string from the text subwindow into the caller's
*	buffer.  Reading stops when a newline is encountered or when the
*	caller's buffer is full; the newline IS stored in the buffer.
*	The string in the buffer is always null terminated.
*
*	The caller must supply the character index (starting with 0) of
*	the position in the textsw where reading is to start.  On return,
*	the caller's index is set to the next available character for
*	reading.
*
* RETURNS
*	pointer to string, or
*	NULL if there are no more characters in text subwindow
*
*-*/
char *
guiTextswGets(textsw, pBuf, dim, pCharPos)
Textsw	textsw;		/* I the text subwindow */
char	*pBuf;		/* I pointer to buffer to receive string */
int	dim;		/* I dimension of buffer */
int	*pCharPos;	/* IO index of character to start reading */
{
    Textsw_index nextPos;
    int		i;
    char	*pStr;

    nextPos = (Textsw_index)xv_get(textsw, TEXTSW_CONTENTS, *pCharPos,
			pBuf, dim-1);
    if (nextPos == *pCharPos)
	return NULL;
    for (i=0; i<dim-1; i++) {
	if (*pCharPos >= nextPos) {
	    pBuf[i] = '\0';
	    return pBuf;
	}
	(*pCharPos)++;
	if (pBuf[i] == '\n') {
	    pBuf[i+1] = '\0';
	    return pBuf;
	}
    }
    pBuf[i] = '\0';
    return pBuf;
}

/*+/subr**********************************************************************
* NAME	guiTextswPuts - write string into textsw
*
* DESCRIPTION
*	Writes the string to the text subwindow.
*
*	The caller must supply the character index (starting with 0) of
*	the position in the textsw where writing is to start.  On return,
*	the caller's index is set to the next available character for
*	writing.
*
* RETURNS
*	0, or
*	EOF	if an error occurs during the write
*
*-*/
int
guiTextswPuts(textsw, pBuf, pCharPos)
Textsw	textsw;		/* I the text subwindow */
char	*pBuf;		/* I pointer to buffer to write to text subwindow */
int	*pCharPos;	/* IO index of character position to start writing */
{
    Textsw_index	n;
    int		L=strlen(pBuf);

    xv_set(textsw, TEXTSW_INSERTION_POINT, *pCharPos, NULL);
    n = textsw_insert(textsw, pBuf, L);
    if (n != L)
	return EOF;
    *pCharPos = (int)xv_get(textsw, TEXTSW_INSERTION_POINT);
    textsw_possibly_normalize(textsw, *pCharPos);
    return 0;
}

/*+/subr**********************************************************************
* NAME	guiTextswReset - reset a text subwindow, discarding its contents
*
* DESCRIPTION
*
* RETURNS
*	void
*
*-*/
void
guiTextswReset(textsw)
Textsw	textsw;		/* I the text subwindow */
{
    textsw_reset(textsw, 0, 0);
}
