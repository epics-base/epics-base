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
*      Frame guiFrame(label, x, y, width, height, ppDisp, pServer)
*      Frame guiFrameCmd(label, parentFrame, pPanel, resizeProc)
*      char *guiGetNameFromSeln(pGuiCtx, textSw, headFlag, tailFlag)
*      Icon  guiIconCreate(frame, iconBits)
*       Menu guiMenu(proc, key1, val1, key2, val2)
*  Menu_item guiMenuItem(label, menu, proc, inact, dflt, key1,val1,key2,val2)
* Panel_item guiMessage(label, panel, pX, pY, pHt)
*       void guiNotice(pGuiCtx, msg)
*       void guiNoticeFile(pGuiCtx, msg, fName)
*       void guiNoticeName(pGuiCtx, msg, name)
* Panel_item guiTextField(lab,pan,pX,pY,pHt,proc,nStr,nDsp,key1,val1,key2,val2)
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
*	
*	xv_init(XV_INIT_ARGC_PTR_ARGV, &argc, argv, 0);
*	guiContext.baseFrame = guiFrame("ARR", 100, 100, 0, 0,
*                &guiContext.pDisp, &guiContext.server);
*	
*	panel = (Panel)xv_create(guiContext.baseFrame, PANEL,
*        XV_X, 0, XV_Y, 0, PANEL_LAYOUT, PANEL_HORIZONTAL, NULL);
*
*-***************************************************************************/

#include <stdio.h>

#if defined XWINDOWS
#   include <xview/xview.h>
#   include <xview/frame.h>
#   include <xview/notice.h>
#   include <xview/openmenu.h>
#   include <xview/panel.h>
#   include <xview/textsw.h>
#   include <xview/seln.h>
#   include <xview/svrimage.h>
#   include <xview/icon.h>
#endif

#include <guiSubr.h>

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
*	menu = guiMenu(..., GUI_PCTX, pCtx...);
*	menuItem = guiMenuItem("label", menu, guiCFshow_mi, ...,
*							GUI_CF, frame...);
*
*	guiButton("Show...", panel, ..., guiCFshow_xvo, ...,
*					 GUI_PCTX, pCtx, GUI_CF, frame);
*
*	menu = guiMenu(..., GUI_PCTX, pCtx...);
*	menuItem = guiMenuItem("label", menu, guiCFshowPin_mi, ...,
*							GUI_CF, frame...);
*
*	guiButton("Show...", panel, ..., guiCFshowPin_xvo, ...,
*					 GUI_PCTX, pCtx, GUI_CF, frame);
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
{   GUI_CTX	*pCtx=(GUI_CTX *)xv_get(menu, XV_KEY_DATA, GUI_PCTX);
    Rect	*mXY;	/* mouse position */
    Frame	frame=(Frame)xv_get(menuItem, XV_KEY_DATA, GUI_CF);
    mXY = (Rect *)xv_get(xv_get(pCtx->baseFrame, XV_ROOT), WIN_MOUSE_XY);
    xv_set(frame, XV_X, mXY->r_left, XV_Y, mXY->r_top, XV_SHOW, TRUE, NULL);
}
void
guiCFshowPin_mi(menu, menuItem)
Menu	menu;
Menu_item menuItem;
{   GUI_CTX	*pCtx=(GUI_CTX *)xv_get(menu, XV_KEY_DATA, GUI_PCTX);
    Rect	*mXY;	/* mouse position */
    Frame	frame=(Frame)xv_get(menuItem, XV_KEY_DATA, GUI_CF);
    mXY = (Rect *)xv_get(xv_get(pCtx->baseFrame, XV_ROOT), WIN_MOUSE_XY);
    xv_set(frame, XV_X, mXY->r_left, XV_Y, mXY->r_top, XV_SHOW, TRUE,
		FRAME_CMD_PUSHPIN_IN, TRUE, NULL);
}
void
guiCFshow_xvo(item)
Xv_opaque item;
{   GUI_CTX	*pCtx=(GUI_CTX *)xv_get(item, XV_KEY_DATA, GUI_PCTX);
    Rect	*mXY;	/* mouse position */
    Frame	frame=(Frame)xv_get(item, XV_KEY_DATA, GUI_CF);
    mXY = (Rect *)xv_get(xv_get(pCtx->baseFrame, XV_ROOT), WIN_MOUSE_XY);
    xv_set(frame, XV_X, mXY->r_left, XV_Y, mXY->r_top, XV_SHOW, TRUE, NULL);
}
void
guiCFshowPin_xvo(item)
Xv_opaque item;
{   GUI_CTX	*pCtx=(GUI_CTX *)xv_get(item, XV_KEY_DATA, GUI_PCTX);
    Rect	*mXY;	/* mouse position */
    Frame	frame=(Frame)xv_get(item, XV_KEY_DATA, GUI_CF);
    mXY = (Rect *)xv_get(xv_get(pCtx->baseFrame, XV_ROOT), WIN_MOUSE_XY);
    xv_set(frame, XV_X, mXY->r_left, XV_Y, mXY->r_top, XV_SHOW, TRUE,
		FRAME_CMD_PUSHPIN_IN, TRUE, NULL);
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
				XV_X, 200, XV_Y, 200, XV_SHOW, FALSE, NULL);
    }
    else {
	frame = (Frame)xv_create(parentFrame, FRAME_CMD, FRAME_LABEL, label,
				FRAME_SHOW_RESIZE_CORNER, TRUE,
				WIN_EVENT_PROC,		resizeProc,
				WIN_CONSUME_EVENTS,	WIN_RESIZE, NULL,
				XV_X, 200, XV_Y, 200, XV_SHOW, FALSE, NULL);
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
guiNotice(pCtx, msg)
GUI_CTX	*pCtx;
char	*msg;
{
    notice_prompt(pCtx->baseFrame, NULL, NOTICE_MESSAGE_STRINGS, msg, NULL,
	    NOTICE_BUTTON_YES,	"Continue", NULL);
    return;
}
void
guiNoticeFile(pCtx, msg, fName)
GUI_CTX	*pCtx;
char	*msg;
char	*fName;
{
    perror(fName);
    notice_prompt(pCtx->baseFrame, NULL,
	    NOTICE_MESSAGE_STRINGS, msg, fName, NULL,
	    NOTICE_BUTTON_YES,	"Continue", NULL);
    return;
}
void
guiNoticeName(pCtx, msg, name)
GUI_CTX	*pCtx;
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
    notice_prompt(pCtx->baseFrame, NULL,
	    NOTICE_MESSAGE_STRINGS, msg, pName, NULL,
	    NOTICE_BUTTON_YES,	"Continue", NULL);
    return;
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
