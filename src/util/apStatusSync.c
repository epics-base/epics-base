/*
 * APSTATUSSYNC.C
 *
TODO	skip vw FATAL ERROR

 * must be run from the top node of an application development node
 *
 * Reports to stdout:
 *        Regular files in this node that are out-for-edit.
 *          These are errors if uid doesn't agree. 
 *        Regular files in this node that should be links to the
 *             application system area. 
 *        Links that point to a dest. outside of this node and the final
 *              dest. is not in the application system area

*******************************************????????????
 *        Other Directories containing Regular files not under sccs control.
 *              These directories should be manually checked for private
 *              Imakefiles/Makefiles ...
 *
 *
 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <dirent.h>
#include <errno.h>

void            procDirEntries();
int             processFile();
int             getAppSysPath();
int             checkLink();
int             dirwalk();
int             verbose;
extern int             errno;

#define BSIZE 128
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
/*static int      isTopDirectory; */


/****************************************************************************
    MAIN PROGRAM
****************************************************************************/
main(argc, argv)
    int             argc;
    char          **argv;
{
    char          **pt;
    char            name[20];
    int             i;
    DIR *dirp;
    struct dirent  *dp;
    char *dir = ".";

    if ((argc > 1) && ((strncmp(argv[1], "-v", 2)) == 0))
    verbose = TRUE;
    else
    verbose = FALSE;
    if (( pDvlTop = (void*)getcwd( (char *)NULL, 128)) == NULL) {
           fprintf(stdout, "getcwd failed\n");
           return;
    }
    lenDvlTop = strlen(pDvlTop);
    if ((getAppSysPath()) < 0 )
        return(-1);
    if ((dirp = opendir(dir)) == NULL)
    {
        fprintf(stderr, "apStatusSync - main: can't open directory %s\n", dir);
        return;
    }
    while ((dp = readdir(dirp)) != NULL) {
    if (strcmp(dp->d_name, ".") == 0
        || strcmp(dp->d_name, "..") == 0)
        continue;        /* skip self and parent */
	procDirEntries(dp->d_name, dir);
    }
    closedir(dirp);
    return(0);
}
#if 1 /* DEBUG rewrite to getAppSysPath */
/****************************************************************************
GETAPPSYSPATH
    returns -1 - error, 0 - OK
****************************************************************************/
getAppSysPath()
{
    void *         pt;
    char resolved_path[MAXPATHLEN];

    FILE           *fp;
    char *app_path_file = ".applShadowOrgin";
    char *epics_path_file = "current_rel";
    char  app_path[BSIZE];
    char  epics_path[BSIZE];

    if ((fp = fopen(app_path_file, "r")) == NULL) {
        fprintf(stdout, "apStatusSync: can't fopen %s\n", app_path_file);
        fprintf(stdout, "Probably not at root of application shadow node\n");
        return(-1);
    }
    if ((fgets(app_path, BSIZE, fp)) == NULL) {
        fprintf(stdout, "apStatusSync: FATAL ERROR - reading %s\n", app_path_file);
        return(-1);
    }
    fclose(fp);
    lenApplSys = strlen(app_path);
    if ( app_path[lenApplSys-1] == '\n' ) {
         app_path[lenApplSys-1] = '\0';
    }
    /* reset App SYS to real path if not on server */
    if(( pt = (void *)realpath(app_path, resolved_path)) == NULL) {
        fprintf(stdout, "FATAL ERROR - failed link component of %s=%s\n",
        app_path, resolved_path);
        return (-1);
    }
    lenApplSys = strlen(pt);
    pAppSysTop = (void *)calloc(1,lenApplSys+1);
    strcpy(pAppSysTop,pt);
    if ((fp = fopen(epics_path_file, "r")) == NULL) {
        fprintf(stdout, "apStatusSync: can't fopen %s\n", epics_path_file);
        fprintf(stdout, "Probably not at root of application shadow node\n");
        return(-1);
    }
    if ((fgets(epics_path, BSIZE, fp)) == NULL) {
        fprintf(stdout, "apStatusSync: FATAL ERROR - reading %s\n", epics_path_file);
        return(-1);
    }
    fclose(fp);
    lenEpicsPath = strlen(epics_path);
    if ( epics_path[lenEpicsPath-1] == '\n' ) {
         epics_path[lenEpicsPath-1] = '\0';
    }
    /* reset epics path to real path if not on server */
    if(( pt = (void *)realpath(epics_path, resolved_path)) == NULL) {
        fprintf(stdout, "FATAL ERROR - failed link component of %s=%s\n",
        epics_path, resolved_path);
        return (-1);
    }
    lenEpicsPath = strlen(pt);
    pEpicsRelease = (void *)calloc(1,lenEpicsPath+1);
    strcpy(pEpicsRelease,pt);
    return(0);
}
#endif /* DEBUG rewrite to getAppSysPath */
/****************************************************************************
PROCESSFILE
    for each regular file:
        if s.dot !exist - skip file (private file)
        else if p.dot exist && s.dot !exist - print error
        else if p.dot exist && s.dot exist
           	print dir file and contents of p.dot
****************************************************************************/
int
processFile(name, dir, pfirstTime)
    char           *name;   /* regular file */
    char           *dir;   /* current directory */
    int            *pfirstTime; 
{
    char            sccsDir[MAXNAMLEN];
    char            dotName[MAXNAMLEN];
    char           *pbeg;    /* beg of file pathname */
    char           *pend;    /* beg of filename */
    struct stat     stbuf;
    struct stat     lstbuf;
    FILE           *fp;
    char            ibuf[BSIZE];/* contents of 1st line of p.dot */
    int             hasSccsDir = FALSE;

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
    pend++;            /* points to filename */
    /* determine if dir has an SCCS sub directory */
    strcpy(sccsDir, dir);
    strcat(sccsDir, "/SCCS");
    if (lstat(sccsDir, &lstbuf) == -1) {
        hasSccsDir = FALSE;
    } else
        hasSccsDir = TRUE;

    if (!hasSccsDir) {
        if (!verbose) {
            if (*pfirstTime) {
                fprintf(stdout, "WARNING regular files in dir: %s\n", dir);
                *pfirstTime = FALSE;
            }
        } else {
            fprintf(stdout, "WARNING regular file %s\n", name);
        }
        return;
    }
    /* form p.dot name */
    strcpy(dotName, dir);
    strcat(dotName, "/");
    strcat(dotName, pdot);
    strcat(dotName, pend);
    if (stat(dotName, &stbuf) == -1) {    /* no p.dot file */
        /* form s.dot name */
        strcpy(dotName, dir);
        strcat(dotName, "/");
        strcat(dotName, sdot);
        strcat(dotName, pend);
        if (stat(dotName, &stbuf) == -1) {    /* no s.dot file */
            if (verbose) {
                fprintf(stdout, "WARNING regular file %s\n", name);
            }
        } else { /* has an s.dot */
            fprintf(stdout, "FATAL ERROR - file %s should be a link\n", name);
        }
        return;
    }
    if ((fp = fopen(dotName, "r")) == NULL) {
        fprintf(stdout, "processFile: can't fopen %s\n", dotName);
        return;
    }
    if ((fgets(ibuf, BSIZE, fp)) != NULL) {
        fprintf(stdout, "%-20s %-25s EDIT - %s", dir, pend, ibuf);
    } else
        fprintf(stdout, "FATAL ERROR - reading %s%s\n", pdot, pend);
    fclose(fp);
    return;
}

