
/*
 * $Id$
 *
 * Author: Jim Kowalkowski
 * Date:   10/22/96
 *
 * $Log$
 *
*/

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

void print_error(char* prog);

main(int argc, char** argv)
{
	int uid=getuid();
	int nval,fval,n=0,f=0,pos=1,opos;
	int uid_changed=0,sys_max;
	char** args;
	struct rlimit lim;

	if(argc<2 || strcmp(argv[1],"-?")==0 ||
	   strcmp(argv[1],"help")==0 || strcmp(argv[1],"-h")==0)
	{
		fprintf(stderr,"Usage: %s [-files value] [-nice value]",argv[0]);
		fprintf(stderr," program_to_run [program options]\n\n");
		fprintf(stderr,"  -files : Change the maximum number of file ");
		fprintf(stderr,"descriptors the program can open.\n");
		fprintf(stderr,"  -nice : Change the priority of the program ");
		fprintf(stderr,"up or down by this value\n\n");
		fprintf(stderr,"The priority value range is [-20,20).  The lower a\n");
		fprintf(stderr,"program's nice value is, the higher priority it\n");
		fprintf(stderr,"runs at.  So -nice -20 results in the highest\n");
		fprintf(stderr,"priority and -nice 19 results in the lowest.\n\n");
		return 1;
	}

	do
	{
		opos=pos;

		if(strcmp(argv[pos],"-nice")==0)
		{
			n=1;
			if(sscanf(argv[++pos],"%d",&nval)!=1)
			{
				fprintf(stderr,"Improper -nice value entered\n");
				return 1;
			}
			++pos;
		}
		if(strcmp(argv[pos],"-files")==0)
		{
			f=1;
			if(sscanf(argv[++pos],"%d",&fval)!=1)
			{
				fprintf(stderr,"Improper -file value entered\n");
				return 1;
			}
			++pos;
		}
	}
	while(pos!=opos && pos<argc);

	if(pos==argc)
	{
		fprintf(stderr,"No program entered, nothing to run\n");
		return 1;
	}

	if(f==0 && n==0)
		fprintf(stderr,"Nothing changed, just running the program\n");

	if(setuid(0)<0)
	{
		/*
		fprintf(stderr,"Cannot change user to root, %s improperly installed,\n",
			argv[0]);
		fprintf(stderr," Program must be installed as root and afterwards\n");
		fprintf(stderr," the command \"chmod +s %s\" must be run\n",argv[0]);
		return 1;

		fprintf(stderr,"Not a root owned program, priority changes will be ");
		fprintf(stderr,"limited.\nFD changes may also be limited.\n");
		*/
	}
	else
		uid_changed=1;

	if(n && nice(nval)<0)
	{
		fprintf(stderr,"Cannot change the priority to %d\n\n",nval);
		if(!uid_changed)
			print_error(argv[0]);
		return 1;
	}

	if(f)
	{
		if(getrlimit(RLIMIT_NOFILE,&lim)<0)
		{
			fprintf(stderr,"Failed to get FD limits\n");
			return 1;
		}

		lim.rlim_cur=fval;
		sys_max=lim.rlim_max;

		if(uid_changed)
			lim.rlim_max=fval;

		/* lim.rlim_max=RLIM_INFINITY; */

		if(setrlimit(RLIMIT_NOFILE,&lim)<0)
		{
			fprintf(stderr,"Failed to set FD max to %d\n",fval);
			fprintf(stderr,"System maximum FD hard limit is %d\n",sys_max);
			print_error(argv[0]);
			return 1;
		}
	}

	if(uid_changed)
		setuid(uid);

	args=(char**)malloc((argc-pos)*sizeof(char*));
	for(n=0;n<(argc-pos);n++) args[n]=argv[pos+n];
	args[n]=NULL;

	if(execvp(argv[pos],args)<0)
		fprintf(stderr,"Cannot execute program\n");

	return 0;
}

void print_error(char* prog)
{
	fprintf(stderr,"\nThe user ID could not be changed to root,\n");
	fprintf(stderr,"so the priority or files you entered is not allowed.\n");
	fprintf(stderr,"You can currently only reduce your priority and cannot\n");
	fprintf(stderr,"go beyond the FD hard limit.  Contact your admin and\n");
	fprintf(stderr,"have the program ");
	fprintf(stderr,"installed as a root owned, setuid bit program\n");
	fprintf(stderr,"and you will be able to increase priorities\n");
	fprintf(stderr,"\nHere is how to install this program:\n");
	fprintf(stderr," su root  (and enter the password)\n");
	fprintf(stderr," chown root %s\n",prog);
	fprintf(stderr," chmod +s %s\n",prog);
}
