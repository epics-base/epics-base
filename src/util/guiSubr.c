/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	02-08-91
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
 * .00	02-08-91	rac	initial version
 * .01	07-30-91	rac	installed in SCCS
 * .02	08-14-91	rac	add guiNoticeName; add more documentation
 * .03	09-06-91	rac	add GuiCheckbox..., guiGetNameFromSeln
 * .04	10-23-91	rac	allow window manager to position command
 *				frames; add file selector routine; add
 *				guiInit, guiFileSelect, guiShellCmd; add
 *				guiTextswXxx; don't specify position for
 *				frames
 *
 * make options
 *	-DXWINDOWS	to use xview/X Window System
 */
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
* Panel_item guiButton(label,panel,pX,pY,pHt,proc,menu, key1,val1,key2,val2)
*       void guiButtonCenter(button1, button2, panel)
*       void guiCFdismiss_xvo(item)
*       void guiCFshow_mi(menu, menuItem)
*       void guiCFshow_xvo(item)
*       void guiCFshowPin_mi(menu, menuItem)
*       void guiCFshowPin_xvo(item)
* Panel_item guiCheckbox(label,panel,nItems,pX,pY,pHt,proc,key1,val1,key2,val2)
*       void guiCheckboxItem(label,checkbox,itemNum,dfltFlag,pX,pY,pHt)
*      char *guiFileSelect(pGuiCtx, title, dir, file, dim, callback, pArg)
*      Frame guiFrame(label, x, y, width, height, ppDisp, pServer)
*      Frame guiFrameCmd(label, parentFrame, pPanel, resizeProc)
*      char *guiGetNameFromSeln(pGuiCtx, textSw, headFlag, tailFlag)
*       Icon guiIconCreate(frame, iconBits)
*      Panel guiInit(pGuiCtx, pArgc, argv, label, x, y, width, height)
*       Menu guiMenu(proc, key1, val1, key2, val2)
*  Menu_item guiMenuItem(label, menu, proc, inact, dflt, key1,val1,key2,val2)
* Panel_item guiMessage(label, panel, pX, pY, pHt)
*       void guiNotice(pGuiCtx, msg)
*       void guiNoticeFile(pGuiCtx, msg, fName)
*       void guiNoticeName(pGuiCtx, msg, name)
*       void guiShellCmd(pGuiCtx, cmdBuf, clientNum, callbackFn, callbackArg)
* Panel_item guiTextField(lab,pan,pX,pY,pHt,proc,nStr,nDsp,key1,val1,key2,val2)
*      char *guiTextswGets(textsw, pBuf, dim, >pCharNum)
*       void guiTextswReset(textsw)
*
* BUGS
* o	although they provide some isolation, these routines are still heavily
*	XView specific
*
* USAGE
*	In the synopsis for each routine, the `pGuiCtx' is intended to be
*	&guiContext, (or whatever name is chosen for the GUI_CTX in the
*	program).
*
* #include <all the xview includes for the features you'll use>
* #include <guiSubr.h>
*
*	GUI_CTX	guiContext;
*	Panel	panel;
*
*	panel = guiInit(&guiContext, &argc, argv, "ARR", 100, 100, 0, 0);
*
*-***************************************************************************/

#include <stdio.h>
#include <sys/ioctl.h>	/* for use with notifier */
#include <sys/wait.h>	/* for use with notifier */
#include <signal.h>	/* for use with notifier */

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

Notify_value	guiCmdRead(), guiCmdSigchld();
static int	callbackNum=-1;
struct callbackStruct {
    Notify_client clientNum;
    void	(*callbackFn)();
    void	*callbackArg;
    int		pipe_io;
};
static struct callbackStruct callback[100];


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
int	*pY;		/* IO pointer to y position in panel, in pixels */
int	*pHt;		/* O ptr to height used by button, in pixels, or NULL */
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
int	*pY;		/* IO pointer to y position in panel, in pixels */
int	*pHt;		/* O ptr to height used by label, in pixels, or NULL */
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
int	*pY;		/* IO pointer to y position in panel, in pixels */
int	*pHt;		/* O ptr to height used by label, in pixels, or NULL */
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


void guiFileSelCan_pb();
void guiFileSelCom_pb();
void guiFileSelLs_pb();
void guiFileSelLsDone();
void guiFileSelResize();

/*+/subr**********************************************************************
* NAME	guiFileSelect - select a file from a group
*
*-*/
void guiFileSelect(pGuiCtx, title, dir, file, dim, callback, pArg)
GUI_CTX	*pGuiCtx;
char	*title;
char	*dir;
char	*file;
int	dim;
void	(*callback)();
void	*pArg;
{
    Frame	frame;
    Panel	panel;
    Panel_item	item1, item2;
    int		x=5, y=5, ht=0;
    char	message[120];

    if (pGuiCtx->fsFrame != NULL)
	xv_destroy_safe(pGuiCtx->fsFrame);
    pGuiCtx->fsCallFn = callback;
    pGuiCtx->fsCallArg = pArg;

    frame = guiFrameCmd(title, pGuiCtx->baseFrame, &panel, guiFileSelResize);
    xv_set(frame, XV_KEY_DATA, GUI_PCTX, pGuiCtx, NULL);
    pGuiCtx->fsFrame = frame;
    pGuiCtx->fsDir_PT = guiTextField("Directory:",
	    panel, &x,&y,&ht, NULL, dim-1, 59, GUI_PCTX, pGuiCtx, 0, NULL);
    xv_set(pGuiCtx->fsDir_PT, PANEL_VALUE, dir, NULL);
    guiButton("List", panel, &x,&y,&ht, guiFileSelLs_pb, NULL,
			GUI_PCTX, pGuiCtx, 0, NULL);
    y += ht + 10; x = 5; ht = 0;
    pGuiCtx->fsFile_PT = guiTextField("File:",
	    panel, &x,&y,&ht, NULL, dim-1, 59, GUI_PCTX, pGuiCtx, 0, NULL);
    xv_set(pGuiCtx->fsFile_PT, PANEL_VALUE, file, NULL);
    y += ht + 10; x = 5; ht = 0;
    item1 = guiButton("Cancel", panel, &x,&y,&ht, guiFileSelCan_pb, NULL,
			GUI_PCTX, pGuiCtx, 0, NULL);
    item2 = guiButton("Commit", panel, &x,&y,&ht, guiFileSelCom_pb, NULL,
			GUI_PCTX, pGuiCtx, 0, NULL);
    y += ht + 10; x = 5; ht = 0;
    pGuiCtx->fsStatus_PM = guiMessage(" ", panel, &x, &y, &ht);
    y += ht + 5; x = 5; ht = 0;

    window_fit(panel);
    guiButtonCenter(item1, item2, panel);
    pGuiCtx->fsTextsw = guiBrowser(frame, panel, 60, 30, NULL);
    window_fit(frame);
    xv_set(frame, XV_SHOW, TRUE, FRAME_CMD_PUSHPIN_IN, TRUE, NULL);
    sprintf(pGuiCtx->fsLsFile, "/tmp/guiFileSel%d.ls.tmp", getpid());
    unlink(pGuiCtx->fsLsFile);
    sprintf(message, "cd %s; ls -l * >%s", dir, pGuiCtx->fsLsFile);
    guiShellCmd(pGuiCtx, message, GUI_FILE_SEL_CLIENT,
					guiFileSelLsDone, pGuiCtx);
    sprintf(message, "processing names in %s", dir);
    xv_set(pGuiCtx->fsStatus_PM, PANEL_LABEL_STRING, message, NULL);
}

/*+/subr**********************************************************************
* NAME	guiFileSelCan_pb
*
*-*/
static void
guiFileSelCan_pb(item)
Panel_item item;
{   GUI_CTX	*pGuiCtx=(GUI_CTX *)xv_get(item, XV_KEY_DATA, GUI_PCTX);
    char	path[240];
    char	*dir, *file;

    xv_set(pGuiCtx->fsFrame,
		XV_SHOW, FALSE, FRAME_CMD_PUSHPIN_IN, FALSE, NULL);
}

/*+/subr**********************************************************************
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
	xv_set(pGuiCtx->fsFrame,
		XV_SHOW, FALSE, FRAME_CMD_PUSHPIN_IN, FALSE, NULL);
    }
    else
	guiNotice(pGuiCtx, "file name is blank; select file or use Cancel");
}

/*+/subr**********************************************************************
* NAME	guiFileSelLs_pb
*
*-*/
static void
guiFileSelLs_pb(item)
Panel_item item;
{   GUI_CTX	*pGuiCtx=(GUI_CTX *)xv_get(item, XV_KEY_DATA, GUI_PCTX);
    char	message[120];
    char	*dir=(char *)xv_get(pGuiCtx->fsDir_PT, PANEL_VALUE);
    unlink(pGuiCtx->fsLsFile);
    sprintf(message, "cd %s; ls -l * >%s", dir, pGuiCtx->fsLsFile);
    guiShellCmd(pGuiCtx, message, GUI_FILE_SEL_CLIENT,
					guiFileSelLsDone, pGuiCtx);
    sprintf(message, "processing names in %s", dir);
    xv_set(pGuiCtx->fsStatus_PM, PANEL_LABEL_STRING, message, NULL);
}

/*+/subr**********************************************************************
* NAME	guiFileSelLsDone
*
*-*/
static void
guiFileSelLsDone(pGuiCtx)
GUI_CTX	*pGuiCtx;
{
    char	*dir=(char *)xv_get(pGuiCtx->fsDir_PT, PANEL_VALUE);
    char	message[120];
    xv_set(pGuiCtx->fsTextsw, TEXTSW_FILE, pGuiCtx->fsLsFile, NULL);
    unlink(pGuiCtx->fsLsFile);
    if ((int)xv_get(pGuiCtx->fsTextsw, TEXTSW_LENGTH) <= 0)
	sprintf(message, "there are no files in %s", dir);
    else
	sprintf(message, "size and modification date are shown");
    xv_set(pGuiCtx->fsStatus_PM, PANEL_LABEL_STRING, message, NULL);
}

/*+/subr**********************************************************************
* NAME	guiFileSelResize
*
*-*/
static void
guiFileSelResize(window, event, arg)
Xv_Window window;
Event   *event;
Notify_arg arg;
{
    Rect        rect;
    int         y;
    GUI_CTX	*pGuiCtx=(GUI_CTX *)xv_get(window, XV_KEY_DATA, GUI_PCTX);

    if (event_id(event) == WIN_RESIZE) {
        rect = *(Rect *)xv_get(pGuiCtx->fsFrame, XV_RECT);
        y = (int)xv_get(pGuiCtx->fsTextsw, XV_Y);
        xv_set(pGuiCtx->fsTextsw, XV_WIDTH, rect.r_width,
                                XV_HEIGHT, rect.r_height-y, NULL);
    }
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
*	Create a command frame and an associated panel.  If a procedure
*	is specified for handling resize, then the frame is set to receive
*	resize events..
*
* RETURNS
*	Frame handle
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
    static char	buf[100];
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
*	o   creating a panel within the base framek
*	o   initializing the gui context block
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

    xv_init(XV_INIT_ARGC_PTR_ARGV, pArgc, argv, 0);
    pGuiCtx->baseFrame = guiFrame(label, x, y, width, height,
		&pGuiCtx->pDisp, &pGuiCtx->server);
    pGuiCtx->fsFrame = NULL;

    return (Panel)xv_create(pGuiCtx->baseFrame, PANEL,
	    XV_X, 0, XV_Y, 0, PANEL_LAYOUT, PANEL_HORIZONTAL, NULL);
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
int	*pY;		/* IO pointer to y position in panel, in pixels */
int	*pHt;		/* O ptr to height of message, in pixels, or NULL */
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
    char	myName[120], *pName;
    if (strlen(name) < 117){
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
* NAME	guiShellCmd - issue a shell command, cooperating with the notifier
*
* DESCRIPTION
*	This routine issues a shell command under the auspices of the
*	xview notifier.  The command is issued following a fork, with a
*	popen call.  The critical issue is that the fork results in a
*	SIGCHLD signal which the notifier must handle.
*
*	This routine (actually guiCmdSigchld) must be customized for each
*	application.
*
*	This customization handles:
*	o   doing a file load into sample set text subwindow of the file
*	    containing a list of file names
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
Notify_client clientNum;/* I a unique, arbitrary value to be used by
				guiCmdSigchld when the child is done
				processing the command */
void	(*callbackFn)();/* I pointer to function for guiCmdSigchld to call */
void	*callbackArg;	/* I arg to pass to callbackFn */
{
    FILE	*fp;
    int		i, pid, cbNum;
    char	resultBuf[80];
    int		pipe_io[2][2];

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

    pipe(pipe_io[0]);
    pipe(pipe_io[1]);
    if ((pid = fork()) == -1) {
	perror("error executing shell command");
	close(pipe_io[0][0]);
	close(pipe_io[0][1]);
	close(pipe_io[1][0]);
	close(pipe_io[1][1]);
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
	if ((fp = popen(cmdBuf, "r")) == NULL) {
	    perror("couldn't do popen to shell");
	    return;
	}
	while ((i = fread(resultBuf, 1, sizeof(resultBuf), fp)) > 0)
	    fwrite(resultBuf, 1, i, stdout);
	_exit(0);
    }
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
    char	buf[80];
    int		nBytes, i;

    if (ioctl(fd, FIONREAD, &nBytes) == 0) {
	while (nBytes > 0) {
	    if ((i = read(fd, buf, sizeof(buf))) > 0) {
#if   0
		textsw_insert(textsw, buf, i);
		write(1, buf, i);
#endif
		nBytes -= i;
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
	    if (WIFEXITED(*status)) {
		notify_set_input_func(client, NOTIFY_FUNC_NULL,
		    callback[j].pipe_io);
		i = NOTIFY_DONE;
	    }

	    if (callback[j].callbackFn != NULL)
		callback[j].callbackFn(callback[j].callbackArg);
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
int	*pY;		/* IO pointer to y position in panel, in pixels */
int	*pHt;		/* O ptr to height used by field, in pixels, or NULL */
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
guiTextswGets(textsw, pBuf, dim, pCharNum)
Textsw	textsw;		/* I the text subwindow */
char	*pBuf;		/* I pointer to buffer to receive string */
int	dim;		/* I dimension of buffer */
int	*pCharNum;	/* IO index of character to start reading */
{
    Textsw_index nextPos;
    int		i;
    char	*pStr;

    nextPos = (Textsw_index)xv_get(textsw, TEXTSW_CONTENTS, *pCharNum,
			pBuf, dim-1);
    if (nextPos == *pCharNum)
	return NULL;
    for (i=0; i<dim-1; i++) {
	if (*pCharNum >= nextPos) {
	    pBuf[i] = '\0';
	    return pBuf;
	}
	(*pCharNum)++;
	if (pBuf[i] == '\n') {
	    pBuf[i+1] = '\0';
	    return pBuf;
	}
    }
    pBuf[i] = '\0';
    return pBuf;
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
