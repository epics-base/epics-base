/*
 *	apCreateShadow.c
 *	todo - if symbolic link is relative 
 *		- it should be the same as the one in the source area
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

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
static char            buffer[MAXPATHLEN];
static char            dest_base[MAXPATHLEN];	/* dest base (root of shadow  node) */
static char            src_base[MAXPATHLEN];	/* current source descend root node */
static char            dpath[MAXPATHLEN];	/* dest/subdir... */
static char            spath[MAXPATHLEN];	/* src/subdir... */
static char            progName[30];	/* program name */
static char            sys_top[MAXPATHLEN];	/* root node */
static char            topAppl[MAXPATHLEN];/* pathname of master appLoc file */


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
/*
 * intial setup
 */
    init_setup(argc, argv);

    if (chdir(sys_top)!=0) {
        fprintf(stderr, "%s: Can't chdir\n", progName);
        exit(1);
    }
    startFromHere();

    fprintf(stderr, "apCreateShadow  %s\n", "completed");
    return (0);
}

/****************************************************************************
PROCLINK
****************************************************************************/
static void
procLink(name)
    char           *name;
{

    char            buff[MAXPATHLEN];
    int             num_chars;

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
static void
createLink()
{
/*
     A symbolic link name2 is created to name1 (name2 is the name
     of  the  file  created, name1 is the string used in creating
     the symbolic link).  Either name may be  an  arbitrary  path
     name; the files need not be on the same file system.
 */
if ( access(dpath,R_OK|F_OK)) {
    if ((symlink(spath, dpath)) != 0) {
	printf("\n####################################################\n");
	printf("createLink:  symlink failure: errno=%d\nspath=%s\n\tdpath=%s\n"
	       ,errno, spath, dpath);
	printf("####################################################\n");
    }
}
}
/****************************************************************************
DIRWALK applies a function to each file in a directory
****************************************************************************/
static void
dirwalk(dir, dbflag,  fcn)
    char           *dir;
    int             dbflag; /* in database directory flag*/
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
	    (*fcn) (name, dir, dbflag);
	}
    }
    closedir(dfd);
}
static void
Usage()
{
    printf("\nUsage:\t%s <root of application system area>\n", progName);
}

/****************************************************************************
PROCESSFILE
	create a soft link and return
****************************************************************************/
static void
processFile(name, indbdirflag)
    char           *name;	/* regular file */
    int             indbdirflag;/* in database directory flag */
{

    char           *ptr;
    char            cmd[2 * MAXNAMLEN];
    strcpy(spath, src_base);
    strcat(spath, "/");
    strcat(spath, name);

    strcpy(dpath, dest_base);
    strcat(dpath, "/");
    strcat(dpath, name);

    if ((ptr = (char *) strstr(name, "Db.")) != NULL
	    && indbdirflag != 0) {
	/* only link in <*>Db.database files */
	if ((ptr = (char *) strstr(name, "Db.database")) != NULL) {
	    createLink();
	} else {
	    return;
	}
    } else {
	createLink();
    }
    return;
}
/****************************************************************************
INIT_SETUP
****************************************************************************/
static void
init_setup(argc, argv)
    int             argc;
    char          **argv;
{
    char           *ptr;
    int             len;
    FILE           *origin_fp;
    char            Xdest_base[MAXPATHLEN];
    char            Xsrc_base[MAXPATHLEN];
    ptr = (char *) strrchr(argv[0], '/');
    if (ptr == 0) {
	strcpy(progName, argv[0]);
    } else {
	strcpy(progName, ptr + 1);
    }
    if (argc != 2) {
	printf("####################################################\n");
	Usage();
	if ((access(".applShadowOrgin",R_OK|F_OK) == 0)) {
	    system("/usr/5bin/echo \"previous invocation:\n\\tapCreateShadow \\c\";cat .applShadowOrgin");
	}
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
    /* if dest_base is substring of src_base - recursion occurs */
    strcpy(Xdest_base, dest_base);
    strcat(Xdest_base, "/");
    strcpy(Xsrc_base, src_base);
    strcat(Xsrc_base, "/");
    if ((strstr(Xdest_base, Xsrc_base)) != NULL) {	/* error */
	fprintf(stderr, "####################################################\n");
	fprintf(stderr, "%s: You can't run this tool in an application system area!\n", progName);
	fprintf(stderr, "####################################################\n");
	exit(1);
    }
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
			"Your new (arg1) system area is %s\n", sys_top);
		exit(1);
	    }
	}
    }
    fclose(origin_fp);
}

/*********************************************/
static void
startFromHere()
{
    char           *dir = ".";
    char            name[MAXNAMLEN];
    struct dirent  *dp;
    DIR            *dfd;
    int             localdbflag=0; /* in database directory flag*/
    if ((dfd = opendir(dir)) == NULL) {
	fprintf(stderr, "startFromHere: can't open %s\n", dir);
	exit(1);
    }
    while ((dp = readdir(dfd)) != NULL) {
	if (strcmp(dp->d_name, ".") == 0
		|| strcmp(dp->d_name, "..") == 0)
	    continue;		/* skip self and parent */
	if (strlen(dir) + strlen(dp->d_name) + 2 > sizeof(name))
	    fprintf(stderr, "startFromHere: name %s/%s too long\n",
		    dir, dp->d_name);
	else {
	    sprintf(name, "%s/%s", dir, dp->d_name);
	    procDirEntries(name,dir,localdbflag);
	}
    }
    printf("\n");
    closedir(dfd);
}
/****************************************************************************
PROCDIRENTRIES
    process directory entries
****************************************************************************/
static void
procDirEntries(name, dir, indbdirflag)
    char           *name;	/* entry name */
    char           *dir;
    int             indbdirflag; /* in database directory flag*/
{
    char           *ptr;
    char           *ptr2;
    struct stat     stbuf;
    int             localdbflag=0; /* in database directory flag*/
    if (lstat(name, &stbuf) == -1) {
	fprintf(stderr, "procDirEntries: can't access %s\n", name);
	exit(1);
    }
	printf("."); fflush(stdout);
    if ((stbuf.st_mode & S_IFMT) == S_IFLNK) {
	procLink(name); 
	return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFREG) {
	processFile(name,indbdirflag);
	return;
    }
    if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
	/* access the last component (directory) in name */
	ptr = (char *) strrchr(name, '/');
	if ((ptr2 = (char *) strstr(ptr, "Db")) != NULL ) {
	    localdbflag=1; /*set in database directory flag*/
	}
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
	if (access(dpath, R_OK | F_OK)) {
	    if ((mkdir(dpath, 0775)) != 0) {
		printf("####################################################\n");
		printf("procDirEntries: Can't mkdir %s - errno=%d\n", dpath, errno);
		printf("####################################################\n");
	    }
	}
	dirwalk(name, localdbflag,  procDirEntries);
    }
    return;
}
