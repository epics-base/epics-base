/*
 *	apCreateShadow.c
 * - todo
 * complete rework ??? to EPICS area tools - templates etc.
 *
 *
 *		maybe add a complete history file
 *
 * rcz
 * changes
 *		use appList as the filter for the top directories.
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

#define BSIZE 128
#define BUFLEN 256
#define SAME 0

void            procNoTopDirs();
void            procNoAppDirs();
int             doTopNoDirs();
void            Usage();
void            procDirEntries();
void            createLink();
int             procLink();
int             processFile();
int             dirwalk();

int             errno;
int             status;
int             len;
struct stat     stbuf;
char            buffer[BUFLEN];
char            command[80];	/* system command */
char            dest_base[80];	/* dest base (root of shadow  node) */
char            src_base[80];	/* current source descend root node */
char            dpath[80];	/* dest/subdir... */
char            spath[80];	/* src/subdir... */
char            xname[30];	/* program name */
char            appl_base[80];	/* root node */
char            appLocName[80];/* pathname of master appLoc file */


/****************************************************************************
MAIN PROGRAM
	This program creates an application developers set of shadow nodes

	It must be invoked with a full path name so that the official
	application area is known.

****************************************************************************/
main(argc, argv)
    int             argc;
    char          **argv;
{
    char           *ptr;
    char            top_node[30];
    FILE           *appLoc_fp;	/* user version of appLoc file */
    char            comLocName[80];	/* refs to all application common
					 * nodes */
/*
 * intial setup
 */
    init_setup(argc, argv);

/*
 * At this point skip all top directories
 * just process files and links in the top node
 */
    chdir(appl_base);
    doTopNoDirs();

/*
 * descend top directories not in appList
 */
    chdir(appl_base);
    doTopDirsNoAppDirs();

/*
 * Now process the application directories specified by the User's
 * modified copy of the appLoc file
 */
    chdir(dest_base);
    if ((appLoc_fp = fopen("appLoc", "r")) == NULL) {
	fprintf(stdout, "%: can't fopen %s in dest_base area\n", xname, "appLoc");
	exit(1);
    }
    /* descend each directory in the dest_base/appLoc file */
    while ((fgets(buffer, sizeof(buffer) + 2, appLoc_fp)) != NULL) {
	len = strlen(buffer);
	/* remove the N/L from input line */
	if (buffer[len - 1] == '\n') {
	    buffer[len - 1] = 0;
	}
	ptr = (char *) strrchr(buffer, '/');
	*ptr = NULL;
	strcpy(top_node, "./");
	strcat(top_node, ++ptr);
	strcpy(src_base, buffer);
	status = chdir(src_base);
	procDirEntries(top_node, src_base);
    }
    fclose(appLoc_fp);
    sprintf(stderr, "makeApplShadow  completed\n");
    return (0);
}
/****************************************************************************
PROCESSFILE
	create a soft link and return
****************************************************************************/
int
processFile(name, dir)
    char           *name;	/* regular file */
    char           *dir;	/* current directory */
{
    /* always skip appLoc */
    if ((strcmp("./appLoc", name)) == SAME) {
        return;
    }
    strcpy(spath, src_base);
    strcat(spath, "/");
    strcat(spath, name);

    strcpy(dpath, dest_base);
    strcat(dpath, "/");
    strcat(dpath, name);
    createLink();
    return;
}
/****************************************************************************
PROCDIRENTRIES
    process directory entries
****************************************************************************/
void
procDirEntries(name, dir)
    char           *name;	/* entry name */
    char           *dir;	/* current directory */
{
    char           *ptr;
    struct stat     stbuf;
    if (lstat(name, &stbuf) == -1) {
	fprintf(stdout, "procDirEntries: can't access %s\n", name);
	return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFLNK) {
	procLink(name);
	return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFREG) {
	processFile(name, dir);
	return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
	/* access the last component (directory) in name */
	ptr = (char *) strrchr(name, '/');
	if ((strcmp(ptr, "/SCCS")) == SAME) {
	    /* dpath should be composed of - dest_base/SCCS */
	    strcpy(dpath, dest_base);
	    strcat(dpath, "/");
	    strcat(dpath, name);
	    strcpy(spath, src_base);
	    strcat(spath, "/");
	    strcat(spath, name);
	    createLink();
	    return;
	}
/* for other directories - create hard and continue dirwalk */
	strcpy(dpath, dest_base);
	strcat(dpath, "/");
	strcat(dpath, name);
	if ((mkdir(dpath, 0755)) != 0) {
	    printf("####################################################\n");
	    printf("procDirEntries: Can't mkdir %s\n", dpath);
	    printf("####################################################\n");
	}
	dirwalk(name, procDirEntries);
    }
    return;
}
/****************************************************************************
PROCLINK
****************************************************************************/
int
procLink(name)
    char           *name;
{

    char            buff[MAXPATHLEN];
    int             num_chars;

    /* always skip appLoc */
    if ((strcmp("./appLoc", name)) == SAME) {
printf("procLink: compare ./appLoc and %s\n",name);
        return;
    }
    if ((num_chars = readlink(name, buff, MAXPATHLEN)) < 0) {
	fprintf(stdout, "procLink: FATAL ERROR - errno=%d name=%s\n", errno, name);
	return;
    }
    buff[num_chars] = NULL;

    if ((buff[0] == '/') || ((buff[0] == '.') && (buff[1] == '/'))) {
	strcpy(spath, src_base);
	strcat(spath, "/");
	strcat(spath, name);
    } else {
	strcpy(spath, buff);
    }
    strcpy(dpath, dest_base);
    strcat(dpath, "/");
    strcat(dpath, name);
    createLink();
    return;
}
/****************************************************************************
CREATELINK
****************************************************************************/
void
createLink()
{
/*
     A symbolic link name2 is created to name1 (name2 is the name
     of  the  file  created, name1 is the string used in creating
     the symbolic link).  Either name may be  an  arbitrary  path
     name; the files need not be on the same file system.
 */
    if ((symlink(spath, dpath)) != 0) {
	printf("\n####################################################\n");
	printf("createLink:  symlink failure: errno=%d\nspath=%s\n\tdpath=%s\n"
	       ,errno, spath, dpath);
	printf("####################################################\n");
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
void
Usage()
{
    printf("\nUsage: %s <app_release_base>\n", xname);
    printf("ex: %s <path_to_AppMgr_release>/<Rx.x>\n", xname);
}

int
doTopNoDirs()
{
    char           *dir = ".";
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
	    continue;		/* skip self and parent */
	if (strlen(dir) + strlen(dp->d_name) + 2 > sizeof(name))
	    fprintf(stdout, "dirwalk: name %s/%s too long\n",
		    dir, dp->d_name);
	else {
	    sprintf(name, "%s/%s", dir, dp->d_name);
	    procNoTopDirs(name, dir);
	}
    }
    closedir(dfd);

}
int
doTopDirsNoAppDirs()
{
    char           *dir = ".";
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
	    continue;		/* skip self and parent */
	if (strlen(dir) + strlen(dp->d_name) + 2 > sizeof(name))
	    fprintf(stdout, "dirwalk: name %s/%s too long\n",
		    dir, dp->d_name);
	else {
	    sprintf(name, "%s/%s", dir, dp->d_name);
	    procNoAppDirs(name, dir);
	}
    }
    closedir(dfd);

}
void
procNoTopDirs(name, dir)
    char           *name;	/* entry name */
    char           *dir;	/* current directory */
{
    struct stat     stbuf;
    if (lstat(name, &stbuf) == -1) {
	fprintf(stdout, "procNoTopDirs: can't access %s\n", name);
	return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFLNK) {
	procLink(name);
	return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFREG) {
	processFile(name, dir);
	return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
	return;
    }
    return;
}

