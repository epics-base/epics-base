/* recCalc.c */
/* share/src/rec $Id$ */

/* recCalc.c - Record Support Routines for Calculation records
 *
 * Author:      Julie Sander and Bob Dalesio
 * Date:        7-27-87
 *
 *	Control System Software for the GTA Project
 *
 *	Copyright 1988, 1989, the Regents of the University of California.
 *
 *	This software was produced under a U.S. Government contract
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory, which is
 *	operated by the University of California for the U.S. Department
 *	of Energy.
 *
 *	Developed by the Controls and Automation Group (AT-8)
 *	Accelerator Technology Division
 *	Los Alamos National Laboratory
 *
 *	Direct inqueries to:
 *	Bob Dalesio, AT-8, Mail Stop H820
 *	Los Alamos National Laboratory
 *	Los Alamos, New Mexico 87545
 *	Phone: (505) 667-3414
 *	E-mail: dalesio@luke.lanl.gov
 *
 * Modification Log:
 * -----------------
 * .01  5-18-88         lrd     modified modulo and power to avoid math library
 * .02  5-19-88         lrd     modified absolute value to avoid math library
 *                              defined unary math lib routines as doubles
 *                              removed include math.h
 *                              stopped loading dinglers math routines (ml)
 *                              wrote a random number generator to return a
 *                                      double between 0 and 1
 * .03  12-09-88        lrd     fixed modulo not to perform zero division
 * .04  12-12-88        lrd     lock the record while processing
 * .05  12-13-88        lrd     made an alarm for math error
 * .06  12-15-88        lrd     Process the forward scan link
 * .07  12-23-88        lrd     Alarm on locked MAX_LOCKED times
 * .08  01-11-89        lrd     Add Right and Left Shift
 * .09  02-01-89        lrd     Add Trig functions
 * .10  03-14-89        lrd     fix true on C question mark operator
 * .11  03-29-89        lrd     make hardware errors MAJOR
 *                              remove hw severity spec from database
 * .12  04-06-89        lrd     add monitor detection
 * .13  05-03-89        lrd     removed process mask from arg list
 * .14  06-05-89        lrd     check for negative square root
 * .15  08-01-89        lrd     full range of exponentiation using pow(x,y)
 * .16  04-04-90        lrd     fix post events for read and calc alarms
 *                              fix neg base raised to integer exponent
 * .17  04-06-90        lrd     change conditional to check for 0 and non-zero
 *                              instead of 0 and 1 (more 'C' like)
 * .18  10-10-90	mrk	Made changes for new record support
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<math.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<special.h>
#include	<calcRecord.h>
#include	<post.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
long init_record();
long process();
long special();
long get_value();
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
long get_units();
long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
long get_graphic_double();
long get_control_double();
long get_alarm_double();

struct rset calcRSET={
	RSETNUMBER,
	report,
	initialize,
	init_record,
	process,
	special,
	get_value,
	cvt_dbaddr,
	get_array_info,
	put_array_info,
	get_units,
	get_precision,
	get_enum_str,
	get_enum_strs,
	get_graphic_double,
	get_control_double,
	get_alarm_double };

void alarm();
void monitor();
int do_calc();
long postfix();
void fetch_values();

static long init_record(pcalc)
    struct calcRecord	*pcalc;
{
    long status;
    short error_number;
    char rpbuf[80];

    /* initialize so that first alarm, archive, and monitor get generated*/
    pcalc->lalm = 1e30;
    pcalc->alst = 1e30;
    pcalc->mlst = 1e30;

    if(pcalc->inpa.type==CONSTANT) pcalc->a = pcalc->inpa.value.value;
    if(pcalc->inpb.type==CONSTANT) pcalc->b = pcalc->inpb.value.value;
    if(pcalc->inpc.type==CONSTANT) pcalc->c = pcalc->inpc.value.value;
    if(pcalc->inpd.type==CONSTANT) pcalc->d = pcalc->inpd.value.value;
    if(pcalc->inpe.type==CONSTANT) pcalc->e = pcalc->inpe.value.value;
    if(pcalc->inpf.type==CONSTANT) pcalc->f = pcalc->inpf.value.value;
    status=postfix(pcalc->calc,rpbuf,&error_number);
    if(status) return(status);
    bcopy(rpbuf,pcalc->rpcl,sizeof(pcalc->rpcl));
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct calcRecord	*pcalc=(struct calcRecord *)(paddr->precord);

	pcalc->pact = TRUE;
	fetch_values(pcalc);
	if(do_calc(pcalc)) {
		if(pcalc->nsev<VALID_ALARM) {
			pcalc->nsta = CALC_ALARM;
			pcalc->nsev = VALID_ALARM;
		}
	}
	/* check for alarms */
	alarm(pcalc);
	/* check event list */
	monitor(pcalc);
	/* process the forward scan link record */
	if (pcalc->flnk.type==DB_LINK) dbScanPassive(pcalc->flnk.value.db_link.pdbAddr);
	pcalc->pact = FALSE;
	return(0);
}

