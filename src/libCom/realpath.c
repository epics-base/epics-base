#if defined(__alpha) || defined(__hpux)
/*
 * file: realpath.c
 * 
 * this routine emulates the realpath() function found in SUNOS
 *
 *
 * NOTE: this function needs to be properly tested and verified on
 *   HPUX and OSF/1 to determine consistency with SunOS's version
 *
 *   N.B. - this was put together in a big hurry!
 *
 *   10/10/l94 (MDA)
 */

#include <unistd.h>
#include <sys/param.h>

char *realpath(char *path, char resolvedPath[MAXPATHLEN])
{
  char *returnPath;
  int status;
  char currentPath[MAXPATHLEN], buffer[MAXPATHLEN];

#ifdef DEBUG
printf(
"\n ---- realpath() - test and verify this (prototype) homegrown version!\n");
#endif

  resolvedPath[0] = '\0';

  if (!access(path,F_OK))  {
    if (path[0] == '.') {
    /* relative pathname */
      getcwd(currentPath,MAXPATHLEN);
      if (strlen(currentPath) > 0) {
	sprintf(buffer,"%s/%s",currentPath,path);
	status = readlink(buffer,resolvedPath,MAXPATHLEN);
	if (status < 0) {
	  returnPath = buffer;
        } else {
	  returnPath = resolvedPath;
	}
      } else {
	returnPath = NULL;
      }
    } else {
    /* full pathname */
      status = readlink(path,resolvedPath,MAXPATHLEN);
	if (status < 0) {
	  returnPath = path;
        } else {
	  returnPath = resolvedPath;
	}
    }
  } else {
    returnPath = NULL;
  }

  return(returnPath);
}



#ifdef TEST_REALPATH
main(int argc, char **argv)
{
  char path[MAXPATHLEN];
  char *returnPath;

  if (argc < 2) {
    printf("\n requires argument for path to be 'realpath()-ed'");
    exit(-1);
  }

  returnPath = realpath(argv[1], path);
  printf("\n realpath(%s) = %s\n",argv[1],returnPath);
  exit(0);
}
#endif


#else

/* for SUNs, using system supplied realpath() function */

#endif
