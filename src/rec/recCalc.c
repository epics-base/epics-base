
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
#include	<cvtTable.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<special.h>
#include	<calcRecord.h>

/* Create RSET - Record Support Entry Table*/
long report();
#define initialize NULL
long init_record();
long process();
long special();
long get_precision();
long get_value();
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_enum_str NULL
long get_units();
long get_graphic_double();
long get_control_double();
#define get_enum_strs NULL

struct rset calcRSET={
	RSETNUMBER,
	report,
	initialize,
	init_record,
	process,
	special,
	get_precision,
	get_value,
	cvt_dbaddr,
	get_array_info,
	put_array_info,
	get_enum_str,
	get_units,
	get_graphic_double,
	get_control_double,
	get_enum_strs };

/* the floating point math routines need to be declared as doubles */
static double   random();       /* random number generator      */
double  sqrt(),log(),log10();
double  acos(),asin(),atan();
double  cos(),sin(),tan();
double  cosh(),sinh(),tanh();

/*	defines for element table      */
#define 	FETCH_A		0
#define		FETCH_B		1
#define		FETCH_C		2
#define		FETCH_D		3
#define		FETCH_E		4
#define		FETCH_F		5
#define		ACOS		6
#define		ASIN		7
#define		ATAN		8
#define		COS		9
#define		COSH		10
#define		SIN		11
#define		STORE_A		12
#define		STORE_B		13
#define		STORE_C		14
#define		STORE_D		15
#define		STORE_E		16
#define		STORE_F		17
#define		RIGHT_SHIFT	18
#define		LEFT_SHIFT	19
#define		SINH		20
#define		TAN		21
#define		TANH		22
#define		LOG_2		23
#define		COND_ELSE	24
#define		ABS_VAL		25
#define		UNARY_NEG	26
#define		SQU_RT		27
#define		LOG_10		28
#define		LOG_E		29
#define		RANDOM		30
/*
#define				31
*/
#define		ADD		32
#define		SUB		33
#define		MULT		34
#define		DIV		35
#define		EXPON		36
#define		MODULO		37
#define		BIT_OR		38
#define		BIT_AND		39
#define		BIT_EXCL_OR	40
#define		GR_OR_EQ	41
#define		GR_THAN		42
#define		LESS_OR_EQ	43
#define		LESS_THAN	44
#define		NOT_EQ		45
#define		EQUAL		46
#define		REL_OR		47
#define		REL_AND		48
#define		PAREN		-1
#define		END_STACK	-1

static long report(fp,paddr)
    FILE	  *fp;
    struct dbAddr *paddr;
{
    struct calcRecord	*pcalc=(struct calcRecord*)(paddr->precord);