static long special(paddr,after)
    struct dbAddr *paddr;
    int	   	  after;
{
    long		status;
    struct calcRecord  	*pcalc = (struct calcRecord *)(paddr->precord);
    int           	special_type = paddr->special;
    short error_number;
    char rpbuf[80];

    if(!after) return(0);
    switch(special_type) {
    case(SPC_CALC):
	status=postfix(pcalc->calc,rpbuf,&error_number);
	if(status) return(status);
	bcopy(rpbuf,pcalc->rpcl,sizeof(pcalc->rpcl));
	return(0);
    default:
	recGblDbaddrError(S_db_badChoice,paddr,"calc: special");
	return(S_db_badChoice);
    }
}

static long get_value(pcalc,pvdes)
    struct calcRecord		*pcalc;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_FLOAT;
    pvdes->no_elements=1;
    (float *)(pvdes->pvalue) = &pcalc->val;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    strncpy(units,pcalc->egu,sizeof(pcalc->egu));
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    *precision = pcalc->prec;
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    pgd->upper_disp_limit = pcalc->hopr;
    pgd->lower_disp_limit = pcalc->lopr;
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    pcd->upper_ctrl_limit = pcalc->hopr;
    pcd->lower_ctrl_limit = pcalc->lopr;
    return(0);
}
static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    pad->upper_alarm_limit = pcalc->hihi;
    pad->upper_warning_limit = pcalc->high;
    pad->lower_warning_limit = pcalc->low;
    pad->lower_alarm_limit = pcalc->lolo;
    return(0);
}


static void alarm(pcalc)
    struct calcRecord	*pcalc;
{
	float	ftemp;
	float	val=pcalc->val;

        /* if difference is not > hysterisis don't bother */
        ftemp = pcalc->lalm - pcalc->val;
        if(ftemp<0.0) ftemp = -ftemp;
        if (ftemp < pcalc->hyst) val=pcalc->lalm;

        /* alarm condition hihi */
        if (pcalc->nsev<pcalc->hhsv){
                if (val > pcalc->hihi){
                        pcalc->lalm = val;
                        pcalc->nsta = HIHI_ALARM;
                        pcalc->nsev = pcalc->hhsv;
                        return;
                }
        }

        /* alarm condition lolo */
        if (pcalc->nsev<pcalc->llsv){
                if (val < pcalc->lolo){
                        pcalc->lalm = val;
                        pcalc->nsta = LOLO_ALARM;
                        pcalc->nsev = pcalc->llsv;
                        return;
                }
        }

        /* alarm condition high */
        if (pcalc->nsev<pcalc->hsv){
                if (val > pcalc->high){
                        pcalc->lalm = val;
                        pcalc->nsta = HIGH_ALARM;
                        pcalc->nsev =pcalc->hsv;
                        return;
                }
        }

        /* alarm condition lolo */
        if (pcalc->nsev<pcalc->lsv){
                if (val < pcalc->low){
                        pcalc->lalm = val;
                        pcalc->nsta = LOW_ALARM;
                        pcalc->nsev = pcalc->lsv;
                        return;
                }
        }
        return;
}

static void monitor(pcalc)
    struct calcRecord	*pcalc;
{
	unsigned short	monitor_mask;
	float		delta;
        short           stat,sevr,nsta,nsev;
	float		*pnew;
	float		*pprev;
	int		i;

        /* get previous stat and sevr  and new stat and sevr*/
        stat=pcalc->stat;
        sevr=pcalc->sevr;
        nsta=pcalc->nsta;
        nsev=pcalc->nsev;
        /*set current stat and sevr*/
        pcalc->stat = nsta;
        pcalc->sevr = nsev;
        pcalc->nsta = 0;
        pcalc->nsev = 0;

        /* anyone waiting for an event on this record */
        if (pcalc->mlis.count == 0) return;

        /* Flags which events to fire on the value field */
        monitor_mask = 0;

