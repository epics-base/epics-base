/* APSTATUSSYNC.C
 *****************************************************************
 * TODO - change the whole tool => almost DONE
 *	from the shadow point of view
 *	dirwalk from ../. (shadow directory)
 *
 *	if regular file - do
 *		if SCCS file and out for edit
 *			(report as EDIT)
 *		else if SCCS file and Not out for edit 
 *			1. (report as illegal file - should be a link)
 *			2. place on the remove list
 *		else 
 *			1. place on the remove list
 *	if link - do
 *		if a relative link (doesn't start with / )
 *			should agree with the appl system area (relative)
 *		else if it doesn't terminate (no access)
 *			1. (report as link component failure)
 *			2. place on the remove list
 *		else if it terminates in the wrong place
 *			1. (report as illegal link)
 *			2. place on the remove list
 *
 *	if dir - do
 *		if name == SCCS
 *			1. (report as illegal SCCS directory)
 *			2. place on the remove list
 *		else if NOT in the system area
 *			1. place on the remove list
*****************************************************************
 * APSTATUSSYNC.C
 *
 * must be run from the top node of an application development node
 *
 * Reports to stdout:
 *        Regular files in this node that are out-for-edit.
 *          These are errors if uid doesn't agree.
 *        Regular files in this node that should be links to the
 *             application system area.
 *        Links that point to a dest. outside of this node and the final
 *              dest. is not in the application system area or EPICS
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <dirent.h>
#include <errno.h>
#include <malloc.h>

void            procDirEntries();
int             processFile();
int             getAppSysPath();
int             procLinkEntries();
int             dirwalk();
int             verbose;
extern int      errno;
#define TRUE 1
#define FALSE 0
#define SAME 0

static char     pdot[] = "SCCS/p.";
static char     sdot[] = "SCCS/s.";
static char     msgbuff[MAXPATHLEN];
static void    *pEpicsRelease = NULL;
static void    *pAppSysTop = NULL;
static void    *pDvlTop = NULL;
static int      lenApplSys;
static int      lenEpicsPath;
static int      lenDvlTop;
static int      initScriptFile();
static int      closeScriptFile();
static int      appendToScriptFile();
static FILE    *apRemoveScriptFp = (FILE*)NULL;
static char    *pScriptFileName = "apRemoveScript";
static    char           *plogname;	/* logname pointer */


/****************************************************************************
    MAIN PROGRAM
****************************************************************************/
main(argc, argv)
    int             argc;
    char          **argv;
{
    if ((argc > 1) && ((strncmp(argv[1], "-v", 2)) == 0))
	verbose = TRUE;
    else
	verbose = FALSE;
	if ((plogname = getenv("LOGNAME")) == 0) {
	    fprintf(stdout, "apStatusSync: Can't get LOGNAME\n");
	    return (-1);
	}
fprintf(stdout, "\n\n===========================================================\n");
fprintf(stdout, "apStatusSync: Status Started  ==>> ==>> ==>> ==>> ==>>\n");
fprintf(stdout, "===========================================================\n");
    if ((initScriptFile()) != 0)
	return (-1);
    if ((processTopDir()) != 0)
	return (-1);
    if ((closeScriptFile()) != 0)
	return (-1);
    postAmble();
    return (0);
}