    if(recGblReportDbCommon(fp,paddr)) return(-1);
    if(fprintf(fp,"VAL  %-12.4G\n",pcalc->val)) return(-1);
    if(recGblReportLink(fp,"INPA ",&(pcalc->inpa))) return(-1);
    if(recGblReportLink(fp,"INPB ",&(pcalc->inpb))) return(-1);
    if(recGblReportLink(fp,"INPC ",&(pcalc->inpc))) return(-1);
    if(recGblReportLink(fp,"INPD ",&(pcalc->inpd))) return(-1);
    if(recGblReportLink(fp,"INPE ",&(pcalc->inpe))) return(-1);
    if(recGblReportLink(fp,"INPF ",&(pcalc->inpf))) return(-1);
    if(recGblReportLink(fp,"FLNK",&(pcalc->flnk))) return(-1);
    if(fprintf(fp,"A  %-12.4G\n",pcalc->a)) return(-1);
    if(fprintf(fp,"B  %-12.4G\n",pcalc->b)) return(-1);
    if(fprintf(fp,"C  %-12.4G\n",pcalc->c)) return(-1);
    if(fprintf(fp,"D  %-12.4G\n",pcalc->d)) return(-1);
    if(fprintf(fp,"E  %-12.4G\n",pcalc->e)) return(-1);
    if(fprintf(fp,"F  %-12.4G\n",pcalc->f)) return(-1);
    if(fprintf(fp,"PREC %d  EGU %-8s\n",pcalc->prec,pcalc->egu)) return(-1);
    if(fprintf(fp,"HOPR %-12.4G LOPR %-12.4G\n",
	pcalc->hopr,pcalc->lopr)) return(-1);
    if(fprintf(fp,"HIHI %-12.4G HIGH %-12.4G  LOW %-12.4G LOLO %-12.4G\n",
	pcalc->hihi,pcalc->high,pcalc->low,pcalc->lolo)) return(-1);
    if(recGblReportGblChoice(fp,pcalc,"HHSV",pcalc->hhsv)) return(-1);
    if(recGblReportGblChoice(fp,pcalc,"HSV ",pcalc->hsv)) return(-1);
    if(recGblReportGblChoice(fp,pcalc,"LSV ",pcalc->lsv)) return(-1);
    if(recGblReportGblChoice(fp,pcalc,"LLSV",pcalc->llsv)) return(-1);
    if(fprintf(fp,"HYST %-12.4G ADEL %-12.4G MDEL %-12.4G\n",
	pcalc->hyst,pcalc->adel,pcalc->mdel)) return(-1);
    if(fprintf(fp,"ACHN %d\n",pcalc->achn)) return(-1);
    if(fprintf(fp,"LALM %-12.4G ALST %-12.4G MLST %-12.4G\n",
	pcalc->lalm,pcalc->alst,pcalc->mlst)) return(-1);
    if(fprintf(fp,"CALC %s\n",pcalc->calc)) return(-1);
    return(0);
}

static long init_record(pcalc)
    struct calcRecord	*pcalc;
{
    long status;
    short error_number;
    char rpbuf[80];

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

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    *precision = pcalc->prec;
    return(0L);
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
    return(0L);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    pgd->upper_disp_limit = pcalc->hopr;
    pgd->lower_disp_limit = pcalc->lopr;
    pgd->upper_alarm_limit = pcalc->hihi;
    pgd->upper_warning_limit = pcalc->high;
    pgd->lower_warning_limit = pcalc->low;
    pgd->lower_alarm_limit = pcalc->lolo;
    return(0L);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    pcd->upper_ctrl_limit = pcalc->hopr;
    pcd->lower_ctrl_limit = pcalc->lopr;
    return(0L);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct calcRecord	*pcalc=(struct calcRecord *)(paddr->precord);
	struct calcdset	*pdset = (struct calcdset *)(pcalc->dset);
	long		 status;

	pcalc->achn = 0;
	/* read inputs  */
	if (fetch_values(pcalc) < 0){
		if (pcalc->stat != READ_ALARM){
			pcalc->stat = READ_ALARM;
			pcalc->sevr = MAJOR_ALARM;
			pcalc->achn = 1;
			monitor_calc(pcalc);
		}
		pcalc->pact = 0;
		return(0);
	}else{
		if (pcalc->stat == READ_ALARM){
			pcalc->stat = NO_ALARM;
			pcalc->sevr = NO_ALARM;
			pcalc->achn = 1;
		}
	}

	/* perform calculation */
	if (do_calc(pcalc) < 0){
		if (pcalc->stat != CALC_ALARM){
			pcalc->stat = CALC_ALARM;
			pcalc->sevr = MAJOR_ALARM;
			pcalc->achn = 1;
			monitor_calc(pcalc);
		}
		pcalc->pact = 0;
		return;
	}else{
		if (pcalc->stat == CALC_ALARM){
			pcalc->stat = NO_ALARM;
			pcalc->sevr = NO_ALARM;
			pcalc->achn = 1;
		}
	}

	/* check for alarms */
	alarm(pcalc);