        /* alarm condition changed this scan */
        if (stat!=nsta || sevr!=nsev) {
                /* post events for alarm condition change*/
                monitor_mask = DBE_ALARM;
                /* post stat and nsev fields */
                db_post_events(pcalc,&pcalc->stat,DBE_VALUE);
                db_post_events(pcalc,&pcalc->sevr,DBE_VALUE);
        }
        /* check for value change */
        delta = pcalc->mlst - pcalc->val;
        if(delta<0.0) delta = -delta;
        if (delta > pcalc->mdel) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                pcalc->mlst = pcalc->val;
        }
        /* check for archive change */
        delta = pcalc->alst - pcalc->val;
        if(delta<0.0) delta = 0.0;
        if (delta > pcalc->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                pcalc->alst = pcalc->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pcalc,&pcalc->val,monitor_mask);
        }
	/* check all input fields for changes*/
	for(i=0, pnew=&pcalc->a, pprev=&pcalc->la; i<6; i++, pnew++, pprev++) {
		if(*pnew != *pprev) {
			db_post_events(pcalc,pnew,monitor_mask|DBE_VALUE);
			*pprev = *pnew;
		}
	}
        return;
}

static void fetch_values(pcalc)
struct calcRecord *pcalc;
{
	struct link	*plink;	/* structure of the link field  */
	float		*pvalue;
	long		options,nRequest;
	int		i;

	for(i=0, plink=&pcalc->inpa, pvalue=&pcalc->a; i<6; i++, plink++, pvalue++) {
		if(plink->type!=DB_LINK) continue;
		options=0;
		nRequest=1;
		(void)dbGetLink(&plink->value.db_link,pcalc,DBR_FLOAT,
			pvalue,&options,&nRequest);
	}
	return;
}

/* the floating point math routines need to be declared as doubles */
static double   random();       /* random number generator      */
double  sqrt(),log(),log10();
double  acos(),asin(),atan();
double  cos(),sin(),tan();
double  cosh(),sinh(),tanh();

/*
 *	DO_CALC
 *
 *	execute the calculation
 */
#define	NOT_SET		0
#define	TRUE_COND	1
#define	FALSE_COND	2

static int do_calc(pcalc)
struct calcRecord *pcalc;  /* pointer to calculation record  */
{
	char	*post;		/* postfix expression	*/
	double	*pstacktop;	/* stack of values	*/
	double	stack[80];
	int	temp;
	short	i;
	double	*top;
	int	itop;		/* integer top value	*/
	int	inexttop;	/* ineteger next to top value 	*/
	short	cond_flag;	/* conditional else flag	*/

	/* initialize flag  */
	cond_flag = NOT_SET;
	pstacktop = &stack[0];

	/* set post to postfix expression in calc structure */
	post = &pcalc->rpcl[0];
	top = pstacktop;

	/* polish calculator loop */
	while (*post != END_STACK){
		switch (*post){
		case FETCH_A:
			++pstacktop;
			*pstacktop = pcalc->a;
			break;

		case FETCH_B:
			++pstacktop;
			*pstacktop = pcalc->b;
			break;

		case FETCH_C:
			++pstacktop;
			*pstacktop = pcalc->c;
			break;

		case FETCH_D:
			++pstacktop;
			*pstacktop = pcalc->d;
			break;

		case FETCH_E:
			++pstacktop;
			*pstacktop = pcalc->e;
			break;

		case FETCH_F:
			++pstacktop;
			*pstacktop = pcalc->f;
			break;

		case ADD:
			--pstacktop;
			*pstacktop = *pstacktop + *(pstacktop+1);
			break;

		case SUB:
			--pstacktop;
			*pstacktop = *pstacktop - *(pstacktop+1);
			break;

		case MULT:
			--pstacktop;
			*pstacktop = *pstacktop * *(pstacktop+1);
			break;

		case DIV:
			--pstacktop;
			if (*(pstacktop+1) == 0) /* can't divide by zero */
				return(-1);
			*pstacktop = *pstacktop / *(pstacktop+1);
			break;

		case COND_ELSE:
			/* first conditional set cond_flag */
			/* true */
			if ((*pstacktop != 0.0) && (cond_flag == NOT_SET)){
				cond_flag  = TRUE_COND;
				--pstacktop;		/* remove condition */
			/* false */
			}else if ((*pstacktop==0.0) && (cond_flag==NOT_SET)){
				cond_flag  = FALSE_COND;
				--pstacktop;		/* remove condition */
				/* check for else condition */
				i = 1;
				while (*(post+i) != COND_ELSE){
					/* no else value */
					if (*(post+i) == END_STACK){
						/* skip to end of expression */
						while (*(post+1) != END_STACK)
							++post;
						/* use last value as result */
						++pstacktop;
						*pstacktop = pcalc->val;
					}
					i++;
				}
			}else if (cond_flag == TRUE_COND){
				/* skip expression - result is on stack */
				while ((*(post+1) != COND_ELSE)
				  && (*(post+1) != END_STACK))
					++post;
			}else if (cond_flag == FALSE_COND){
				/* remove true answer from stack top */
				--pstacktop;
			}
			break;
				
		case ABS_VAL:
			if (*pstacktop < 0) *pstacktop = -*pstacktop;
			break;

		case UNARY_NEG:
			*pstacktop = -1* (*pstacktop);
			break;

		case SQU_RT:
			if (*pstacktop < 0) return(-1);	/* undefined */
			*pstacktop = sqrt(*pstacktop);
			break;

		case LOG_10:
			*pstacktop = log10(*pstacktop);
			break;

		case LOG_E:
			*pstacktop = log(*pstacktop);
			break;

		case RANDOM:
			++pstacktop;
			*pstacktop = random();
			break;

		case EXPON:
			--pstacktop;
			if (*pstacktop < 0){
				temp = (int) *(pstacktop+1);
				/* is exponent an integer */
				if ((*(pstacktop+1) - (double)temp) != 0) return (-1);
        			*pstacktop = exp(*(pstacktop+1) * log(-*pstacktop));
				/* is value negative */
				if ((temp % 2) > 0) *pstacktop = -*pstacktop;
			}else{
        			*pstacktop = exp(*(pstacktop+1) * log(*pstacktop));
			}
			break;

		case MODULO:
			--pstacktop;
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop+1);
			if (inexttop == 0)
				return(-1);
			i =  itop % inexttop;
			*pstacktop = i;
			break;

		case REL_OR:
			--pstacktop;
			*pstacktop = (*pstacktop || *(pstacktop+1));
			break;

		case REL_AND:
			--pstacktop;
			*pstacktop = (*pstacktop && *(pstacktop+1));
			break;

		case BIT_OR:
			/* force double values into integers and or them */
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop-1);
			--pstacktop;
			*pstacktop = (inexttop | itop);
			break;