/****************************************************************************
PROCLINKENTRIES
   checks valid symbolic link for EPICS
   at entry path is the name of a local link
****************************************************************************/
int
procLinkEntries(path)
    char           *path;
{
    char            resolved_path[MAXPATHLEN];
    char            buf[MAXPATHLEN];
    char            buf2[MAXPATHLEN];
    char            buf3[MAXPATHLEN];
    char            buffer[MAXNAMLEN];
    void           *pt;
    int             nchars;
    resolved_path[0] = '\0';
    if ((pt = (void *) realpath(path, resolved_path)) == NULL) {
	fprintf(stdout, "FATAL ERROR - failed link component of %s\n\t= %s\n",
		path, resolved_path);
	sprintf(buffer, "#/bin/rm -f  ./%s\n", path);
	if ((appendToScriptFile(buffer)) < 0)
	    return (-1);	/* should not happen */
	return (-1);
    }
    /* skip any link path with "/templates/" in it */
    if ((strstr(resolved_path, "/templates/")) != NULL)
	return;
    /* skip any link path with "vw" in it */
    if ((strstr(path, "vw")) != NULL) {
	return;
    }
    /* skip any link path with "/vxWorks" in it */
    if ((strstr(path, "/vxWorks")) != NULL)
	return;

/* assume a relative link name doesn't begin with a '/' character */
    buf[0] = '\0';
    if ((nchars = readlink(path, buf, MAXPATHLEN)) < 0) {
	fprintf(stdout, "\treadlink failed errno=%d\n", errno);
	sprintf(buffer, "#/bin/rm -f  ./%s\n", path);
	if ((appendToScriptFile(buffer)) < 0)
	    return;
    }
    buf[nchars] = '\0';
    strcpy(buf2, pAppSysTop);
    strcat(buf2, "/");
    strcat(buf2, path);
    buf3[0] = '\0';
    if ((strstr(resolved_path, pAppSysTop)) != NULL) {
	return;
    } else if ((strstr(resolved_path, pEpicsRelease)) != NULL) {
	return;
    } else if ((strstr(resolved_path, pDvlTop)) != NULL) {
	return;
    } else {
	fprintf(stdout,
		"FATAL ERROR - link '%s' should point to the application system area\n\tor the application shadow area or an EPICS release area\n\tdest='%s'\n", path, resolved_path);
    }
    return;
}
/****************************************************************************
GETAPPSYSPATH
    returns -1 - error, 0 - OK
****************************************************************************/
getAppSysPath()
{
    void           *pt;
    char            resolved_path[MAXPATHLEN];

    FILE           *fp;
    char           *app_path_file = ".applShadowOrgin";
    char           *epics_path_file = "current_rel";
    char            app_path[MAXPATHLEN];
    char            epics_path[MAXPATHLEN];
    resolved_path[0] = '\0';
    if ((fp = fopen(app_path_file, "r")) == NULL) {
	fprintf(stdout, "apStatusSync: can't fopen %s\n", app_path_file);
	fprintf(stdout, "Probably not at root of application shadow node\n");
	return (-1);
    }
    if ((fgets(app_path, MAXPATHLEN, fp)) == NULL) {
	fprintf(stdout, "apStatusSync: FATAL ERROR - reading %s\n", app_path_file);
	return (-1);
    }
    fclose(fp);
    lenApplSys = strlen(app_path);
    if (app_path[lenApplSys - 1] == '\n') {
	app_path[lenApplSys - 1] = '\0';
    }
    /* reset App SYS to real path if not on server */
    if ((pt = (void *) realpath(app_path, resolved_path)) == NULL) {
	fprintf(stdout, "FATAL ERROR - failed link component of %s=%s\n",
		app_path, resolved_path);
	return (-1);
    }
    lenApplSys = strlen(pt);
    pAppSysTop = (void *) calloc(1, lenApplSys + 1);
    strcpy(pAppSysTop, pt);
    if ((fp = fopen(epics_path_file, "r")) == NULL) {
	fprintf(stdout, "apStatusSync: can't fopen %s\n", epics_path_file);
	fprintf(stdout, "Probably not at root of application shadow node\n");
	return (-1);
    }
    if ((fgets(epics_path, MAXPATHLEN, fp)) == NULL) {
	fprintf(stdout, "apStatusSync: FATAL ERROR - reading %s\n", epics_path_file);
	return (-1);
    }
    fclose(fp);
    lenEpicsPath = strlen(epics_path);
    if (epics_path[lenEpicsPath - 1] == '\n') {
	epics_path[lenEpicsPath - 1] = '\0';
    }
    /* reset epics path to real path if not on server */
    if ((pt = (void *) realpath(epics_path, resolved_path)) == NULL) {
	fprintf(stdout, "FATAL ERROR - failed link component of %s\n\t= %s\n",
		epics_path, resolved_path);
	return (-1);
    }
    lenEpicsPath = strlen(pt);
    pEpicsRelease = (void *) calloc(1, lenEpicsPath + 1);
    strcpy(pEpicsRelease, pt);
    return (0);
}

