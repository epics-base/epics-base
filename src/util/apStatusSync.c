#define DEBUG

/*
 * apStatusSync.c
 *
 * Reports to status file:
 *        Regular files in this node that are out-for-edit.
 *
 * Reports to removeScript file:
 *	Regular files in this node that exist in the system node
 *      and are not out-for-edit
 *
 *      Links that point to a dest. outside of this node and the final
 *      dest. is not in the application system area
 *
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
int             getAppSysPath();
int             checkLink();
int             dirwalk();
/* rz RMV verbose */
int             verbose;

#define BSIZE 128
#define MAXMSG 256
#define TRUE 1
#define FALSE 0
#define SAME 0

static char     pdot[] = "SCCS/p.";
static char     sdot[] = "SCCS/s.";
static char     msgbuff[MAXMSG];
static void *  pEpics = NULL;
static char *  pTop = NULL;
static int             lenAppl;
static int             lenTop;


/****************************************************************************
    MAIN PROGRAM
****************************************************************************/
main(argc, argv)
    int             argc;
    char          **argv;
{
    DIR *dirp;
    struct dirent  *dp;
    char *dir = ".";

    if ((pTop = (char*)getcwd((char *)NULL, 128)) == NULL) {
           fprintf(stderr, "getcwd failed errno=%d\n",errno);
           return;
    }
    lenTop = strlen(pTop);
    if ((getAppSysPath()) < 0 )
        return;
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
    return;
}

/****************************************************************************
GETAPPSYSPATH
    returns -1 - error, 0 - OK

change this to contents of .applShadowOrgin

****************************************************************************/
getAppSysPath()
{
    void *         pt2;
    char resolved_path[MAXPATHLEN];

    FILE           *fp;
    char *app_path_file = ".applShadowOrgin";
    char            app_path[BSIZE];/* contents of 1st line of p.dot */

    if ((fp = fopen(app_path_file, "r")) == NULL) {
        fprintf(stderr, "apStatusSync: can't fopen %s\n", app_path_file);
        fprintf(stderr, "Probably not at root of application shadow node\n");
        return(-1);
    }
    if ((fgets(app_path, BSIZE, fp)) == NULL) {
        fprintf(stderr, "apStatusSync: FATAL ERROR - reading %s\n", app_path_file);
        return(-1);
    }
    fclose(fp);
    lenAppl = strlen(app_path);
    if ( app_path[lenAppl-1] == '\n' ) {
         app_path[lenAppl-1] = '\0';
    }
    /* reset epics to real path if not on server */
    if(( pt2 = (void *)realpath(app_path, resolved_path)) == NULL) {
        fprintf(stderr, "FATAL ERROR - failed link component of %s=%s\n",
        app_path, resolved_path);
        return (-1);
    }
    lenAppl = strlen(pt2);
    pEpics = (void *)calloc(1,lenAppl+1);
    strcpy(pEpics,pt2);
    return(0);
}
/****************************************************************************
PROCESSFILE

if s.dot exist && p.dot exist - print (out-for-edit) by contents of p.dot.

else if s.dot exist && p.dot !exist - print error

else if file exists in system area - add to removal script


****************************************************************************/
int
processFile(name, dir)
    char           *name;   /* regular file */
    char           *dir;   /* current directory */
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
printf("%s is S_ISREG dir=%s\n",name,dir);
return;
    pbeg = name;
    len = strlen(name);
    if (len + 7 > MAXNAMLEN) {
        fprintf(stderr, "processFile: pathname  %s too long\n", name);
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

    /* if no sccs directory - return */
    if (lstat(sccsDir, &lstbuf) == -1)
        return;
    /* form p.dot name */
    strcpy(dotName, dir);
    strcat(dotName, "/");
    strcat(dotName, pdot);
    strcat(dotName, pend);
    if (stat(dotName, &stbuf) == -1)
    {    /* no p.dot file */
        /* form s.dot name */
        strcpy(dotName, dir);
        strcat(dotName, "/");
        strcat(dotName, sdot);
        strcat(dotName, pend);
        if (stat(dotName, &stbuf) == -1)     /* no s.dot file */
            return;
    }
    if ((fp = fopen(dotName, "r")) == NULL)
    {
        fprintf(stderr, "processFile: can't fopen %s\n", dotName);
        return;
    }
    if ((fgets(ibuf, BSIZE, fp)) != NULL) {
        fprintf(stderr, "%-20s %-25s EDIT - %s", dir, pend, ibuf);
    } else
        fprintf(stderr, "FATAL ERROR - reading %s%s\n", pdot, pend);
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
    int            firstTime=1;  /* ptr to firstTime flag*/
    struct stat     stbuf;

    if (lstat(name, &stbuf) == -1) {
        fprintf(stderr, "procDirEntries: can't access %s errno=%d\n", name, errno);
        return;
    }
    if (S_ISLNK(stbuf.st_mode)) {
	/* skip all links for now*/
	return;
    }
    if (S_ISREG(stbuf.st_mode)) {
	processFile(name, dir );
	return;
    }
    if (S_ISDIR(stbuf.st_mode)) {
/*	printf("name %s is S_ISDIR \n",name); */
	dirwalk(name, procDirEntries);
    }
    return;
}
/****************************************************************************
CHECKLINK
   checks valid symbolic link to application system area
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
        fprintf(stderr, "FATAL ERROR - failed link component of %s=%s\n",
        path, resolved_path);
        return;
    }
    /* compare appl system path and local path with beg of dest */
    /* if neither present in dest - fail */
    if (((strncmp(pEpics, resolved_path, lenAppl)) == SAME )
       || ((strncmp(pTop,   resolved_path, lenTop)) == SAME )) {
        return;
    } else {
        fprintf(stderr,
            "FATAL ERROR - link '%s' must point to application system area or shadow area\n\t dest='%s'\n",path,resolved_path);
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
    DIR            *dirp;
    if ((dirp = opendir(dir)) == NULL) {
        fprintf(stderr, "dirwalk: can't open %s\n", dir);
        return;
    }
    while ((dp = readdir(dirp)) != NULL) {
    if (strcmp(dp->d_name, ".") == 0
        || strcmp(dp->d_name, "..") == 0)
        continue;        /* skip self and parent */
    if (strlen(dir) + strlen(dp->d_name) + 2 > sizeof(name))
        fprintf(stderr, "dirwalk: name %s/%s too long\n",
            dir, dp->d_name);
    else {
        sprintf(name, "%s/%s", dir, dp->d_name);
        (*fcn) (name, dir );
    }
    }
    closedir(dirp);
}