		case BIT_AND:
			/* force double values into integers and and them */
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop-1);
			--pstacktop;
			*pstacktop = (inexttop & itop);
			break;

		case BIT_EXCL_OR:
			/*force double values to integers to exclusive or them*/
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop-1);
			--pstacktop;
			*pstacktop = (inexttop ^ itop);
			break;

		case GR_OR_EQ:
			--pstacktop;
			*pstacktop = *pstacktop >= *(pstacktop+1);
			break;

		case GR_THAN:
			--pstacktop;
			*pstacktop = *pstacktop > *(pstacktop+1);
			break;

		case LESS_OR_EQ:
			--pstacktop;
			*pstacktop = *pstacktop <= *(pstacktop+1);
			break;

		case LESS_THAN:
			--pstacktop;
			*pstacktop = *pstacktop < *(pstacktop+1);
			break;

		case NOT_EQ:
			--pstacktop;
			*pstacktop = *pstacktop != *(pstacktop+1);
			break;

		case EQUAL:
			--pstacktop;
			*pstacktop = (*pstacktop == *(pstacktop+1));
			break;

		case RIGHT_SHIFT:
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop-1);
			--pstacktop;
			*pstacktop = (inexttop >> itop);
			break;

		case LEFT_SHIFT:
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop-1);
			--pstacktop;
			*pstacktop = (inexttop << itop);
			break;

		case ACOS:
			*pstacktop = acos(*pstacktop);
			break;

		case ASIN:
			*pstacktop = asin(*pstacktop);
			break;

		case ATAN:
			*pstacktop = atan(*pstacktop);
			break;

		case COS:
			*pstacktop = cos(*pstacktop);
			break;

		case SIN:
			*pstacktop = sin(*pstacktop);
			break;

		case TAN:
			*pstacktop = tan(*pstacktop);
			break;

		case COSH:
			*pstacktop = cosh(*pstacktop);
			break;

		case SINH:
			*pstacktop = sinh(*pstacktop);
			break;

		case TANH:
			*pstacktop = tanh(*pstacktop);
			break;

		default:
			printf("%d bad expression element\n",*post);
			break;
		}

		/* move ahead in postfix expression */
		++post;
	}

	/* if everything is peachy,the stack should end at its first position */
	if (++top == pstacktop)
		pcalc->val = *pstacktop;
	else {
		if(pcalc->nsev < MAJOR_ALARM) {
			pcalc->nsev = MAJOR_ALARM;
			pcalc->nsta = CALC_ALARM;
		}
	}
	return(0);
}

/*
 * RANDOM
 *
 * generates a random number between 0 and 1 using the
 * seed = (multy * seed) + addy		Random Number Generator by Knuth
 *						SemiNumerical Algorithms
 *						Chapter 1
 * randy = 1.0 / (seed & 0xff)		To normalize the number between 0 - 1
 */
static unsigned short seed = 0xa3bf;
static unsigned short multy = 191 * 8 + 5;  /* 191 % 8 == 5 */
static unsigned short addy = 0x3141;
static double random()
{
	double	randy;

	/* random number */
	seed = (seed * multy) + addy;
	randy = 1.0 / (seed & 0xff);

	/* between 0 - 1 */
	return(randy);
}