	/* check event list */
	if(!pcalc->disa) status = monitor(pcalc);

	/* process the forward scan link record */
	if (pcalc->flnk.type==DB_LINK) dbScanPassive(&pcalc->flnk.value);

	pcalc->pact=FALSE;
	return(status);
}

/*
 *	DO_CALC
 *
 *	execute the calculation
 */
#define	NOT_SET		0
#define	TRUE_COND	1
#define	FALSE_COND	2

static do_calc(pcalc)
struct calcRecord *pcalc;  /* pointer to calculation record  */
{
	char	*post;		/* postfix expression	*/
	double	*pstacktop;	/* stack of values	*/
	double	stack[80];
	double	temp;
	short	temp1;
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
				temp1 = (int) *(pstacktop+1);
				/* is exponent an integer */
				if ((*(pstacktop+1) - (double)temp1) != 0) return (-1);
        			*pstacktop = exp(*(pstacktop+1) * log(-*pstacktop));
				/* is value negative */
				if ((temp1 % 2) > 0) *pstacktop = -*pstacktop;
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
	else
		return(-1);
	return(0);
}

/*
 * FETCH_VALUES
 *
 * fetch the values for the variables in the calculation
 */
static fetch_values(pcalc)
struct calcRecord *pcalc;
{
	short	status;

	/* note - currently not using alarm status */
	status = 0;
	status |= get_calc_inp(&pcalc->inpa,&pcalc->a);
	status |= get_calc_inp(&pcalc->inpb,&pcalc->b);
	status |= get_calc_inp(&pcalc->inpc,&pcalc->c);
	status |= get_calc_inp(&pcalc->inpd,&pcalc->d);
	status |= get_calc_inp(&pcalc->inpe,&pcalc->e);
	status |= get_calc_inp(&pcalc->inpf,&pcalc->f);
	return(status);
}

/*
 *	GET_CALC_INPUT
 *
 *	return an input value
 */