/****************************************************************************
PROCDIRENTRIES
    process directory entries
****************************************************************************/
void
procDirEntries(name, dir)
    char           *name;	/* entry name */
    char           *dir;	
{
    struct stat     stbuf;
    char           *pbeg;	/* beg of file pathname */
    char           *pend;	/* beg of filename */
    char           *pendm1;	/* beg of filename */
    int             len;
    int             j;
    char            buffer[MAXNAMLEN];

    if (lstat(name, &stbuf) == -1) {
	fprintf(stdout, "procDirEntries: can't access %s\n", name);
	return;
    }
    pbeg = name;
    len = strlen(name);
    if (len + 7 > MAXNAMLEN) {
	fprintf(stdout, "processFile: pathname  %s too long\n", name);
	return ;
    }
    /* search for last slash '/' in pathname */
    for (j = len, pend = pbeg + len; j > 0; j--, pend--) {
	if (*pend == '/')
	    break;
    }
    pend++;
    pendm1 = pend;
    pendm1--;
    /* pend points to filename */
    if ((stbuf.st_mode & S_IFMT) == S_IFLNK) {
	procLinkEntries(name);
	return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFREG) {
	processFile(name, dir);
	return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
    /* if current directory is SCCS - ... */
    if ((strstr(pendm1, "SCCS")) != NULL) {
	fprintf(stdout, "FATAL ERROR - directory ./%s should be a link\n", name);
	sprintf(buffer, "#/bin/rm -fr  ./%s\n", name);
	if ((appendToScriptFile(buffer)) < 0)
	    return;
    }
	dirwalk(name, procDirEntries);
    }
    return;
}