/****************************************************************************
PROCDIRENTRIES
    process directory entries
****************************************************************************/
void 
procDirEntries(name, dir)
    char           *name; /* entry name */
    char           *dir;  /* current directory */
{
    struct stat     stbuf;
    int             firstTime;

    if (lstat(name, &stbuf) == -1) {
    fprintf(stdout, "procDirEntries: can't access %s\n", name);
    return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFLNK) {
    checkLink(name);
    return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFREG) {
    firstTime = TRUE;
    processFile(name, dir, &firstTime);
    return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
	dirwalk(name, procDirEntries);
    }
    return;
}
/****************************************************************************
CHECKLINK
   checks valid symbolic link for EPICS
****************************************************************************/
int
checkLink(path)
    char           *path;
{
    char resolved_path[MAXPATHLEN];
    void *         pt;
	/* skip any path with "/templates/" in it */
	if ((strstr(path,"/templates/")) != NULL )
	{
		return;
	}
    if(( pt = (void *)realpath(path, resolved_path)) == NULL) {
        fprintf(stdout, "FATAL ERROR - failed link component of %s=%s\n",
        path, resolved_path);
        return;
    }
    /* compare $epics or beg of shadow with beg of dest */
    /* if neither present in dest - fail */
    if (((strncmp(pAppSysTop, resolved_path, lenApplSys)) == SAME )
       || ((strncmp(pEpicsRelease,   resolved_path, lenEpicsPath)) == SAME )
       || ((strncmp(pDvlTop,   resolved_path, lenDvlTop)) == SAME )) {
        return;
    } else {
        fprintf(stdout,
"FATAL ERROR - link '%s' must point to application system area or application shadow area or EPICS release\n\t dest='%s'\n",path,resolved_path);
    }
    return;
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
    if ((dfd = opendir(dir)) == NULL) {
    fprintf(stdout, "dirwalk: can't open %s\n", dir);
    return;
    }
    while ((dp = readdir(dfd)) != NULL) {
    if (strcmp(dp->d_name, ".") == 0
        || strcmp(dp->d_name, "..") == 0)
        continue;        /* skip self and parent */
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