static get_calc_inp(plink,pvalue)
struct link	*plink;	/* structure of the link field  */
float		*pvalue;
{
	float	float_value;

	/* hardware io into a calc not currently supported */
	if (plink->type == VME_IO){
		return(-1);

	/* database link */
	}else if (plink->type == DB_LINK){
		if (dbGetLink(&plink->value.db_link,DBR_FLOAT,&float_value,1) < 0)
			return(-1);
		*pvalue = float_value;

	/* constant */
	}else if (plink->type == CONSTANT){
		;
	/* illegal link type */
	}else{
		return(-1);
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

static long alarm(pcalc)
    struct calcRecord	*pcalc;
{
	float	ftemp;

	/* if in alarm and difference is not > hysterisis don't bother */
	if (pcalc->stat != NO_ALARM){
		ftemp = pcalc->lalm - pcalc->val;
		if(ftemp<0.0) ftemp = -ftemp;
		if (ftemp < pcalc->hyst) return(0);
	}

	/* alarm condition hihi */
	if (pcalc->hhsv != NO_ALARM){
		if (pcalc->val > pcalc->hihi){
			pcalc->lalm = pcalc->val;
			if (pcalc->stat != HIHI_ALARM){
				pcalc->stat = HIHI_ALARM;
				pcalc->sevr = pcalc->hhsv;
				pcalc->achn = 1;
			}
			return(0);
		}
	}

	/* alarm condition lolo */
	if (pcalc->llsv != NO_ALARM){
		if (pcalc->val < pcalc->lolo){
			pcalc->lalm = pcalc->val;
			if (pcalc->stat != LOLO_ALARM){
				pcalc->stat = LOLO_ALARM;
				pcalc->sevr = pcalc->llsv;
				pcalc->achn = 1;
			}
			return(0);
		}
	}

	/* alarm condition high */
	if (pcalc->hsv != NO_ALARM){
		if (pcalc->val > pcalc->high){
			pcalc->lalm = pcalc->val;
			if (pcalc->stat != HIGH_ALARM){
				pcalc->stat = HIGH_ALARM;
				pcalc->sevr =pcalc->hsv;
				pcalc->achn = 1;
			}
			return(0);
		}
	}

	/* alarm condition lolo */
	if (pcalc->lsv != NO_ALARM){
		if (pcalc->val < pcalc->low){
			pcalc->lalm = pcalc->val;
			if (pcalc->stat != LOW_ALARM){
				pcalc->stat = LOW_ALARM;
				pcalc->sevr = pcalc->lsv;
				pcalc->achn = 1;
			}
			return(0);
		}
	}

	/* no alarm */
	if (pcalc->stat != NO_ALARM){
		pcalc->stat = NO_ALARM;
		pcalc->sevr = NO_ALARM;
		pcalc->achn = 1;
	}

	return(0);
}

static long monitor(pcalc)
    struct calcRecord	*pcalc;
{
	unsigned short	monitor_mask;
	float		delta;

	/* anyone wcalcting for an event on this record */
	if (pcalc->mlis.count == 0) return(0L);

	/* Flags which events to fire on the value field */
	monitor_mask = 0;

	/* alarm condition changed this scan */
	if (pcalc->achn){
		/* post events for alarm condition change and value change */
		monitor_mask = DBE_ALARM | DBE_VALUE | DBE_LOG;

		/* post stat and sevr fields */
		db_post_events(pcalc,&pcalc->stat,DBE_VALUE);
		db_post_events(pcalc,&pcalc->sevr,DBE_VALUE);

		/* update last value monitored */
		pcalc->mlst = pcalc->val;

	/* check for value change */
	}else{
		delta = pcalc->mlst - pcalc->val;
		if(delta<0.0) delta = -delta;
		if (delta > pcalc->mdel) {
			/* post events for value change */
			monitor_mask = DBE_VALUE;

			/* update last value monitored */
			pcalc->mlst = pcalc->val;
		}
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
	return(0L);
}

/*
 * POSTFIX.C
 *
 * Subroutines used to convert an infix expression to a postfix expression
 *
 *	Author:	Bob Dalesio
 *	Date:	12-12-86
 *	@(#)postfix.c	1.1	9/21/88
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
 *	Andy Kozubal, AT-8, Mail Stop H820
 *	Los Alamos National Laboratory
 *	Los Alamos, New Mexico 87545
 *	Phone: (505) 667-6508
 *	E-mail: kozubal@k2.lanl.gov
 *
 * Modification Log:
 * -----------------
 * .01	01-11-89	lrd	added right shift and left shift operations
 * .02	01-13-89	lrd	modified to load into IOCs
 * .03	02-01-89	lrd	added trigonometric functions
 * .04	04-05-89	lrd	fixed the order of some operations in the
 *				element table and added a warning label
*/

/*
 * Subroutines
 *
 * find_element		finds a symbolic element in the expression element tbl
 *	args
 *		pbuffer		pointer to the infox expression element
 *		pelement	pointer to the expression element table entry
 *		pno_bytes	pointer to the size of this element
 *	returns
 *		TRUE		element found
 *		FALSE		element not found
 * get_element		finds the next expression element in the infix expr
 *	args
 *		pinfix		pointer into the infix expression
 *		pelement	pointer to the expression element table
 *		pno_bytes	size of the element in the infix expression
 *	returns
 *		FINE		found an expression element
 *		VARIABLE	found a database reference
 *		UNKNOWN_ELEMENT	unknown element found in the infix expression
 * match_element	finds an alpha element in the expression table
 *	args
 *		pbuffer		pointer to an alpha expression element
 *		pelement	pointer to the expression element table
 *	returns
 *		TRUE		found the element in the element table
 *		FLASE		expression element not found
 * postfix		convert an algebraic expression to symbolic postfix
 *	args
 *		pinfix		the algebraic expression
 *		ppostfix	the symbolic postfix expression
 *	returns
 *		0		successful
 *		-1		not successful
 */

/* element types */
#define	OPERAND		0
#define UNARY_OPERATOR	1
#define	BINARY_OPERATOR	2
#define	EXPR_TERM	3
#define	COND		4
#define	CLOSE_PAREN	5
#define	CONDITIONAL	6
#define	ELSE		7
#define	TRASH		8

/* flags end of element table */
#define	END_ELEMENTS	-1

/* parsing return values */
#define	FINE		0
#define	UNKNOWN_ELEMENT	-1
#define	END		-2

/*
 * element table
 *
 * structure of an element
 */
struct	expression_element{
	char	element[10];	/* character representation of an element */
	char	in_stack_pri;	/* priority in translation stack */
	char	in_coming_pri;	/* priority when first checking */
	char	type;	/* element type */
	char	code;			/* postfix representation */
};

/*
 * NOTE: DO NOT CHANGE WITHOUT READING THIS NOTICE !!!!!!!!!!!!!!!!!!!!
 * Because the routine that looks for a match in this table takes the first 
 * match it finds, elements whose designations are contained in other elements
 * MUST come first in this list. (e.g. ABS will match A if A preceeds ABS and
 * then try to find BS therefore ABS must be first in this list
 */
static struct expression_element	elements[] = {
/* element	i_s_p	i_c_p	type_element	internal_rep */
"ABS",		7,	8,	UNARY_OPERATOR,	ABS_VAL,   /* absolute value */
"NOT",		7,	8,	UNARY_OPERATOR,	UNARY_NEG, /* unary negate */
"SQR",		7,	8,	UNARY_OPERATOR,	SQU_RT,    /* square root */
"LOG",		7,	8,	UNARY_OPERATOR,	LOG_10,    /* log 10 */
"LOGE",		7,	8,	UNARY_OPERATOR,	LOG_E,     /* log E */
"ACOS",		7,	8,	UNARY_OPERATOR,	ACOS,      /* arc cosine */
"ASIN",		7,	8,	UNARY_OPERATOR,	ASIN,      /* arc sine */
"ATAN",		7,	8,	UNARY_OPERATOR,	ATAN,      /* arc tangent */
"COS",		7,	8,	UNARY_OPERATOR,	COS,       /* cosine */
"COSH",		7,	8,	UNARY_OPERATOR,	COSH,      /* hyperbolic cosine */
"SIN",		7,	8,	UNARY_OPERATOR,	SIN,       /* sine */
"SINH",		7,	8,	UNARY_OPERATOR,	SINH,      /* hyperbolic sine */
"TAN",		7,	8,	UNARY_OPERATOR,	TAN,       /* tangent */
"TANH",		7,	8,	UNARY_OPERATOR,	TANH,      /* hyperbolic tangent*/
"RNDM",    	0,	0,	OPERAND,	RANDOM,    /* Random Number */
"OR",		1,	1,	BINARY_OPERATOR,BIT_OR,    /* or */
"AND",		2,	2,	BINARY_OPERATOR,BIT_AND,   /* and */
"XOR",		1,	1,	BINARY_OPERATOR,BIT_EXCL_OR, /* exclusive or */
"A",		0,	0,	OPERAND,	FETCH_A,   /* fetch var A */
"B",		0,	0,	OPERAND,	FETCH_B,   /* fetch var B */
"C",		0,	0,	OPERAND,	FETCH_C,   /* fetch var C */
"D",		0,	0,	OPERAND,	FETCH_D,   /* fetch var D */
"E",		0,	0,	OPERAND,	FETCH_E,   /* fetch var E */
"F",		0,	0,	OPERAND,	FETCH_F,   /* fetch var F */
"a",		0,	0,	OPERAND,	FETCH_A,   /* fetch var A */
"b",		0,	0,	OPERAND,	FETCH_B,   /* fetch var B */
"c",		0,	0,	OPERAND,	FETCH_C,   /* fetch var C */
"d",		0,	0,	OPERAND,	FETCH_D,   /* fetch var D */
"e",		0,	0,	OPERAND,	FETCH_E,   /* fetch var E */
"f",		0,	0,	OPERAND,	FETCH_F,   /* fetch var F */
"?",		0,	0,	CONDITIONAL,	COND_ELSE, /* conditional */
":",		0,	0,	ELSE,		COND_ELSE, /* else */
"(",		0,	8,	UNARY_OPERATOR,	PAREN,        /* open paren */
"^",		6,	6,	BINARY_OPERATOR,EXPON,     /* exponentiation */
"**",		6,	6,	BINARY_OPERATOR,EXPON,     /* exponentiation */
"+",		4,	4,	BINARY_OPERATOR,ADD,       /* addition */
"-",		4,	4,	BINARY_OPERATOR,SUB,       /* subtraction */
"*",		5,	5,	BINARY_OPERATOR,MULT,      /* multiplication */
"/",		5,	5,	BINARY_OPERATOR,DIV,       /* division */
"%",		5,	5,	BINARY_OPERATOR,MODULO,    /* modulo */
")",		0,	0,	CLOSE_PAREN,	PAREN,     /* close paren */
"||",		1,	1,	BINARY_OPERATOR,REL_OR,    /* or */
"|",		1,	1,	BINARY_OPERATOR,BIT_OR,    /* or */
"&&",		2,	2,	BINARY_OPERATOR,REL_AND,   /* and */
"&",		2,	2,	BINARY_OPERATOR,BIT_AND,   /* and */
">>",		2,	2,	BINARY_OPERATOR,RIGHT_SHIFT, /* right shift */
">=",		3,	3,	BINARY_OPERATOR,GR_OR_EQ,  /* greater or equal*/
">",		3,	3,	BINARY_OPERATOR,GR_THAN,    /* greater than */
"<<",		2,	2,	BINARY_OPERATOR,LEFT_SHIFT, /* left shift */
"<=",		3,	3,	BINARY_OPERATOR,LESS_OR_EQ,/* less or equal to*/
"<",		3,	3,	BINARY_OPERATOR,LESS_THAN, /* less than */
"#",		3,	3,	BINARY_OPERATOR,NOT_EQ,    /* not equal */
"=",		3,	3,	BINARY_OPERATOR,EQUAL,     /* equal */
""
};

/*
 * FIND_ELEMENT
 *
 * find the pointer to an entry in the element table
 */
 find_element(pbuffer,pelement,pno_bytes)
#define SAME 0
 char	*pbuffer;
 struct expression_element	**pelement;
 short	*pno_bytes;
 {

 	/* compare the string to each element in the element table */
 	*pelement = &elements[0];
 	while ((*pelement)->element[0] != NULL){
 		if (strncmp(pbuffer,(*pelement)->element,
		  strlen((*pelement)->element)) == SAME){
 			*pno_bytes += strlen((*pelement)->element);
 			return(TRUE);
 		}
 		*pelement += 1;
 	}
 	return(FALSE);
 }
 
/*
 * GET_ELEMENT
 *
 * get an expression element
 */
get_element(pinfix,pelement,pno_bytes)
char	*pinfix;
struct expression_element	**pelement;
short		*pno_bytes;
{
	char	buffer[40];
	short	i;

	/* get the next expression element from the infix expression */
	if (*pinfix == NULL) return(END);
	*pno_bytes = 0;
	while (*pinfix == 0x20){
		*pno_bytes += 1;
		pinfix++;
	}
	if (!find_element(pinfix,pelement,pno_bytes))
		return(UNKNOWN_ELEMENT);
	return(FINE);

	
}

/*
 * POSTFIX
 *
 * convert an infix expression to a postfix expression
 */
long postfix(pinfix,ppostfix,perror)
char	*pinfix;
char	*ppostfix;
short		*perror;
{
	short		got_if;
	short		got_else;
	short		no_bytes;
	short	operand_needed;
	short	new_expression;
	short	link_inx;	/* index into variable table	*/
	struct expression_element	stack[80];
	struct expression_element	*pelement;
	struct expression_element	*pstacktop;

	/* place the expression elements into postfix */
	got_if = FALSE;
	got_else = FALSE;
	operand_needed = TRUE;
	new_expression = TRUE;
	pstacktop = &stack[0];
	while (get_element(pinfix,&pelement,&no_bytes) != END){

	    pinfix += no_bytes;
	    switch (pelement->type){

	    case OPERAND:
		if (!operand_needed){
		    *perror = 5;
		    return(-1);
		}

		/* add operand to the expression */
		*ppostfix++ = pelement->code;

		operand_needed = FALSE;
		new_expression = FALSE;
		break;

	    case BINARY_OPERATOR:
		if (operand_needed){
		    *perror = 4;
		    return(-1);
		}

		/* add operators of higher or equal priority to	*/
		/* postfix notation				*/
		while ((pstacktop->in_stack_pri >= pelement->in_coming_pri)
		  && (pstacktop >= &stack[1])){
		    *ppostfix++ = pstacktop->code;
		    pstacktop--;
		}

		/* add new operator to stack */
		pstacktop++;
		*pstacktop = *pelement;

		operand_needed = TRUE;
		break;

	    case UNARY_OPERATOR:
		if (!operand_needed){
		    *perror = 5;
		    return(-1);
		}

		/* add operators of higher or equal priority to	*/
		/* postfix notation 				*/
		while ((pstacktop->in_stack_pri >= pelement->in_coming_pri)
		  && (pstacktop >= &stack[1])){
		      *ppostfix++ = pstacktop->code;
		      pstacktop--;
		 }

		/* add new operator to stack */
		pstacktop++;
		*pstacktop = *pelement;

		new_expression = FALSE;
		break;

	    case CLOSE_PAREN:
		if (operand_needed){
		    *perror = 4;
		    return(-1);
		}

		/* add operators to postfix until matching paren */
		while (pstacktop->element[0] != '('){
		    if (pstacktop == &stack[1]){
			*perror = 6;
			return(-1);
		    }
		    *ppostfix++ = pstacktop->code;
		    pstacktop--;
		}
		pstacktop--;	/* remove ( from stack */
		break;

	    /* conditional includes expression termination */
	    case CONDITIONAL:
		if (got_if){
		    *perror = 9;
		    return(-1);
		}
		got_if = TRUE;

	    /* else includes expression termination */
	    case ELSE:
		if (pelement->type == ELSE){
		    if (!got_if){
			*perror = 11;
			return(-1);
		    }
		    if (got_else){
			*perror = 10;
			return(-1);
		    }
		    got_else = TRUE;
		}

	    case EXPR_TERM:
		if (operand_needed && !new_expression){
		    *perror = 4;
		    return(-1);
		}

		/* add all operators on stack to postfix */
		while (pstacktop >= &stack[1]){
		    if (pstacktop->element[0] == '('){
			*perror = 6;
			return(-1);
		    }
		    *ppostfix++ = pstacktop->code;
		    pstacktop--;
		}

		/* add new element to the postfix expression */
		*ppostfix++ = pelement->code;

		operand_needed = TRUE;
		new_expression = TRUE;
		break;


	    default:
		*perror = 8;
		return(-1);
	    }
	}
	if (operand_needed){
		*perror = 4;
		return(-1);
	}

	/* add all operators on stack to postfix */
	while (pstacktop >= &stack[1]){
	    if (pstacktop->element[0] == '('){
		*perror = 6;
		return(-1);
	    }
	    *ppostfix++ = pstacktop->code;
	    pstacktop--;
	}
	*ppostfix = END_STACK;

	return(0);
}