/****************************************************************************
PROCESSFILE
    for each regular file:
        if s.dot !exist - skip file (private file)
        else if p.dot exist && s.dot !exist - print error
        else if p.dot exist && s.dot exist
           	print dir file and contents of p.dot
****************************************************************************/
int
processFile(name, dir)
    char           *name;	/* regular file */
    char           *dir;	/* current directory */
{
    char            sccsDir[MAXNAMLEN];
    char            sdotName[MAXNAMLEN];
    char            pdotName[MAXNAMLEN];
    char            buffer[MAXNAMLEN];
    char           *pbeg;	/* beg of file pathname */
    char           *pend;	/* beg of filename */
    char           *pendm1;	/* beg of filename */
    char            udotname[MAXNAMLEN+5];	/* took file out-for-edit user name */
    struct stat     stbuf;
    struct stat     lstbuf;
    FILE           *fp;
    char            ibuf[MAXPATHLEN+5];	/* contents of 1st line of p.dot */
    int             hasSccsDir = FALSE;
    int             hasPdot = FALSE;
    int             hasSdot = FALSE;

    int             len,
                    j;
    pbeg = name;
    len = strlen(name);
    if (len + 7 > MAXNAMLEN) {
	fprintf(stdout, "processFile: pathname  %s too long\n", name);
	return (0);
    }
    /* search for last slash '/' in pathname */
    for (j = len, pend = pbeg + len; j > 0; j--, pend--) {
	if (*pend == '/')
	    break;
    }
    pend++;
    pendm1 = pend;
    pendm1--;
    /* pend points to filename */

    /* skip some setup files */
    if ((strstr(pendm1, ".applShadow")) != NULL)
	return;
    if ((strstr(pendm1, ".applShadowOrgin")) != NULL)
	return;
    if ((strstr(pendm1, "apRemoveScript")) != NULL)
	return;
    /* determine if dir has an SCCS sub directory */
    strcpy(sccsDir, dir);
    strcat(sccsDir, "/SCCS");

    if (lstat(sccsDir, &lstbuf) == -1) {
	hasSccsDir = FALSE;
    } else {
	hasSccsDir = TRUE;
    }
    /* form p.dot name */
    strcpy(pdotName, dir);
    strcat(pdotName, "/");
    strcat(pdotName, pdot);
    strcat(pdotName, pend);
    if (stat(pdotName, &stbuf) == 0)
	hasPdot = TRUE;
    /* form s.dot name */
    strcpy(sdotName, dir);
    strcat(sdotName, "/");
    strcat(sdotName, sdot);
    strcat(sdotName, pend);
    if (stat(sdotName, &stbuf) == 0)
	hasSdot = TRUE;
    if (hasPdot && hasSdot) {
	if ((fp = fopen(pdotName, "r")) == NULL) {
	    fprintf(stdout, "processFile: can't fopen %s\n", pdotName);
	    return (-1);
	}
	if ((fgets(ibuf, MAXPATHLEN, fp)) != NULL) {
	    /* is this the original sccs out-for-edit owner ??? */
	    strcpy(udotname, ibuf);
	    if ((strstr(udotname, plogname))==NULL) {
		fclose(fp);
		return (0);
	    }
	    /* else print status */
	    fprintf(stdout, "%-20s %-25s EDIT - %s", dir, pend, ibuf);
	} else {
	    fprintf(stdout, "FATAL ERROR - reading %s%s\n", pdot, pend);
	}
	fclose(fp);
    } else if (hasPdot && !hasSdot) {
	/* stray ???  somebody removed the s.dot */
	fprintf(stdout, "Warning - someone removed '%s' from sccs control\nw/o removing the lock file\n", name);
	sprintf(buffer, "#/bin/rm -f  ./%s\n", name);
	if ((appendToScriptFile(buffer)) < 0)
	    return (-1);	/* should not happen */
    } else if (!hasPdot && hasSdot) {
	fprintf(stdout, "POSSIBLE FATAL ERROR - file ./%s should be a link\n", name);
	sprintf(buffer, "#/bin/rm -f  ./%s\n", name);
	if ((appendToScriptFile(buffer)) < 0)
	    return (-1);
    } else if (!hasPdot && !hasSdot) {
	/* stray ???  regular files in directories */
	sprintf(buffer, "#/bin/rm -f  ./%s\n", name);
	if ((appendToScriptFile(buffer)) < 0)
	    return (-1);
    }
    return (0);
}
/****************************************************************************
DIRWALK applies a function to each file in a directory
****************************************************************************/
int
dirwalk(dir, fcn)
    char           *dir;
    void            (*fcn) ();
{
    char            name[MAXNAMLEN];
    struct dirent  *dp;
    DIR            *dfd;
    struct stat     sb;
    char            buffer[MAXNAMLEN];
    if ((dfd = opendir(dir)) == NULL) {
	fprintf(stdout, "dirwalk: can't open %s\n", dir);
	return(-1);
    }
    strcpy(name, pAppSysTop);
    strcat(name, "/");
    strcat(name, dir);
    if ((stat(name, &sb) != 0)) {
	sprintf(buffer, "#/bin/rm -fr  ./%s\n", dir);
	if ((appendToScriptFile(buffer)) < 0)
	    return (-1);
    }
    while ((dp = readdir(dfd)) != NULL) {
	if (strcmp(dp->d_name, ".") == 0
		|| strcmp(dp->d_name, "..") == 0)
	    continue;		/* skip self and parent */
	if (strlen(dir) + strlen(dp->d_name) + 2 > sizeof(name))
	    fprintf(stdout, "dirwalk: name %s/%s too long\n",
		    dir, dp->d_name);
	else {
	    sprintf(name, "%s/%s", dir, dp->d_name);
	    (*fcn) (name, dir);
	}
    }
    closedir(dfd);
}
/****************************************************************************
processTopDir
	processes the top directory
****************************************************************************/
processTopDir()
{
    DIR            *dirp;
    struct dirent  *dp;
    char           *dir = ".";
    if ((pDvlTop = (void *) getcwd((char *) NULL, 128)) == NULL) {
	fprintf(stdout, "getcwd failed\n");
	return (-1);
    }
    lenDvlTop = strlen(pDvlTop);
    if ((getAppSysPath()) < 0)
	return (-1);
    if ((dirp = opendir(dir)) == NULL) {
	fprintf(stderr, "apStatusSync - main: can't open directory %s\n", dir);
	return (-1);
    }
    while ((dp = readdir(dirp)) != NULL) {
	if (strcmp(dp->d_name, ".") == 0
		|| strcmp(dp->d_name, "..") == 0)
	    continue;		/* skip self and parent */
	procDirEntries(dp->d_name, dir);
    }
    closedir(dirp);
    return (0);
}

