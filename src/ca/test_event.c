/*	System includes		*/

#include 		<cadef.h>
#include 		<db_access.h>


void 	
ca_test_event(usr,chix,type,count,pval)
void	*usr;
chid	chix;
chtype	type;
int	count;
void	*pval;
{

  printf("~~~### in test event for [%s] ###~~~\n",chix+1);
  printf("user arg\t%x\n",usr);
  printf("val  ptr\t%x\n",pval);
  printf("mon type\t%x\n",type);
  printf("channel type\t%x\n",chix->type);
  switch(type){
  case	DBR_STRING:
    printf("Value:\t<%s>\n",pval);
    break;
  case	DBR_INT:
  case	DBR_ENUM:
    printf("Value:\t<%d>\n",*(int *)pval);
    break;
  case	DBR_FLOAT:
    printf("Value:\t<%f>\n",*(float *)pval);
    break;
  case	DBR_STS_STRING:
    printf("Value:\t<%s>\n",((struct dbr_sts_string *)pval)->value);
    break;
  case	DBR_STS_INT:
    printf("Value:\t<%d>\n",((struct dbr_sts_int *)pval)->value);
    break;
  case	DBR_STS_FLOAT:
    printf("Value:\t<%f>\n",((struct dbr_sts_float *)pval)->value);
    break;
  case	DBR_STS_ENUM:
    printf("Value:\t<%d>\n",((struct dbr_sts_enum *)pval)->value);
    break;
  case	DBR_GR_FLOAT:
    printf("Value:\t<%f>\n",((struct dbr_gr_float *)pval)->value);
    break;

  default:
    printf("Sorry test_event does not handle this type monitor yet\n");
  }
}

