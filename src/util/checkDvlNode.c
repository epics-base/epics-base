/*
 * CHECKDVLNODE.C
 *
 * must be run from the top of an EPICS development node
 *
 * Reports to stdout:
 *        Regular files in this node that are out-for-edit.
 *          These are errors if uid doesn't agree. 
 *        Regular files in this node that should be links to epics
 *        Links that point to a dest. outside of this node and the final
 *              dest. is not in an EPICS node (Note: vw should be excluded).
 *        Other Directories containing Regular files not under sccs control.
 *              These directories should be manually checked for private
 *              Imakefiles/Makefiles ...
 *
 * BUGS - only checks final destination of links.
 *	if a link points outside of the current node
 *	but the final real file/directory resides in epics
 *	it is not reported. Then if it is changed in the outside
 *	node you won't know it w/o running this tool again.
 *
 * distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * rcz
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
int             getRealEpicsPath();
int             checkLink();
int             dirwalk();
int             verbose;
extern int             errno;

#define BSIZE 128
#define MAXMSG 256
#define TRUE 1
#define FALSE 0
#define SAME 0

static char     pdot[] = "SCCS/p.";
static char     sdot[] = "SCCS/s.";
static char     msgbuff[MAXMSG];
static caddr_t  pEpics = (caddr_t)NULL;
static caddr_t  pDvl = (caddr_t)NULL;
static int             lenEpics;
static int             lenDev;

static char    *dirList[] = {
    "share",
    "Unix",
    "Vx",
    "config",
    "release"
};
#define DIR_COUNT (sizeof(dirList) / sizeof(caddr_t))

/****************************************************************************
    MAIN PROGRAM
****************************************************************************/
main(argc, argv)
    int             argc;
    char          **argv;
{
    char  *getcwd();
    char          **pt;
    char            name[20];
    int             i;
    pt = dirList;
    if ((argc > 1) && ((strncmp(argv[1], "-v", 2)) == 0))
    verbose = TRUE;
    else
    verbose = FALSE;
    if ((pDvl = getcwd((char *)NULL, 128)) == NULL) {
           fprintf(stdout, "getcwd failed\n");
           return;
    }
    lenDev = strlen(pDvl);
    if ((getRealEpicsPath()) < 0 )
        return;
    fprintf(stdout, "\n\n\n%s:\nSTARTING SEARCH FROM NODE: %s\n",argv[0], pDvl);
    for (i = 0; i < DIR_COUNT; i++, pt++) {
    strcpy(name, *pt);
    fprintf(stdout, "Descending node --------------- %s\n",name);
    procDirEntries(name, name);
    }
    return;
}
/****************************************************************************
GETREALEPICSPATH
    returns -1 - error, 0 - OK
****************************************************************************/
getRealEpicsPath()
{
    caddr_t         pt2;
    char resolved_path[MAXPATHLEN];

    FILE           *fp;
    char *epath_file = ".epicsShadowSrc";
    char            epath[BSIZE];/* contents of 1st line of p.dot */

    if ((fp = fopen(epath_file, "r")) == NULL) {
        fprintf(stdout, "checkDvlNode: can't fopen %s\n", epath_file);
        fprintf(stdout, "Probably not at root of shadow node\n");
        return(-1);
    }
    if ((fgets(epath, BSIZE, fp)) == NULL) {
        fprintf(stdout, "checkDvlNode: FATAL ERROR - reading %s\n", epath_file);
        return(-1);
    }
    fclose(fp);
    lenEpics = strlen(epath);
    if ( epath[lenEpics-1] == '\n' ) {
         epath[lenEpics-1] = '\0';
    }
    /* reset epics to real path if not on server */
    if(( pt2 = (caddr_t)realpath(epath, resolved_path)) == NULL) {
        fprintf(stdout, "FATAL ERROR - failed link component of %s=%s\n",
        epath, resolved_path);
        return (-1);
    }
    lenEpics = strlen(pt2);
    pEpics = (caddr_t)calloc(1,lenEpics+1);
    strcpy(pEpics,pt2);
    return(0);
}
/****************************************************************************
PROCESSFILE
    for each regular file:
        if s.dot !exist - skip file
        if s.dot exist && p.dot !exist - print error
           else print dir file and contents of p.dot
****************************************************************************/
int
processFile(name, dir, pfirstTime)
    char           *name;   /* regular file */
    char           *dir;   /* current directory */
    int            *pfirstTime;  /* ptr to firstTime flag*/
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
procDirEntries(name, dir, pfirstTime)
    char           *name; /* entry name */
    char           *dir;  /* current directory */
    int            *pfirstTime;  /* ptr to firstTime flag*/
{
    struct stat     stbuf;
    if (lstat(name, &stbuf) == -1) {
    fprintf(stdout, "procDirEntries: can't access %s\n", name);
    return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFLNK) {
    checkLink(name);
    return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFREG) {
    processFile(name, dir, pfirstTime);
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
    caddr_t         pt;
	/* skip any path with "/templates/" in it */
	if ((strstr(path,"/templates/")) != NULL )
	{
		return;
	}
    if(( pt = (caddr_t)realpath(path, resolved_path)) == NULL) {
        fprintf(stdout, "FATAL ERROR - failed link component of %s=%s\n",
        path, resolved_path);
        return;
    }
    /* compare $epics or beg of shadow with beg of dest */
    /* if neither present in dest - fail */
    if (((strncmp(pEpics, resolved_path, lenEpics)) == SAME )
       || ((strncmp(pDvl,   resolved_path, lenDev)) == SAME )) {
        return;
    } else {
        fprintf(stdout,
            "FATAL ERROR - link '%s' must point to epics or shadow area\n\t dest='%s'\n",path,resolved_path);
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
    int             firstTime;
    if ((dfd = opendir(dir)) == NULL) {
    fprintf(stdout, "dirwalk: can't open %s\n", dir);
    return;
    }
    firstTime = TRUE;
    while ((dp = readdir(dfd)) != NULL) {
    if (strcmp(dp->d_name, ".") == 0
        || strcmp(dp->d_name, "..") == 0)
        continue;        /* skip self and parent */
    if (strlen(dir) + strlen(dp->d_name) + 2 > sizeof(name))
        fprintf(stdout, "dirwalk: name %s/%s too long\n",
            dir, dp->d_name);
    else {
        sprintf(name, "%s/%s", dir, dp->d_name);
        (*fcn) (name, dir, &firstTime);
    }
    }
    closedir(dfd);
}
