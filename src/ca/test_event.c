/*	System includes		*/

#include 		<cadef.h>
#include 		<db_access.h>


void 	
ca_test_event(args)
struct event_handler_args	args;
{

  printf("~~~### in test event for [%s] ###~~~\n",args.chid+1);
  printf("User argument\t%x\n", args.usr);
  printf("Native channel data type\t%x\n", args.chid->type);
  printf("Monitor data type\t%x\n", args.type);
  switch(args.type){
  case	DBR_STRING:
    printf("Value:\t<%s>\n",args.dbr);
    break;
  case	DBR_INT:
  case	DBR_ENUM:
    printf("Value:\t<%d>\n",*(int *)args.dbr);
    break;
  case	DBR_FLOAT:
    printf("Value:\t<%f>\n",*(float *)args.dbr);
    break;
  case	DBR_STS_STRING:
    printf("Value:\t<%s>\n",((struct dbr_sts_string *)args.dbr)->value);
    break;
  case	DBR_STS_INT:
    printf("Value:\t<%d>\n",((struct dbr_sts_int *)args.dbr)->value);
    break;
  case	DBR_STS_FLOAT:
    printf("Value:\t<%f>\n",((struct dbr_sts_float *)args.dbr)->value);
    break;
  case	DBR_STS_ENUM:
    printf("Value:\t<%d>\n",((struct dbr_sts_enum *)args.dbr)->value);
    break;
  case	DBR_GR_FLOAT:
    printf("Value:\t<%f>\n",((struct dbr_gr_float *)args.dbr)->value);
    break;

  default:
    printf(	"Sorry test_event does not handle data type %d yet\n",
		args.type);
  }
}