/****************************************************************************
initScriptFile
****************************************************************************/
static int 
initScriptFile()
{
    struct stat     sb;
/*
 * if apRemoveScript already exists - abort
 * also - don't allow run in system area
 *
 */
    if (!(stat(".applShadow", &sb) == 0)) {
	fprintf(stderr,
		"\napStatusSync: - Can't find file: '.applShadow'\n");
	fprintf(stderr,
	      "You must be in the <top> of your application shadow node\n");
	fprintf(stderr,
		"to run this tool\n");
	return (-1);
    }
    if ((stat(pScriptFileName, &sb) == 0)) {
	fprintf(stderr,
	  "\nThe previous output file '%s' still exists!\n", pScriptFileName);
	fprintf(stderr,
		"Please remove the file and try again\n");
	fprintf(stderr,
		"\t/bin/rm apRemoveScript\n");
	return (-1);
    }
    if ((apRemoveScriptFp = fopen(pScriptFileName, "w")) == NULL) {
	fprintf(stderr, "apStatusSync: can't fopen %s for writing\n", pScriptFileName);
	return (-1);
    }
    return (0);
}
/****************************************************************************
closeScriptFile
****************************************************************************/
static int 
closeScriptFile()
{
    if (!apRemoveScriptFp) {
	fprintf(stderr, "apStatusSync: closeScriptFile() - %s not open\n", pScriptFileName);
	return (-1);
    }
chmod("apRemoveScript",0755);
close (apRemoveScriptFp);
return(0);
}
/****************************************************************************
APPENDTOSCRIPTFILE
****************************************************************************/
static int 
appendToScriptFile(pstring)
    char           *pstring;
{
    if (!apRemoveScriptFp) {
	fprintf(stderr,
		"apStatusSync: appendToScriptFile() - %s not open\n", pScriptFileName);
	return (-1);
    }
    if ((wrt_buf(apRemoveScriptFp, pstring, pScriptFileName) != 0))
	return (-1);
    return (0);
}
/****************************************************************************
wrt_buf - Writes buffer to file
****************************************************************************/
static int
wrt_buf(fp, pstring, pfile)
    FILE           *fp;
    char           *pstring;
    char           *pfile;
{
    int             len;
    len = strlen(pstring);
    if ((fwrite(pstring, len, 1, fp)) != 1) {
	printf("apStatusSync: wrt_buf() fwrite error. errno=%i file=%s\n",
	       errno, pfile);
	return (-1);
    }
    return (0);
}
postAmble()
{
fprintf(stdout,"===========================================================\n");
fprintf(stdout,"Status Completed \n");
fprintf(stdout,"===========================================================\n");
fprintf(stdout,"When you are satisfied with the above status - continue\n");
fprintf(stdout," else - correct the situation and run apStatusSync again\n");
fprintf(stdout,"\ncontinue below\n");
fprintf(stdout,"\nNow please review the contents of the remove script file 'apRemoveScript'.\n");
fprintf(stdout,"Enable each appropriate unix command by removing the leading '#'\n");
fprintf(stdout,"character from the line\n");
fprintf(stdout,"Then invoke the following unix commands:\n");
fprintf(stdout,"\n");
fprintf(stdout,"%%\tapRemoveScript\n");
fprintf(stdout,"\tfollowed by\n");
fprintf(stdout,"%%\tapCreateShadow\n");
}
