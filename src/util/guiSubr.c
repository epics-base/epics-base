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
*      Frame guiFrame(label, x, y, width, height, ppDisp, pServer)
*      Frame guiFrameCmd(label, parentFrame, pPanel, resizeProc)
*       Menu guiMenu(proc, key1, val1, key2, val2)
*  Menu_item guiMenuItem(label, menu, proc, inact, dflt, key1,val1,key2,val2)
* Panel_item guiMessage(label, panel, pX, pY, pHt)
*       void guiNotice(pCtx, msg)
*       void guiNoticeFile(pCtx, msg, fName)
* Panel_item guiTextField(lab,pan,pX,pY,pHt,proc,nStr,nDsp,key1,val1,key2,val2)
*
* USAGE
*	In the synopsis for each routine, the `pCtx' is intended to be
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
* NAME	guiFrame - create a base frame
*
* DESCRIPTION
*
* RETURNS
*	Frame handle
*
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

/*+/subr**********************************************************************
* NAME	guiNotice - display a pop-up notice
*
* DESCRIPTION
*	guiNoticeFile(pCtx, msg, fileName) displays the message, along
*			with the file name and the message from perror()
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
