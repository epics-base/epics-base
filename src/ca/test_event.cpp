/*
 *
 *	T E S T _ E V E N T . C
 *	Author:	Jeffrey O. Hill
 *	simple stub for testing monitors
 *
 *	
 *	History
 *	joh 	031891	printed type in decimal instead of hex
 *	joh	072792	better messages	
 *
 *
 *
 *
 */

#include	"iocinf.h"


void epicsShareAPI ca_test_event(struct event_handler_args args)
{
  printf ("CAC: ~~~### in test event for [%s] ###~~~\n", ca_name(args.chid));
  printf ("CAC: User argument\t%p\n", args.usr);
  printf ("CAC: Native channel data type\t%d\n", ca_field_type(args.chid));
  printf ("CAC: Monitor data type\t%ld\n", args.type);
  printf ("CAC: CA Status \"%s\"\n", ca_message(args.status));

  if(!args.dbr || !(CA_M_SUCCESS&args.status)){
	return;
  }

  switch(args.type){
  case	DBR_STRING:
    printf ("CAC: Value:\t<%s>\n", (const char *)args.dbr);
    break;
  case	DBR_CHAR:
    printf ("CAC: Value:\t<%d>\n",*(char *)args.dbr);
    break;
#if DBR_INT != DBR_SHORT
  case	DBR_INT:
#endif
  case	DBR_SHORT:
  case	DBR_ENUM:
    printf ("CAC: Value:\t<%d>\n",*(short *)args.dbr);
    break;
  case	DBR_LONG:
    printf ("CAC: Value:\t<%ld>\n",*(long *)args.dbr);
    break;
  case	DBR_FLOAT:
    printf ("CAC: Value:\t<%f>\n",*(float *)args.dbr);
    break;
  case	DBR_DOUBLE:
    printf ("CAC: Value:\t<%f>\n",*(double *)args.dbr);
    break;
  case	DBR_STS_STRING:
    printf ("CAC: Value:\t<%s>\n",((struct dbr_sts_string *)args.dbr)->value);
    break;
  case	DBR_STS_INT:
    printf ("CAC: Value:\t<%d>\n",((struct dbr_sts_int *)args.dbr)->value);
    break;
  case	DBR_STS_FLOAT:
    printf ("CAC: Value:\t<%f>\n",((struct dbr_sts_float *)args.dbr)->value);
    break;
  case	DBR_STS_ENUM:
    printf ("CAC: Value:\t<%d>\n",((struct dbr_sts_enum *)args.dbr)->value);
    break;
  case	DBR_GR_FLOAT:
    printf ("CAC: Value:\t<%f>\n",((struct dbr_gr_float *)args.dbr)->value);
    break;
  default:
    printf (	"CAC: Sorry test_event does not handle data type %ld yet\n",
		args.type);
  }
}

