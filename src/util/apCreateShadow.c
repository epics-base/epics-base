/*
 *	apCreateShadow.c
 * - todo

changed appLoc to appList ...


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

static void            Usage();
static void            procDirEntries();
static void            createLink();
static void            procLink();
static void            processFile();
static void            dirwalk();
static void            startFromHere();
static void            init_setup();

int             errno;
static struct stat     stbuf;
static char            buffer[BUFLEN];
static char            dest_base[80];	/* dest base (root of shadow  node) */
static char            src_base[80];	/* current source descend root node */
static char            dpath[80];	/* dest/subdir... */
static char            spath[80];	/* src/subdir... */
static char            progName[30];	/* program name */
static char            sys_top[80];	/* root node */
static char            topAppl[80];/* pathname of master appLoc file */


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
#if 0
    char           *ptr;
    char            top_node[30];
    FILE           *appLoc_fp;	/* user version of appLoc file */
    char            comLocName[80];	/* refs to all application common
					 * nodes */
#endif
/*
 * intial setup
 */
    init_setup(argc, argv);

    if (chdir(sys_top)!=0) {
        fprintf(stderr, "%s: Can't chdir\n", progName);
        exit(1);
    }
/* TODO */
/*fprintf(stderr,"%s: Creating a shadow node attached to %s",progName, xxx);*/
/* TODO */
    startFromHere();

    fprintf(stderr, "makeApplShadow  %s\n", "completed");
    return (0);
}

