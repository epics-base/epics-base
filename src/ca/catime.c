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
#include		<vxWorks.h>
#ifdef vxWorks
#include		<taskLib.h>
#endif

#include 		<cadef.h>
#include 		<caerr.h>
#include		<db_access.h>

#define CA_TEST_CHNL	"AI_2000"


#ifdef vxWorks
spcatime()
{
  int acctst();

  return taskSpawn("acctst",200,VX_FP_TASK,20000,acctst); 
}
#endif

#define NUM 1

catime()
{
  chid			ai_1;

  long			status;
  long			i,j;
  void			*ptr;
  int			ca_array_put();
  int			ca_array_get();


  SEVCHK(ca_task_initialize(),"Unable to initialize");


  SEVCHK(ca_search(CA_TEST_CHNL,&ai_1),NULL);
  status = ca_pend_io(5.0);
  SEVCHK(status,NULL);
  if(status == ECA_TIMEOUT)
    exit();

  ptr = (void *) malloc(NUM*sizeof(union db_access_val)+NUM*MAX_STRING_SIZE);
  if(!ptr)
    exit();

  printf("channel name %s native type %d native count %d\n",ai_1+1,ai_1->type,ai_1->count);


  for(i=0;i<NUM;i++)
    ((float *)ptr)[i]=1.0;
  test(ai_1, "DBR_FLOAT", DBR_FLOAT, NUM, ptr);

  for(i=0;i<NUM;i++)
    strcpy((char *)ptr+(MAX_STRING_SIZE*i),"1.0");
  test(ai_1, "DBR_STRING", DBR_STRING, NUM, ptr);

  for(i=0;i<NUM;i++)
    ((int *)ptr)[i]=1;
  test(ai_1, "DBR_INT", DBR_INT, NUM, ptr);

  free(ptr);

  exit();
}


test(chix, name, type, count, ptr)
chid chix;
int type,count;
void *ptr;
{
  int test_put(), test_get(), test_wait();

  printf("%s\n",name);
  timex(test_put,chix, name, type, count, ptr);
  timex(test_get,chix, name, type, count, ptr);

  timexN(test_wait,chix, name, type, count, ptr);
}

test_put(chix, name, type, count, ptr)
chid chix;
int type,count;
void *ptr;
{
  int i;
  
  for(i=0; i< 10000;i++){
    SEVCHK(ca_array_put(type,count,chix,ptr),NULL);
  }
  SEVCHK(ca_flush_io(),NULL);
}
test_get(chix, name, type, count, ptr)
chid chix;
int type,count;
void *ptr;
{
  int i;

  for(i=0; i< 10000;i++){
    SEVCHK(ca_array_get(type,count,chix,ptr),NULL);
  }
  SEVCHK(ca_pend_io(0.0),NULL);
}


test_wait(chix, name, type, count, ptr)
chid chix;
int type,count;
void *ptr;
{

  SEVCHK(ca_array_get(type,count,chix,ptr),NULL);
  SEVCHK(ca_pend_io(0.0),NULL);
}

