/*


	CA performance test 

	History 
	joh	09-12-89	Initial release



*/

# ifdef VMS
  lib$init_timer();
# endif
# ifdef VMS
  lib$show_timer();
#endif


/*	System includes		*/
#ifdef vxWorks
#include		<vxWorks.h>
#include		<taskLib.h>
#endif

#include 		<cadef.h>
#include 		<caerr.h>
#include		<db_access.h>

#ifndef OK
#define OK 0
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef min
#define min(A,B) ((A)>(B)?(B):(A))
#endif

#ifndef NELEMENTS 
#define NELEMENTS(A) (sizeof (A) / sizeof ((A) [0]))
#endif

#define NUM 1
#define ITERATION_COUNT 10000

#define WAIT_FOR_ACK

chid chan_list[min(10000,ITERATION_COUNT)];

#ifndef vxWorks
main(argc, argv)
int	argc;
char	**argv;
{
	char	*pname;

	if(argc == 2){
		pname = argv[1];	
		catime(pname);
	}
	else{
		printf("usage: %s <channel name>", argv[0]);
	}
}
#endif

catime(channelName)
char     *channelName;
{
  chid			ai_1;

  long			status;
  long			i,j;
  void			*ptr;
  int			test_search(), test_free();

  SEVCHK(ca_task_initialize(),"Unable to initialize");


  SEVCHK(ca_search(channelName,&ai_1),NULL);
  status = ca_pend_io(5.0);
  SEVCHK(status,NULL);
  if(status == ECA_TIMEOUT)
    exit(OK);

  ptr = (void *) malloc(NUM*sizeof(union db_access_val)+NUM*MAX_STRING_SIZE);
  if(!ptr)
    exit(OK);

  printf("channel name %s native type %d native count %d\n",ai_1+1,ai_1->type,ai_1->count);
  printf("search test\n");
  timex(test_search,channelName);
  printf("free test\n");
  timex(test_free,ai_1);


  for(i=0;i<NUM;i++)
    ((float *)ptr)[i] = 0.0;
  test(ai_1, "DBR_FLOAT", DBR_FLOAT, NUM, ptr);

  for(i=0;i<NUM;i++)
    strcpy((char *)ptr+(MAX_STRING_SIZE*i),"0.0");
  test(ai_1, "DBR_STRING", DBR_STRING, NUM, ptr);

  for(i=0;i<NUM;i++)
    ((int *)ptr)[i]=0;
  test(ai_1, "DBR_INT", DBR_INT, NUM, ptr);

  free(ptr);

  exit(OK);
}


test(chix, name, type, count, ptr)
chid chix;
char *name;
int type,count;
void *ptr;
{
  int test_put(), test_get(), test_wait();

  printf("%s\n",name);
  timex(test_put,chix, name, type, count, ptr);
  timex(test_get,chix, name, type, count, ptr);
  timex(test_wait,chix, name, type, count, ptr);
}

#ifndef vxWorks
timex(pfunc, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
void (*pfunc)(); 
int arg1;
int arg2; 
int arg3; 
int arg4; 
int arg5; 
int arg6; 
int arg7;
{
	(*pfunc)(arg1, arg2, arg3, arg4, arg5, arg6, arg7);

}
#endif

test_search(name)
char *name;
{
  int i;

  for(i=0; i< NELEMENTS(chan_list);i++){
    SEVCHK(ca_search(name,&chan_list[i]),NULL);
  }
  SEVCHK(ca_pend_io(0.0),NULL);
}

test_free(chix)
chid chix;
{
  int 			i;
  union db_access_val	ival;

  for(i=0; i< NELEMENTS(chan_list);i++){
    SEVCHK(ca_clear_channel(chan_list[i]),NULL);
  }
#ifdef WAIT_FOR_ACK
  SEVCHK(ca_array_get(DBR_INT,1,chix,&ival),NULL);
  SEVCHK(ca_pend_io(0.0),NULL);
#else
  SEVCHK(ca_flush_io(),NULL);
#endif
}

test_put(chix, name, type, count, ptr)
chid chix;
char *name;
int type,count;
void *ptr;
{
  int i;
  
  for(i=0; i< ITERATION_COUNT;i++){
    SEVCHK(ca_array_put(type,count,chix,ptr),NULL);
#if 0
    ca_flush_io();
#endif
  }
#ifdef WAIT_FOR_ACK
  SEVCHK(ca_array_get(type,count,chix,ptr),NULL);
  SEVCHK(ca_pend_io(0.0),NULL);
#else
  SEVCHK(ca_flush_io(),NULL);
#endif
}

test_get(chix, name, type, count, ptr)
chid chix;
char *name;
int type,count;
void *ptr;
{
  int i;

  for(i=0; i< ITERATION_COUNT;i++){
    SEVCHK(ca_array_get(type,count,chix,ptr),NULL);
#if 0
    ca_flush_io();
#endif
  }
  SEVCHK(ca_pend_io(0.0),NULL);
}


test_wait(chix, name, type, count, ptr)
chid chix;
char *name;
int type,count;
void *ptr;
{
  int i;

  for(i=0; i< ITERATION_COUNT;i++){
    SEVCHK(ca_array_get(type,count,chix,ptr),NULL);
    SEVCHK(ca_pend_io(0.0),NULL);
  }
}