/****************************************************************************
PROCLINK
****************************************************************************/
void
procLink(name)
    char           *name;
{

    char            buff[MAXPATHLEN];
    int             num_chars;

    /* always skip appLoc */
    if ((strcmp("./appLoc", name)) == SAME) {
printf("procLink: compare ./appLoc and %s\n",name);
        exit(1);
    }
    if ((num_chars = readlink(name, buff, MAXPATHLEN)) < 0) {
	fprintf(stderr, "procLink: FATAL ERROR - errno=%d name=%s\n", errno, name);
	exit(1);
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
}
/****************************************************************************
DIRWALK applies a function to each file in a directory
****************************************************************************/
void
dirwalk(dir, fcn)
    char           *dir;
    void            (*fcn) ();
{
    char            name[MAXNAMLEN];
    struct dirent  *dp;
    DIR            *dfd;
    if ((dfd = opendir(dir)) == NULL) {
	fprintf(stderr, "dirwalk: can't open %s\n", dir);
	exit(1);
    }
    while ((dp = readdir(dfd)) != NULL) {
	if (strcmp(dp->d_name, ".") == 0
		|| strcmp(dp->d_name, "..") == 0)
	    continue;		/* skip self and parent */
	if (strlen(dir) + strlen(dp->d_name) + 2 > sizeof(name))
	    fprintf(stderr, "dirwalk: name %s/%s too long\n",
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
    printf("\nUsage:\t%s <appSystemTop>\n", progName);
    printf("\tor\n");
    printf("\t%s <appReleaseTop>\n", progName);
}

/****************************************************************************
PROCESSFILE
	create a soft link and return
****************************************************************************/
void
processFile(name)
    char           *name;	/* regular file */
{
    strcpy(spath, src_base);
    strcat(spath, "/");
    strcat(spath, name);

    strcpy(dpath, dest_base);
    strcat(dpath, "/");
    strcat(dpath, name);
    createLink();
}


void
init_setup(argc, argv)
    int             argc;
    char          **argv;
{
    char           *ptr;
    int             len;
    FILE           *origin_fp;
    ptr = (char *) strrchr(argv[0], '/');
    if (ptr == 0) {
	strcpy(progName, argv[0]);
    } else {
	strcpy(progName, ptr+1);
    }
if ( argc != 2 ) {
	printf("####################################################\n");
	Usage();
	printf("####################################################\n");
	exit(1); 
}


    if (*argv[1] != '/') {
	printf("####################################################\n");
	printf("%s: ERROR - arg#1 must be a full pathname\n", progName);
	Usage();
	printf("####################################################\n");
	exit(1);
    }

/* check to see if path points to app system area - file .topAppl exists*/
    /* application base is argv[1] */
    len = strlen(argv[1]);
    strncpy(sys_top, argv[1], len);
    sys_top[len] = NULL;

    strcpy(topAppl, sys_top);
    strcat(topAppl, "/.topAppl");
    if ((stat(topAppl, &stbuf)) != 0) {
	fprintf(stderr, "####################################################\n");
	fprintf(stderr, "%s: Can't stat file: %s\n", progName, topAppl);
	fprintf(stderr, "####################################################\n");
	Usage();
	exit(1);
    }
    /* get shadow base */
    if ((getcwd(dest_base, 78)) == NULL) {
	fprintf(stderr, "getcwd failed\n");
	exit(1);
    }
    strcpy(src_base, sys_top);

    /* if .applShadow doesn't exist - require touch .applShadow */
    if ((stat("./.applShadow", &stbuf)) != 0) {
	fprintf(stderr, "\n");
	fprintf(stderr, "####################################################\n");
	fprintf(stderr, "%s: Can't stat file: './applShadow'\n", progName);
	fprintf(stderr, "####################################################\n");
	fprintf(stderr, "If this is NOT the root of your application shadow directory,\n");
	fprintf(stderr, "then please cd to the appropriate place and try again.\n");
	fprintf(stderr, "If this IS the root your shadow directory, then create .applShadow\n");
	fprintf(stderr, "with the following command and then run %s again.\n", progName);
	fprintf(stderr, "    %%\n touch .applShadow\n");
	fprintf(stderr, "####################################################\n");
	exit(1);
    }
/*********************** create/check an invocation marker *******************/
/*rz*/
    if ((stat("./.applShadowOrgin", &stbuf)) != 0) {
	if ((origin_fp = fopen("./.applShadowOrgin", "w")) == NULL) {
	    fprintf(stderr, "%s: can't fopen %s in dest_base area\n",
		    progName, ".applShadowOrgin");
	    exit(1);
	}
	fprintf(origin_fp, "%s\n", sys_top);
    } else {
	if ((origin_fp = fopen("./.applShadowOrgin", "r")) == NULL) {
	    fprintf(stderr, "%s: can't fopen %s in dest_base area\n",
		    progName, ".applShadowOrgin");
	    exit(1);
	}
	/* compare sys_top with buffer */
	while ((fgets(buffer, sizeof(buffer) + 2, origin_fp)) != NULL) {
	len = strlen(buffer);
	/* remove the N/L from input line */
	if (buffer[len - 1] == '\n') {
	    buffer[len - 1] = 0;
	}
	    if ((strcmp(buffer, sys_top)) != SAME) {
fprintf(stderr, "%s: FATAL ERROR - Illegal invocation\n", progName);
		fprintf(stderr,
"Your last system area was %s\n", buffer);
		fprintf(stderr,
"Your new (arg1) system area is %s\n",sys_top);
		exit(1);
	    }
	}
#if 0 /* DEBUG */
	fprintf(stderr, "%s: WARNING - This is NOT the first run!\n",progName );
#endif
    }
    fclose(origin_fp);


#if 0 /* RZ COMMENT OUT */
/*********************** create a marker *******************/
    /* if appLoc doesn't exist - provide one */
    if ((stat("./appLoc", &stbuf)) != 0) {
	/* make a copy of topAppl */
	sprintf(command, "cp %s %s\n", topAppl, "appLoc");
	status = system(command);
	fprintf(stderr, "####################################################\n");
	fprintf(stderr, "I am providing you with a copy of the appLoc file\n");
	fprintf(stderr, "The appLoc file determines how many shadow applications\n");
	fprintf(stderr, "will be created in your shadow node\n");
	fprintf(stderr, "You may/should edit your copy of the appLoc file\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Once you are satisfied with the contents of your appLoc file,\n");
	fprintf(stderr, "Please invoke this program again:\n");
	fprintf(stderr, "   %%\n %s/bin/%s\n", sys_top, progName);
	fprintf(stderr, "####################################################\n");
	exit(1);
    }
#endif /* ENDIF RZ COMMENT OUT */
}

/*********************************************/
void
startFromHere()
{
    char           *dir = ".";
    char            name[MAXNAMLEN];
    struct dirent  *dp;
    DIR            *dfd;
    if ((dfd = opendir(dir)) == NULL) {
	fprintf(stderr, "dirwalk: can't open %s\n", dir);
	exit(1);
    }
    while ((dp = readdir(dfd)) != NULL) {
	if (strcmp(dp->d_name, ".") == 0
		|| strcmp(dp->d_name, "..") == 0)
	    continue;		/* skip self and parent */
	if (strlen(dir) + strlen(dp->d_name) + 2 > sizeof(name))
	    fprintf(stderr, "dirwalk: name %s/%s too long\n",
		    dir, dp->d_name);
	else {
	    sprintf(name, "%s/%s", dir, dp->d_name);
	    procDirEntries(name);
	}
    }
    closedir(dfd);

}
/****************************************************************************
PROCDIRENTRIES
    process directory entries
****************************************************************************/
void
procDirEntries(name)
    char           *name;	/* entry name */
{
    char           *ptr;
    struct stat     stbuf;
    if (lstat(name, &stbuf) == -1) {
	fprintf(stderr, "procDirEntries: can't access %s\n", name);
	exit(1);
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFLNK) {
	procLink(name);
	return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFREG) {
	processFile(name);
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
	    exit(1);
	}
	dirwalk(name, procDirEntries);
    }
    return;
}
