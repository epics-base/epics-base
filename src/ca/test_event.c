/*
 *
 *	T E S T _ E V E N T . C
 *	Author:	Jeffrey O. Hill
 *	simple stub for testing monitors
 *
 *	
 *	History
 *	joh 	031891	printed type in decimanl instead of hex
 *
 *
 *
 *
 */

/*	System includes		*/

#include 		<cadef.h>
#include 		<db_access.h>


void 	
ca_test_event(args)
struct event_handler_args	args;
{
  ca_printf("~~~### in test event for [%s] ###~~~\n",args.chid+1);
  ca_printf("User argument\t%x\n", args.usr);
  ca_printf("Native channel data type\t%d\n", args.chid->type);
  ca_printf("Monitor data type\t%d\n", args.type);
  switch(args.type){
  case	DBR_STRING:
    ca_printf("Value:\t<%s>\n",args.dbr);
    break;
  case	DBR_CHAR:
    ca_printf("Value:\t<%d>\n",*(char *)args.dbr);
    break;
#if DBR_INT != DBR_SHORT
  case	DBR_INT:
#endif
  case	DBR_SHORT:
  case	DBR_ENUM:
    ca_printf("Value:\t<%d>\n",*(short *)args.dbr);
    break;
  case	DBR_LONG:
    ca_printf("Value:\t<%d>\n",*(long *)args.dbr);
    break;
  case	DBR_FLOAT:
    ca_printf("Value:\t<%f>\n",*(float *)args.dbr);
    break;
  case	DBR_DOUBLE:
    ca_printf("Value:\t<%f>\n",*(double *)args.dbr);
    break;
  case	DBR_STS_STRING:
    ca_printf("Value:\t<%s>\n",((struct dbr_sts_string *)args.dbr)->value);
    break;
  case	DBR_STS_INT:
    ca_printf("Value:\t<%d>\n",((struct dbr_sts_int *)args.dbr)->value);
    break;
  case	DBR_STS_FLOAT:
    ca_printf("Value:\t<%f>\n",((struct dbr_sts_float *)args.dbr)->value);
    break;
  case	DBR_STS_ENUM:
    ca_printf("Value:\t<%d>\n",((struct dbr_sts_enum *)args.dbr)->value);
    break;
  case	DBR_GR_FLOAT:
    ca_printf("Value:\t<%f>\n",((struct dbr_gr_float *)args.dbr)->value);
    break;
  default:
    ca_printf(	"Sorry test_event does not handle data type %d yet\n",
		args.type);
  }
}