void procNoAppDirs(name, dir)
    char           *name;	/* entry name */
    char           *dir;	/* current directory */
{
    FILE           *appList;
    char           *ptr;
    struct stat     stbuf;
    char            appname[30];
    if (lstat(name, &stbuf) == -1) {
	fprintf(stdout, "procNoAppDirs: can't access %s\n", name);
	return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFLNK) {
	return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFREG) {
	return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
	chdir(appl_base);
	if ((appList = fopen("appList", "r")) == NULL) {
	    fprintf(stdout, "%: can't fopen %s in appl_base area\n", xname, "appList");
	    exit(1);
	}
	/* check for match */
	while ((fgets(buffer, sizeof(buffer) + 2, appList)) != NULL) {
	    len = strlen(buffer);
	    /* remove the N/L from input line */
	    if (buffer[len - 1] == '\n') {
		buffer[len - 1] = 0;
	    }
	    strcpy(appname, "./");
	    strcat(appname, buffer);
	    if ((strcmp(appname, name)) == SAME) {
		fclose(appList);
		return;
	    }
	}
	fclose(appList);
	/* descend the directories not listed in the appl_base/appList file */
	procDirEntries(name, dir);
	return;
    }
    return;
}

int
init_setup(argc, argv)
    int             argc;
    char          **argv;
{
    char           *ptr;
    int             len;
    FILE           *origin_fp;
    ptr = (char *) strrchr(argv[0], '/');
    if (ptr == 0) {
	/* no slash in program pathname */
	strcpy(xname, argv[0]);
    } else {
	strcpy(xname, ++ptr);
	--ptr;
    }
    if (*argv[0] != '/') {
	printf("####################################################\n");
	printf("%s: ERROR - You must invoke %s with a full pathname\n", xname, xname);
	printf("####################################################\n");
	exit(1);
    }
    /* derive application base from argv[0] */
    *ptr = NULL;
    /* remove /bin from invocation */
    ptr = (char *) strrchr(argv[0], '/');
    *ptr = NULL;
    len = strlen(argv[0]);
    strncpy(appl_base, argv[0], len);
    appl_base[len] = NULL;

    strcpy(appLocName, appl_base);
    strcat(appLocName, "/appLoc");
    if ((stat(appLocName, &stbuf)) != 0) {
	fprintf(stdout, "####################################################\n");
	fprintf(stdout, "%s: Can't stat file: %s\n", xname, appLocName);
	fprintf(stdout, "####################################################\n");
	Usage();
	exit(1);
    }
    /* get shadow base */
    if ((getcwd(dest_base, 78)) == NULL) {
	fprintf(stdout, "getcwd failed\n");
	return;
    }
    strcpy(src_base, appl_base);

    /* if .applShadow doesn't exist - require touch .applShadow */
    if ((stat("./.applShadow", &stbuf)) != 0) {
	fprintf(stdout, "\n");
	fprintf(stdout, "####################################################\n");
	fprintf(stdout, "%s: Can't stat file: './applShadow'\n", xname);
	fprintf(stdout, "####################################################\n");
	fprintf(stdout, "If this is NOT the root of your application shadow directory,\n");
	fprintf(stdout, "then please cd to the appropriate place and try again.\n");
	fprintf(stdout, "If this IS the root your shadow directory, then create .applShadow\n");
	fprintf(stdout, "with the following command and then run %s again.\n", xname);
	fprintf(stdout, "    %%\n touch .applShadow\n");
	fprintf(stdout, "####################################################\n");
	exit(1);
    }
/*********************** create/check an invocation marker *******************/
    if ((stat("./.applShadowOrgin", &stbuf)) != 0) {
	if ((origin_fp = fopen("./.applShadowOrgin", "w")) == NULL) {
	    fprintf(stdout, "%s: can't fopen %s in dest_base area\n",
		    xname, ".applShadowOrgin");
	    exit(1);
	}
	fprintf(origin_fp, "%s\n", appl_base);
    } else {
	if ((origin_fp = fopen("./.applShadowOrgin", "r")) == NULL) {
	    fprintf(stdout, "%s: can't fopen %s in dest_base area\n",
		    xname, ".applShadowOrgin");
	    exit(1);
	}
	/* compare appl_base with buffer */
	while ((fgets(buffer, sizeof(buffer) + 2, origin_fp)) != NULL) {
	len = strlen(buffer);
	/* remove the N/L from input line */
	if (buffer[len - 1] == '\n') {
	    buffer[len - 1] = 0;
	}
	    if ((strcmp(buffer, appl_base)) != SAME) {
		fprintf(stdout, "%s: FATAL ERROR - Illegal invocation\n", xname);
		fprintf(stdout,
			 "Your last invocation was from %s/bin\n", buffer);
		fprintf(stdout,
			 "Your current invocation is from %s/bin\n",appl_base);
		exit(1);
	    }
	}
#if 0 /* DEBUG */
	fprintf(stdout, "%s: WARNING - This is NOT the first run!\n",xname );
#endif
    }
    fclose(origin_fp);
/*********************** create a marker *******************/
    /* if appLoc doesn't exist - provide one */
    if ((stat("./appLoc", &stbuf)) != 0) {
	/* make a copy of appLocName */
	sprintf(command, "cp %s %s\n", appLocName, "appLoc");
	status = system(command);
	fprintf(stdout, "####################################################\n");
	fprintf(stdout, "I am providing you with a copy of the appLoc file\n");
	fprintf(stdout, "The appLoc file determines how many shadow applications\n");
	fprintf(stdout, "will be created in your shadow node\n");
	fprintf(stdout, "You may/should edit your copy of the appLoc file\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "Once you are satisfied with the contents of your appLoc file,\n");
	fprintf(stdout, "Please invoke this program again:\n");
	fprintf(stdout, "   %%\n %s/bin/%s\n", appl_base, xname);
	fprintf(stdout, "####################################################\n");
	exit(1);
    }
    return;
}
