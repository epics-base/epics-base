/*
 *	C O N V E R T . C
 *
 *	Author: D. Kersteins
 *
 *
 * 	NOTES: 
 *
 *	1) All routines in this file have an encode argument which
 *	determines if we are converting from the standard format to
 *	the local format or vise versa. To date only float and double data 
 *	types must be converted differently depending on the encode
 *	argument - joh
 *
 *	History
 *	joh	10-21-90	Moved atomic types here to avoid
 *				maintenance problems in service.c
 *				This also provides for a cleaner seperation
 *				of function.
 *	joh	10-21-90	Added new DBR_XXXX types for release two
 *	joh	03-15-94	Added many missing types, named ca types,
 *				and ANSI C changes.
 *	joh	07-05-94	Fixed double invocation of ++ operator
 *				by byte swap macro bug in cvrt_short(),
 *				cvrt_long(), cvrt_enum().
 *	joh	11-02-94	moved float convert to this source 
 *
 *
 */

static char	*sccsId = "@(#) $Id$";

#include	<string.h>

#include	"iocinf.h"
#include	"net_convert.h"

/*
 * NOOP if this isnt required
 */
#ifdef CONVERSION_REQUIRED

/*
 * if hton is true then it is a host to network conversion
 * otherwise vise-versa
 *
 * net format: big endian and IEEE float
 * 
 * typedef void CACVRTFUNC(
 *		void *pSource, 
 *		void *pDestination, 
 *		int hton, 
 *		unsigned long count);
 */

LOCAL CACVRTFUNC	cvrt_string;
LOCAL CACVRTFUNC	cvrt_short;
LOCAL CACVRTFUNC	cvrt_float;
LOCAL CACVRTFUNC	cvrt_enum;
LOCAL CACVRTFUNC	cvrt_char;
LOCAL CACVRTFUNC	cvrt_long;
LOCAL CACVRTFUNC	cvrt_double;

LOCAL CACVRTFUNC	cvrt_sts_string;
LOCAL CACVRTFUNC	cvrt_sts_short;
LOCAL CACVRTFUNC	cvrt_sts_float;
LOCAL CACVRTFUNC	cvrt_sts_enum;
LOCAL CACVRTFUNC	cvrt_sts_char;	
LOCAL CACVRTFUNC	cvrt_sts_long;
LOCAL CACVRTFUNC	cvrt_sts_double;

LOCAL CACVRTFUNC	cvrt_time_string;
LOCAL CACVRTFUNC	cvrt_time_short;
LOCAL CACVRTFUNC	cvrt_time_float;
LOCAL CACVRTFUNC	cvrt_time_enum;
LOCAL CACVRTFUNC	cvrt_time_char;
LOCAL CACVRTFUNC	cvrt_time_long;
LOCAL CACVRTFUNC	cvrt_time_double;

LOCAL CACVRTFUNC	cvrt_gr_short;
LOCAL CACVRTFUNC	cvrt_gr_float;
LOCAL CACVRTFUNC	cvrt_gr_enum;
LOCAL CACVRTFUNC	cvrt_gr_char;
LOCAL CACVRTFUNC	cvrt_gr_long;
LOCAL CACVRTFUNC	cvrt_gr_double;

LOCAL CACVRTFUNC	cvrt_ctrl_short;
LOCAL CACVRTFUNC	cvrt_ctrl_float;
LOCAL CACVRTFUNC	cvrt_ctrl_enum;
LOCAL CACVRTFUNC	cvrt_ctrl_char;
LOCAL CACVRTFUNC	cvrt_ctrl_long;
LOCAL CACVRTFUNC	cvrt_ctrl_double;

LOCAL CACVRTFUNC	cvrt_put_ackt;
LOCAL CACVRTFUNC	cvrt_stsack_string;

/*  cvrt is (array of) (pointer to) (function returning) int */
epicsShareDef CACVRTFUNC *cac_dbr_cvrt[]
	=
	{
	cvrt_string,
	cvrt_short,
	cvrt_float,
	cvrt_enum,
	cvrt_char,
	cvrt_long,
	cvrt_double,

	cvrt_sts_string,
	cvrt_sts_short,
	cvrt_sts_float,
	cvrt_sts_enum,
	cvrt_sts_char,
	cvrt_sts_long,
	cvrt_sts_double,

	cvrt_time_string,
	cvrt_time_short,
	cvrt_time_float,
	cvrt_time_enum,
	cvrt_time_char,
	cvrt_time_long,
	cvrt_time_double,

	cvrt_sts_string, /* DBR_GR_STRING identical to dbr_sts_string */
	cvrt_gr_short,
	cvrt_gr_float,
	cvrt_gr_enum,
	cvrt_gr_char,
	cvrt_gr_long,
	cvrt_gr_double,

	cvrt_sts_string, /* DBR_CTRL_STRING identical to dbr_sts_string */
	cvrt_ctrl_short,
	cvrt_ctrl_float,
	cvrt_ctrl_enum,
	cvrt_ctrl_char,
	cvrt_ctrl_long,
	cvrt_ctrl_double,

	cvrt_put_ackt,  
	cvrt_put_ackt, /* DBR_PUT_ACKS identical to DBR_PUT_ACKT */
	cvrt_stsack_string
	};


#define dbr_ntohs(A)	(ntohs(A))
#define dbr_ntohl(A)	(ntohl(A))
#define dbr_htons(A)	(htons(A))
#define dbr_htonl(A)	(htonl(A))



/*
 *	CVRT_STRING()
 *
 *
 *
 *
 */
LOCAL void cvrt_string(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	char		*pSrc = s;
	char		*pDest = d;

	/* convert "in place" -> nothing to do */
	if (s == d)
		return;
	if(num == 1){
		strcpy(pDest, pSrc);
	}
	else{
		memcpy(pDest, pSrc, num*MAX_STRING_SIZE);
	}
}


/*
 *	CVRT_SHORT()
 *
 *
 *
 *
 */
LOCAL void cvrt_short(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	dbr_short_t	*pSrc = s;
	dbr_short_t	*pDest = d;
      	unsigned long	i;

      	for(i=0; i<num; i++){
      	  	*pDest = dbr_ntohs( *pSrc );
		/*
		 * dont increment these inside the MACRO 
		 */
		pDest++;
		pSrc++;
	}
}


/*
 *	CVRT_CHAR()
 *
 *
 *
 *
 */
LOCAL void cvrt_char(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
      	unsigned long	i;
	dbr_char_t	*pSrc = s;
	dbr_char_t	*pDest = d;

	/* convert "in place" -> nothing to do */
	if (s == d)
		return;
      	for(i=0; i<num; i++){
      	  	*pDest++ = *pSrc++;
	}
}


/*
 *	CVRT_LONG()
 *
 *
 *
 *
 */
LOCAL void cvrt_long(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
      	unsigned long	i;
	dbr_long_t	*pSrc = s;
	dbr_long_t	*pDest = d;

      	for(i=0; i<num; i++){
      	  	*pDest = dbr_ntohl( *pSrc );
		/*
		 * dont increment these inside the MACRO 
		 */
		pDest++;
		pSrc++;
	}
}


/*
 *	CVRT_ENUM()
 *
 *
 *
 *
 */
LOCAL void cvrt_enum(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
      	unsigned long 	i;
	dbr_enum_t	*pSrc;
	dbr_enum_t	*pDest;

	pSrc = s;
	pDest = d;
      	for(i=0; i<num; i++){
      	  	*pDest = dbr_ntohs(*pSrc);
		/*
		 * dont increment these inside the MACRO 
		 */
		pDest++;
		pSrc++;
	}
}


/*
 *	CVRT_FLOAT()
 *
 *
 *	NOTES:
 *	placing encode outside the loop results in more 
 * 	code but better performance.
 *
 */
LOCAL void cvrt_float(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
      	unsigned long	i;
	dbr_float_t	*pSrc = s;
	dbr_float_t	*pDest = d;

      	for(i=0; i<num; i++){
		if(encode){
	  		dbr_htonf(pSrc, pDest);
		}
		else{
	  		dbr_ntohf(pSrc, pDest);
		}
		/*
		 * dont increment these inside the MACRO 
		 */
		pSrc++;
		pDest++;
	}
}


/*
 *	CVRT_DOUBLE()
 *
 *
 *
 *
 */
LOCAL void cvrt_double(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
      	unsigned long	i;
	dbr_double_t	*pSrc = s;
	dbr_double_t	*pDest = d;

      	for(i=0; i<num; i++){
		if(encode){
			dbr_htond(pSrc,pDest);
		}
		else{
			dbr_ntohd(pSrc,pDest);
		}
		/*
		 * dont increment these inside the MACRO 
		 */
		pSrc++;
		pDest++;
	}
}





/****************************************************************************
**	cvrt_sts_string(s,d)
**		struct dbr_sts_string *s  	pointer to source struct
**		struct dbr_sts_string *d	pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**	      
**	converts fields of struct in HOST format to NET format
**	   or 
**	converts fields of struct in NET format to fields with HOST 
**		format;
****************************************************************************/

LOCAL void cvrt_sts_string(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_sts_string	*pSrc = s;
	struct dbr_sts_string 	*pDest = d;
			
    	/* convert ieee to vax format or vax to ieee */
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);

	/* convert "in place" -> nothing else to do */
	if (s == d)
		return;

    	if (num == 1)	/* if single value */
		strcpy(pDest->value, pSrc->value);
    	else
		memcpy(pDest->value, pSrc->value, (MAX_STRING_SIZE * num));

}


/****************************************************************************
**	cvrt_sts_short(s,d)
**		struct dbr_sts_int *s  	pointer to source struct
**		struct dbr_sts_int *d	pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	converts fields ofstruct in HOST format to ieee format
**	   or 
**	converts fields of struct in NET format to fields with HOST 
**		format
****************************************************************************/

LOCAL void cvrt_sts_short(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_sts_int	*pSrc = s;
	struct dbr_sts_int	*pDest = d;

    	/* convert vax to ieee or ieee to vax format -- same code*/
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);

    	if (num == 1)	/* single value */
		pDest->value = dbr_ntohs(pSrc->value);
    	else		/* array chan-- multiple pts */
    	{
		cvrt_short(&pSrc->value, &pDest->value, encode, num);
    	}
}

/****************************************************************************
**	cvrt_sts_float(s,d)
**		struct dbr_sts_float *s  	pointer to source struct
**		struct dbr_sts_float *d		pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	   if encode 
**		converts struct in HOST format to ieee format
**	   else 
**		converts fields of struct in NET format to fields with HOST 
**		format;
****************************************************************************/

LOCAL void cvrt_sts_float(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_sts_float	*pSrc = s;
	struct dbr_sts_float	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);

	cvrt_float(&pSrc->value, &pDest->value, encode, num);
}


/****************************************************************************
**	cvrt_sts_double(s,d)
**
**	   if encode 
**		converts struct in HOST format to ieee format
**	   else 
**		converts fields of struct in NET format to fields with HOST 
**		format;
****************************************************************************/

LOCAL void cvrt_sts_double(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_sts_double	*pSrc = s;
	struct dbr_sts_double	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);

	cvrt_double(&pSrc->value, &pDest->value, encode, num);
}


/****************************************************************************
**	cvrt_sts_enum(s,d)
**		struct dbr_sts_enum *s  	pointer to source struct
**		struct dbr_sts_enum *d		pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	converts fields of struct in NET format to fields with HOST format
**		 or  
**	converts fields of struct in HOST format to fields with NET format
**	 
****************************************************************************/

LOCAL void cvrt_sts_enum(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_sts_enum	*pSrc = s;
	struct dbr_sts_enum	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
    	if (num == 1)
        	pDest->value	= dbr_ntohs(pSrc->value);
    	else {
		cvrt_enum(&pSrc->value,&pDest->value,encode,num);
    	}
}


/****************************************************************************
**	cvrt_gr_short()
**
**	converts fields of struct in NET format to fields with HOST format
**		 or  
**	converts fields of struct in HOST format to fields with NET format
**	 
****************************************************************************/

LOCAL void cvrt_gr_short(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_gr_int	*pSrc = s;
	struct dbr_gr_int	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));

    	pDest->upper_disp_limit 	= dbr_ntohs(pSrc->upper_disp_limit);
	pDest->lower_disp_limit 	= dbr_ntohs(pSrc->lower_disp_limit);
    	pDest->upper_alarm_limit 	= dbr_ntohs(pSrc->upper_alarm_limit);
    	pDest->upper_warning_limit 	= dbr_ntohs(pSrc->upper_warning_limit);
    	pDest->lower_alarm_limit 	= dbr_ntohs(pSrc->lower_alarm_limit);
    	pDest->lower_warning_limit 	= dbr_ntohs(pSrc->lower_warning_limit);

    	if (num == 1)
    		pDest->value = dbr_ntohs(pSrc->value);
    	else {
		cvrt_short(&pSrc->value, &pDest->value, encode,num);
	}
}


/****************************************************************************
**	cvrt_gr_char()
**
**	converts fields of struct in NET format to fields with HOST format
**		 or  
**	converts fields of struct in HOST format to fields with NET format
**	 
****************************************************************************/

LOCAL void cvrt_gr_char(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_gr_char	*pSrc = s;
	struct dbr_gr_char	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);

	if (s == d)	/* source == dest -> no more conversions */
		return;

	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));

    	pDest->upper_disp_limit 	= pSrc->upper_disp_limit;
	pDest->lower_disp_limit 	= pSrc->lower_disp_limit;
    	pDest->upper_alarm_limit 	= pSrc->upper_alarm_limit;
    	pDest->upper_warning_limit 	= pSrc->upper_warning_limit;
    	pDest->lower_alarm_limit 	= pSrc->lower_alarm_limit;
    	pDest->lower_warning_limit 	= pSrc->lower_warning_limit;

    	if (num == 1)
    		pDest->value = pSrc->value;
    	else {
		memcpy((char *)&pDest->value, (char *)&pSrc->value, num);
	}
}


/****************************************************************************
**	cvrt_gr_long()
**
**	converts fields of struct in NET format to fields with HOST format
**		 or  
**	converts fields of struct in HOST format to fields with NET format
**	 
****************************************************************************/

LOCAL void cvrt_gr_long(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_gr_long	*pSrc = s;
	struct dbr_gr_long	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));

    	pDest->upper_disp_limit 	= dbr_ntohl(pSrc->upper_disp_limit);
	pDest->lower_disp_limit 	= dbr_ntohl(pSrc->lower_disp_limit);
    	pDest->upper_alarm_limit 	= dbr_ntohl(pSrc->upper_alarm_limit);
    	pDest->upper_warning_limit 	= dbr_ntohl(pSrc->upper_warning_limit);
    	pDest->lower_alarm_limit 	= dbr_ntohl(pSrc->lower_alarm_limit);
    	pDest->lower_warning_limit 	= dbr_ntohl(pSrc->lower_warning_limit);

    	if (num == 1)
    		pDest->value = dbr_ntohl(pSrc->value);
    	else {
		cvrt_long(&pSrc->value, &pDest->value, encode, num);
	}
}


/****************************************************************************
**	cvrt_gr_enum(s,d)
**
**	   if encode 
**		converts struct in HOST format to ieee format
**	   else 
**		converts fields of struct in NET format to fields with HOST 
**		format;
****************************************************************************/

LOCAL void cvrt_gr_enum(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_gr_enum	*pSrc = s;
	struct dbr_gr_enum	*pDest = d;

    	pDest->status 			= dbr_ntohs(pSrc->status);
    	pDest->severity			= dbr_ntohs(pSrc->severity);
    	pDest->no_str			= dbr_ntohs(pSrc->no_str);
    	memcpy((void *)pDest->strs,(void *)pSrc->strs,sizeof(pSrc->strs));

    	if (num == 1)	/* single value */
		pDest->value = dbr_ntohs(pSrc->value);
    	else		/* array chan-- multiple pts */
    	{
		cvrt_enum(&(pSrc->value), &(pDest->value), encode, num);
	}
}


/****************************************************************************
**	cvrt_gr_double(s,d)
**
**	   if encode 
**		converts struct in HOST format to ieee format
**	   else 
**		converts fields of struct in NET format to fields with HOST 
**		format;
****************************************************************************/

LOCAL void cvrt_gr_double(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_gr_double	*pSrc = s;
	struct dbr_gr_double	*pDest = d;

    	/* these are same for vax to ieee or ieee to vax */
    	pDest->status 			= dbr_ntohs(pSrc->status);
    	pDest->severity			= dbr_ntohs(pSrc->severity);
    	pDest->precision		= dbr_ntohs(pSrc->precision);
    	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));

    	if (encode)			/* vax to ieee convert		*/
    	{
		if (num == 1){
		    dbr_htond(&pSrc->value, &pDest->value);
		}
		else {
		    cvrt_double(&pSrc->value, &pDest->value, encode,num);
		}
    		dbr_htond(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
    		dbr_htond(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
    		dbr_htond(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
    		dbr_htond(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
    		dbr_htond(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
    		dbr_htond(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    	}
    	else			/* ieee to vax convert 	 	*/
    	{
		if (num == 1){
		    dbr_ntohd(&pSrc->value, &pDest->value);
		}
		else {
		    cvrt_double(&pSrc->value, &pDest->value, encode,num);
		}
    		dbr_ntohd(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
    		dbr_ntohd(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
    		dbr_ntohd(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
    		dbr_ntohd(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
   	 	dbr_ntohd(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
    		dbr_ntohd(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    	}
}



/****************************************************************************
**	cvrt_gr_float(s,d)
**		struct dbr_gr_float *d		pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	   if encode 
**		converts struct in HOST format to ieee format
**	   else 
**		converts fields of struct in NET format to fields with HOST 
**		format;
****************************************************************************/

LOCAL void cvrt_gr_float(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_gr_float     *pSrc = s;
	struct dbr_gr_float     *pDest = d;

    	/* these are same for vax to ieee or ieee to vax */
    	pDest->status 			= dbr_ntohs(pSrc->status);
    	pDest->severity			= dbr_ntohs(pSrc->severity);
    	pDest->precision		= dbr_ntohs(pSrc->precision);
    	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));

    	if (encode)			/* vax to ieee convert		*/
    	{
		if (num == 1){
		    dbr_htonf(&pSrc->value, &pDest->value);
		}
		else {
		    cvrt_float(&pSrc->value, &pDest->value, encode,num);
		}
    		dbr_htonf(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
    		dbr_htonf(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
    		dbr_htonf(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
    		dbr_htonf(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
    		dbr_htonf(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
    		dbr_htonf(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    	}
    	else			/* ieee to vax convert 	 	*/
    	{
		if (num == 1){
		    dbr_ntohf(&pSrc->value, &pDest->value);
		}
		else {
		    cvrt_float(&pSrc->value, &pDest->value, encode,num);
		}
    		dbr_ntohf(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
    		dbr_ntohf(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
    		dbr_ntohf(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
    		dbr_ntohf(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
   	 	dbr_ntohf(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
    		dbr_ntohf(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    	}
}




/****************************************************************************
**	cvrt_ctrl_short(s,d)
**		struct dbr_gr_int *s  	pointer to source struct
**		struct dbr_gr_int *d	pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	converts fields of struct in NET format to fields with HOST format
**		 or  
**	converts fields of struct in HOST format to fields with NET format
**	 
****************************************************************************/

LOCAL void cvrt_ctrl_short(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_ctrl_int	*pSrc = s;
	struct dbr_ctrl_int	*pDest = d;

    	/* vax to ieee or ieee to vax -- same code */
    	pDest->status 			= dbr_ntohs(pSrc->status);
    	pDest->severity			= dbr_ntohs(pSrc->severity);
    	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));

    	pDest->upper_disp_limit 	= dbr_ntohs(pSrc->upper_disp_limit);
    	pDest->lower_disp_limit 	= dbr_ntohs(pSrc->lower_disp_limit);
    	pDest->upper_alarm_limit 	= dbr_ntohs(pSrc->upper_alarm_limit);
    	pDest->upper_warning_limit 	= dbr_ntohs(pSrc->upper_warning_limit);
    	pDest->lower_alarm_limit 	= dbr_ntohs(pSrc->lower_alarm_limit);
    	pDest->lower_warning_limit 	= dbr_ntohs(pSrc->lower_warning_limit);
    	pDest->lower_ctrl_limit 	= dbr_ntohs(pSrc->lower_ctrl_limit);
    	pDest->upper_ctrl_limit 	= dbr_ntohs(pSrc->upper_ctrl_limit);

    	if (num == 1)
    		pDest->value = dbr_ntohs(pSrc->value);
    	else {
		cvrt_short(&pSrc->value, &pDest->value, encode, num);
    	}
}

/****************************************************************************
**	cvrt_ctrl_long(s,d)
**
**	converts fields of struct in NET format to fields with HOST format
**		 or  
**	converts fields of struct in HOST format to fields with NET format
**	 
****************************************************************************/

LOCAL void cvrt_ctrl_long(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_ctrl_long	*pSrc = s;
	struct dbr_ctrl_long	*pDest = d;

    	/* vax to ieee or ieee to vax -- same code */
    	pDest->status 			= dbr_ntohs(pSrc->status);
    	pDest->severity			= dbr_ntohs(pSrc->severity);
    	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));

    	pDest->upper_disp_limit 	= dbr_ntohl(pSrc->upper_disp_limit);
    	pDest->lower_disp_limit 	= dbr_ntohl(pSrc->lower_disp_limit);
    	pDest->upper_alarm_limit 	= dbr_ntohl(pSrc->upper_alarm_limit);
    	pDest->upper_warning_limit 	= dbr_ntohl(pSrc->upper_warning_limit);
    	pDest->lower_alarm_limit 	= dbr_ntohl(pSrc->lower_alarm_limit);
    	pDest->lower_warning_limit 	= dbr_ntohl(pSrc->lower_warning_limit);
    	pDest->lower_ctrl_limit 	= dbr_ntohl(pSrc->lower_ctrl_limit);
    	pDest->upper_ctrl_limit 	= dbr_ntohl(pSrc->upper_ctrl_limit);

    	if (num == 1)
    		pDest->value = dbr_ntohl(pSrc->value);
    	else {
		cvrt_long(&pSrc->value, &pDest->value, encode, num);
    	}
}

/****************************************************************************
**	cvrt_ctrl_short(s,d)
**
**	converts fields of struct in NET format to fields with HOST format
**		 or  
**	converts fields of struct in HOST format to fields with NET format
**	 
****************************************************************************/

LOCAL void cvrt_ctrl_char(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_ctrl_char	*pSrc = s;
	struct dbr_ctrl_char	*pDest = d;

    	/* vax to ieee or ieee to vax -- same code */
    	pDest->status 			= dbr_ntohs(pSrc->status);
    	pDest->severity			= dbr_ntohs(pSrc->severity);

    	pDest->upper_disp_limit 	= pSrc->upper_disp_limit;
    	pDest->lower_disp_limit 	= pSrc->lower_disp_limit;
    	pDest->upper_alarm_limit 	= pSrc->upper_alarm_limit;
    	pDest->upper_warning_limit 	= pSrc->upper_warning_limit;
    	pDest->lower_ctrl_limit 	= pSrc->lower_ctrl_limit;
    	pDest->upper_ctrl_limit 	= pSrc->upper_ctrl_limit;

    	if (num == 1)
    		pDest->value = pSrc->value;
    	else {
		memcpy((void *)&pDest->value, (void *)&pSrc->value, num);
    	}
}


/****************************************************************************
**	cvrt_ctrl_double(s,d)
**
**	   if encode 
**		converts struct in HOST format to ieee format
**	   else 
**		converts fields of struct in NET format to fields with HOST 
**		format;
****************************************************************************/

LOCAL void cvrt_ctrl_double(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_ctrl_double	*pSrc = s;
	struct dbr_ctrl_double	*pDest = d;

	    /* these are the same for ieee to vax or vax to ieee */
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
    	pDest->precision	= dbr_ntohs(pSrc->precision);
    	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));
    	if (encode)			/* vax to ieee convert		*/
    	{
		if (num == 1){
	    		dbr_htond(&pSrc->value, &pDest->value);
		}
		else {
			cvrt_double(&pSrc->value, &pDest->value, encode, num);
		}
    		dbr_htond(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
    		dbr_htond(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
    		dbr_htond(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
    		dbr_htond(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
    		dbr_htond(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
    		dbr_htond(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    		dbr_htond(&pSrc->lower_ctrl_limit, &pDest->lower_ctrl_limit);
    		dbr_htond(&pSrc->upper_ctrl_limit, &pDest->upper_ctrl_limit);
    	}
    	else			/* ieee to vax convert 	 	*/
    	{
		if (num == 1){
	    		dbr_ntohd(&pSrc->value, &pDest->value);
		}
		else {
			cvrt_double(&pSrc->value, &pDest->value, encode, num);
		}
    		dbr_ntohd(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
    		dbr_ntohd(&pSrc->upper_disp_limit, &pDest->upper_disp_limit);
    		dbr_ntohd(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
    		dbr_ntohd(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
    		dbr_ntohd(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
    		dbr_ntohd(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    		dbr_ntohd(&pSrc->lower_ctrl_limit, &pDest->lower_ctrl_limit);
    		dbr_ntohd(&pSrc->upper_ctrl_limit, &pDest->upper_ctrl_limit);
    	}

}




/****************************************************************************
**	cvrt_ctrl_float(s,d)
**
**	   if encode 
**		converts struct in HOST format to ieee format
**	   else 
**		converts fields of struct in NET format to fields with HOST 
**		format;
****************************************************************************/

LOCAL void cvrt_ctrl_float(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_ctrl_float	*pSrc = s;
	struct dbr_ctrl_float	*pDest = d;

	    /* these are the same for ieee to vaax or vax to ieee */
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
    	pDest->precision		= dbr_ntohs(pSrc->precision);
    	memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));
    	if (encode)			/* vax to ieee convert		*/
    	{
		if (num == 1){
	    		dbr_htonf(&pSrc->value, &pDest->value);
		}
		else {
			cvrt_float(&pSrc->value, &pDest->value, encode, num);
		}
    		dbr_htonf(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
    		dbr_htonf(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
    		dbr_htonf(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
    		dbr_htonf(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
    		dbr_htonf(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
    		dbr_htonf(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    		dbr_htonf(&pSrc->lower_ctrl_limit, &pDest->lower_ctrl_limit);
    		dbr_htonf(&pSrc->upper_ctrl_limit, &pDest->upper_ctrl_limit);
    	}
    	else			/* ieee to vax convert 	 	*/
    	{
		if (num == 1){
	    		dbr_ntohf(&pSrc->value, &pDest->value);
		}
		else {
			cvrt_float(&pSrc->value, &pDest->value, encode, num);
		}
    		dbr_ntohf(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
    		dbr_ntohf(&pSrc->upper_disp_limit, &pDest->upper_disp_limit);
    		dbr_ntohf(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
    		dbr_ntohf(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
    		dbr_ntohf(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
    		dbr_ntohf(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    		dbr_ntohf(&pSrc->lower_ctrl_limit, &pDest->lower_ctrl_limit);
    		dbr_ntohf(&pSrc->upper_ctrl_limit, &pDest->upper_ctrl_limit);
    	}

}



/****************************************************************************
**	cvrt_ctrl_enum(s,d)
**
**	   if encode 
**		converts struct in HOST format to ieee format
**	   else 
**		converts fields of struct in NET format to fields with HOST 
**		format;
****************************************************************************/

LOCAL void cvrt_ctrl_enum(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_ctrl_enum	*pSrc = s;
	struct dbr_ctrl_enum	*pDest = d;

    	pDest->status 			= dbr_ntohs(pSrc->status);
    	pDest->severity			= dbr_ntohs(pSrc->severity);
    	pDest->no_str			= dbr_ntohs(pSrc->no_str);
    	memcpy((void *)pDest->strs,(void *)pSrc->strs,sizeof(pSrc->strs));

    	if (num == 1)	/* single value */
		pDest->value = dbr_ntohs(pSrc->value);
    	else		/* array chan-- multiple pts */
    	{
		cvrt_enum(&(pSrc->value), &(pDest->value), encode, num);
	}
}


/****************************************************************************
**	cvrt_sts_char(s,d)
**		struct dbr_sts_int *s  	pointer to source struct
**		struct dbr_sts_int *d	pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	converts fields ofstruct in HOST format to ieee format
**	   or 
**	converts fields of struct in NET format to fields with HOST 
**		format
****************************************************************************/

LOCAL void cvrt_sts_char(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_sts_char	*pSrc = s;
	struct dbr_sts_char	*pDest = d;

    	/* convert vax to ieee or ieee to vax format -- same code*/
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);

    	if (num == 1)	/* single value */
		pDest->value = pSrc->value;
    	else		/* array chan-- multiple pts */
    	{
		memcpy((void *)&pDest->value, (void *)&pSrc->value, num);
    	}
}


/****************************************************************************
**	cvrt_sts_long(s,d)
**
**	converts fields ofstruct in HOST format to ieee format
**	   or 
**	converts fields of struct in NET format to fields with HOST 
**		format
****************************************************************************/

LOCAL void cvrt_sts_long(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_sts_long	*pSrc = s;
	struct dbr_sts_long	*pDest = d;

    	/* convert vax to ieee or ieee to vax format -- same code*/
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);

    	if (num == 1)	/* single value */
		pDest->value = dbr_ntohl(pSrc->value);
    	else		/* array chan-- multiple pts */
    	{
		cvrt_long(&pDest->value, &pSrc->value, encode, num);
    	}
}



/****************************************************************************
**	cvrt_time_string(s,d)
**	      
**	converts fields of struct in HOST format to NET format
**	   or 
**	converts fields of struct in NET format to fields with HOST 
**		format;
****************************************************************************/

LOCAL void cvrt_time_string(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_time_string	*pSrc = s;
	struct dbr_time_string 	*pDest = d;
			
    	/* convert ieee to vax format or vax to ieee */
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
	pDest->stamp.nsec 	= dbr_ntohl(pSrc->stamp.nsec);

    	if (num == 1)	/* if single value */
		strcpy(pDest->value, pSrc->value);
    	else
		memcpy(pDest->value, pSrc->value, (MAX_STRING_SIZE * num));

}


/****************************************************************************
**	cvrt_time_short(s,d)
**
**	converts fields ofstruct in HOST format to ieee format
**	   or 
**	converts fields of struct in NET format to fields with HOST 
**		format
****************************************************************************/

LOCAL void cvrt_time_short(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_time_short	*pSrc = s;
	struct dbr_time_short	*pDest = d;

    	/* convert vax to ieee or ieee to vax format -- same code*/
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
	pDest->stamp.nsec 	= dbr_ntohl(pSrc->stamp.nsec);

    	if (num == 1)	/* single value */
		pDest->value = dbr_ntohs(pSrc->value);
    	else		/* array chan-- multiple pts */
    	{
		cvrt_short(&pSrc->value, &pDest->value, encode, num);
    	}
}


/****************************************************************************
**	cvrt_time_float(s,d)
**
**	   if encode 
**		converts struct in HOST format to ieee format
**	   else 
**		converts fields of struct in NET format to fields with HOST 
**		format;
****************************************************************************/

LOCAL void cvrt_time_float(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_time_float	*pSrc = s;
	struct dbr_time_float	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
	pDest->stamp.nsec 	= dbr_ntohl(pSrc->stamp.nsec);

	cvrt_float(&pSrc->value, &pDest->value, encode, num);
}


/****************************************************************************
**	cvrt_time_double(s,d)
**
**	   if encode 
**		converts struct in HOST format to ieee format
**	   else 
**		converts fields of struct in NET format to fields with HOST 
**		format;
****************************************************************************/

LOCAL void cvrt_time_double(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_time_double	*pSrc = s;
	struct dbr_time_double	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
	pDest->stamp.nsec 	= dbr_ntohl(pSrc->stamp.nsec);

	cvrt_double(&pSrc->value, &pDest->value, encode, num);
}




/****************************************************************************
**	cvrt_time_enum(s,d)
**
**	converts fields of struct in NET format to fields with HOST format
**		 or  
**	converts fields of struct in HOST format to fields with NET format
**	 
****************************************************************************/

LOCAL void cvrt_time_enum(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_time_enum	*pSrc = s;
	struct dbr_time_enum	*pDest = d;

    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
	pDest->stamp.nsec 	= dbr_ntohl(pSrc->stamp.nsec);
    	if (num == 1)
        	pDest->value	= dbr_ntohs(pSrc->value);
    	else {
		cvrt_enum(&pSrc->value,&pDest->value,encode,num);
    	}
}


/****************************************************************************
**	cvrt_sts_char(s,d)
**
**	converts fields ofstruct in HOST format to ieee format
**	   or 
**	converts fields of struct in NET format to fields with HOST 
**		format
****************************************************************************/

LOCAL void cvrt_time_char(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_time_char	*pSrc = s;
	struct dbr_time_char	*pDest = d;

    	/* convert vax to ieee or ieee to vax format -- same code*/
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
	pDest->stamp.nsec 	= dbr_ntohl(pSrc->stamp.nsec);

    	if (num == 1)	/* single value */
		pDest->value = pSrc->value;
    	else		/* array chan-- multiple pts */
    	{
		memcpy((void *)&pDest->value, (void *)&pSrc->value, num);
    	}
}

/****************************************************************************
**	cvrt_time_long(s,d)
**
**	converts fields ofstruct in HOST format to ieee format
**	   or 
**	converts fields of struct in NET format to fields with HOST 
**		format
****************************************************************************/

LOCAL void cvrt_time_long(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_time_long	*pSrc = s;
	struct dbr_time_long	*pDest = d;

    	/* convert vax to ieee or ieee to vax format -- same code*/
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
	pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
	pDest->stamp.nsec 	= dbr_ntohl(pSrc->stamp.nsec);

    	if (num == 1)	/* single value */
		pDest->value = dbr_ntohl(pSrc->value);
    	else		/* array chan-- multiple pts */
    	{
		cvrt_long(&pDest->value, &pSrc->value, encode, num);
    	}
}


/*
 *	cvrt_put_ackt()
 *
 *
 *
 *
 */
LOCAL void cvrt_put_ackt(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	dbr_put_ackt_t	*pSrc = s;
	dbr_put_ackt_t	*pDest = d;
      	unsigned long	i;

      	for(i=0; i<num; i++){
      	  	*pDest = dbr_ntohs( *pSrc );
		/*
		 * dont increment these inside the MACRO 
		 */
		pDest++;
		pSrc++;
	}
}


/****************************************************************************
**	cvrt_stsack_string(s,d)
**		struct dbr_stsack_string *s  	pointer to source struct
**		struct dbr_stsack_string *d	pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**	      
**	converts fields of struct in HOST format to NET format
**	   or 
**	converts fields of struct in NET format to fields with HOST 
**		format;
****************************************************************************/

LOCAL void cvrt_stsack_string(
void		*s,			/* source			*/
void		*d,			/* destination			*/
int 		encode,			/* cvrt HOST to NET if T	*/
unsigned long	num			/* number of values		*/
)
{
	struct dbr_stsack_string	*pSrc = s;
	struct dbr_stsack_string 	*pDest = d;
			
    	/* convert ieee to vax format or vax to ieee */
    	pDest->status 		= dbr_ntohs(pSrc->status);
    	pDest->severity		= dbr_ntohs(pSrc->severity);
    	pDest->ackt		= dbr_ntohs(pSrc->ackt);
    	pDest->acks		= dbr_ntohs(pSrc->acks);

	/* convert "in place" -> nothing else to do */
	if (s == d)
		return;

    	if (num == 1)	/* if single value */
		strcpy(pDest->value, pSrc->value);
    	else
		memcpy(pDest->value, pSrc->value, (MAX_STRING_SIZE * num));

}


#if defined(CA_FLOAT_MIT) 
/************************************************************************/
/*      double convert 				                        */
/*      (THIS ASSUMES IEEE IS THE NETWORK FLOATING POINT FORMAT)        */
/************************************************************************/

/* (this includes mapping of fringe reals to zero or infinity) */
/* (byte swaps included in conversion */

struct ieeedbl {
        unsigned int    mant2 : 32;
        unsigned int    mant1 : 20;
        unsigned int    exp   : 11;
        unsigned int    sign  : 1;
};

#define IEEE_DBL_SB             1023

/*      Conversion Range        */
/*      -1022<exp<1024  with mantissa of form 1.mant                    */
#define DBLEXPMINIEEE   -1022   /* min for norm # IEEE exponent */

struct mitdbl {
        unsigned int    mant1 : 7;
        unsigned int    exp   : 8;
        unsigned int    sign  : 1;
        unsigned int    mant2 : 16;
        unsigned int    mant3 : 16;
        unsigned int    mant4 : 16;
};

  /*    Exponent sign bias      */
#define        MIT_DBL_SB      129

  /*    Conversion Ranges       */
  /*    -128<exp<126    with mantissa of form 1.mant                    */
#define        DBLEXPMAXMIT    126     /* max MIT exponent             */
#define        DBLEXPMINMIT    -128    /* min MIT exponent             */

/*
 * Converts VAX D floating point to IEEE double precision 
 * (D floating is the VAX C default)
 */
void dbr_htond(dbr_double_t *pHost, dbr_double_t *pNet)
{
	dbr_double_t	copyin;
	struct mitdbl 	*pMIT;
	struct ieeedbl 	*pIEEE;
    	ca_uint32_t	*ptmp;
    	ca_uint32_t	tmp;

	/*
	 * Use internal buffer so the src and dest ptr
	 * can be identical
	 */
	copyin = *pHost;
	pMIT = (struct mitdbl *) &copyin;
	pIEEE = (struct ieeedbl *) pNet;

    	if( ((int)pMIT->exp) < (DBLEXPMINMIT+MIT_DBL_SB) ){
      		pIEEE->mant1 = 0;
      		pIEEE->mant2 = 0;
      		pIEEE->exp  = 0;
      		pIEEE->sign = 0;
    	}
    	else{
      		pIEEE->exp  = ((int)pMIT->exp)+(IEEE_DBL_SB-MIT_DBL_SB);
     		pIEEE->mant1 = (pMIT->mant1<<13) | (pMIT->mant2>>3);
      		pIEEE->mant2 = (pMIT->mant2<<29) | (pMIT->mant3<<13) | 
        				(pMIT->mant4>>3); 
      		pIEEE->sign = pMIT->sign;
    	}

    	/*
    	 * byte swap to net order
     	 */
    	ptmp = (ca_uint32_t *) pNet;
    	tmp = dbr_htonl(ptmp[0]);
    	ptmp[0] = dbr_htonl(ptmp[1]);
    	ptmp[1] = tmp;
}


/*
 * Converts IEEE double precision to VAX D floating point
 * (D floating is the VAX default)
 *
 * sign must be forced to zero if the exponent is zero to prevent a reserved
 * operand fault- joh 9-13-90
 */
void dbr_ntohd(dbr_double_t *pNet, dbr_double_t *pHost)
{
	struct ieeedbl	copyin;
	struct mitdbl 	*pMIT;
	struct ieeedbl 	*pIEEE;
    	ca_uint32_t	*ptmp;
    	ca_uint32_t	tmp;

	pIEEE = (struct ieeedbl *)pNet;
	pMIT = (struct mitdbl *)pHost;

	/*
	 * Use internal buffer so the src and dest ptr
	 * can be identical
	 */
	copyin = *pIEEE;
	pIEEE = &copyin;

	/*
	 * Byte swap from net order to host order
	 */
    	ptmp = (ca_uint32_t *) pIEEE;
    	tmp = dbr_htonl(ptmp[0]);
    	ptmp[0] = dbr_htonl(ptmp[1]);
    	ptmp[1] = tmp;

    	if( ((int)pIEEE->exp) > (DBLEXPMAXMIT + IEEE_DBL_SB) ){
      		pMIT->sign = pIEEE->sign; 
      		pMIT->exp = DBLEXPMAXMIT + MIT_DBL_SB; 
      		pMIT->mant1 = ~0; 
      		pMIT->mant2 = ~0; 
      		pMIT->mant3 = ~0; 
      		pMIT->mant4 = ~0; 
    	}
    	else if( ((int)pIEEE->exp) < (DBLEXPMINMIT + IEEE_DBL_SB) ){
      		pMIT->sign = 0; 
      		pMIT->exp = 0; 
      		pMIT->mant1 = 0; 
      		pMIT->mant2 = 0; 
      		pMIT->mant3 = 0; 
      		pMIT->mant4 = 0; 
    	}
    	else{
      		pMIT->sign = pIEEE->sign; 
      		pMIT->exp = ((int)pIEEE->exp)+(MIT_DBL_SB-IEEE_DBL_SB); 
      		pMIT->mant1 = pIEEE->mant1>>13; 
      		pMIT->mant2 = (pIEEE->mant1<<3) | (pIEEE->mant2>>29); 
      		pMIT->mant3 =  pIEEE->mant2>>13; 
      		pMIT->mant4 =  pIEEE->mant2<<3; 
    	}
} 

/************************************************************************/
/*      (THIS ASSUMES IEEE IS THE NETWORK FLOATING POINT FORMAT)        */
/************************************************************************/

struct ieeeflt{
  unsigned      mant    :23;
  unsigned      exp     :8;
  unsigned      sign    :1;
};

/*      Exponent sign bias      */
#define IEEE_SB         127

/*      Conversion Range        */
/*      -126<exp<127    with mantissa of form 1.mant                    */
#define EXPMINIEEE      -126            /* min for norm # IEEE exponent */


struct mitflt{
    unsigned    mant1   :7;
    unsigned    exp     :8;
    unsigned    sign    :1;
    unsigned    mant2   :16;
};

/*    Exponent sign bias      */
# define        MIT_SB          129

/*    Conversion Ranges       */
/*    -128<exp<126    with mantissa of form 1.mant                    */
# define        EXPMAXMIT       126     /* max MIT exponent             */
# define        EXPMINMIT       -128    /* min MIT exponent             */

/* 
 * (this includes mapping of fringe reals to zero or infinity) 
 * (byte swaps included in conversion 
 *
 * Uses internal buffer so the src and dest ptr
 * can be identical
 */
void dbr_htonf(dbr_float_t *pHost, dbr_float_t *pNet)
{
	struct mitflt	*pMIT = (struct mitflt *) pHost;
	struct ieeeflt	*pIEEE = (struct ieeeflt *) pNet;
    	long  		exp,mant,sign;

    	sign = pMIT->sign;

	if( ((int)pMIT->exp) < EXPMINIEEE + MIT_SB){
		exp       = 0;
		mant      = 0;
		sign      = 0;
	}
	else{
		exp = ((int)pMIT->exp)-MIT_SB+IEEE_SB;
		mant = (pMIT->mant1<<16) | pMIT->mant2;
    	}
    	pIEEE->mant   = mant;
    	pIEEE->exp    = exp;
    	pIEEE->sign   = sign;
    	*(ca_uint32_t *)pIEEE = dbr_htonl(*(ca_uint32_t *)pIEEE);
}


/*
 * sign must be forced to zero if the exponent is zero to prevent a reserved
 * operand fault- joh 9-13-90
 *
 * Uses internal buffer so the src and dest ptr
 * can be identical
 */
void dbr_ntohf(dbr_float_t *pNet, dbr_float_t *pHost)
{
	struct mitflt	*pMIT = (struct mitflt *) pHost;
	struct ieeeflt	*pIEEE = (struct ieeeflt *) pNet;
  	long  		exp,mant2,mant1,sign;

    	*(ca_uint32_t *)pIEEE = dbr_ntohl(*(ca_uint32_t *)pIEEE);
    	if( ((int)pIEEE->exp) > EXPMAXMIT + IEEE_SB){
		sign      = pIEEE->sign;
		exp       = EXPMAXMIT + MIT_SB;
		mant2     = ~0;
		mant1     = ~0;
    	}
    	else if( pIEEE->exp == 0){
		sign      = 0;
		exp       = 0;
		mant2     = 0;
		mant1     = 0;
    	}
    	else{
		sign      = pIEEE->sign;
		exp       = pIEEE->exp+MIT_SB-IEEE_SB;
		mant2     = pIEEE->mant;
		mant1     = pIEEE->mant>>(unsigned)16;
    	}
    	pMIT->exp      = exp;
    	pMIT->mant2    = mant2;
    	pMIT->mant1    = mant1;
    	pMIT->sign     = sign;
}


#endif /*CA_FLOAT_MIT*/

#if defined(CA_FLOAT_IEEE) && 0

/*
 * dbr_htond ()
 * performs only byte swapping
 */
void dbr_htond (dbr_double_t *IEEEhost, dbr_double_t *IEEEnet)
{
#ifdef CA_LITTLE_ENDIAN
    	ca_uint32_t	*pHost = (ca_uint32_t *) IEEEhost;
    	ca_uint32_t	*pNet = (ca_uint32_t *) IEEEnet;
	ca_uint32_t	tmp;

    	/*
    	 * byte swap to net order
	 * (assume that src and dest ptrs
	 * may be identical)
     	 */
	tmp = pHost[0];
	pNet[0] = dbr_htonl (pHost[1]);
	pNet[1] = dbr_htonl (tmp);	
#else
	*IEEEnet = *IEEEhost;
#endif
}

/*
 * dbr_ntohd ()
 * performs only byte swapping
 */
void dbr_ntohd (dbr_double_t *IEEEnet, dbr_double_t *IEEEhost)
{
#ifdef CA_LITTLE_ENDIAN
    	ca_uint32_t	*pHost = (ca_uint32_t *) IEEEhost;
    	ca_uint32_t	*pNet = (ca_uint32_t *) IEEEnet;
	ca_uint32_t	tmp;

    	/*
    	 * byte swap to net order
	 * (assume that src and dest ptrs
	 * may be identical)
     	 */
	tmp = pNet[0];
	pHost[0] = dbr_ntohl (pNet[1]);
	pHost[1] = dbr_ntohl (tmp);	
#else
	*IEEEhost = *IEEEnet;
#endif
}

/*
 * dbr_ntohf ()
 * performs only byte swapping
 */
void dbr_ntohf (dbr_float_t *IEEEnet, dbr_float_t *IEEEhost)
{
    	ca_uint32_t	*pHost = (ca_uint32_t *) IEEEhost;
    	ca_uint32_t	*pNet = (ca_uint32_t *) IEEEnet;

    	*pHost = dbr_ntohl (*pNet);
}

/*
 * dbr_htonf ()
 * performs only byte swapping
 */
void dbr_htonf (dbr_float_t *IEEEhost, dbr_float_t *IEEEnet)
{
    	ca_uint32_t	*pHost = (ca_uint32_t *) IEEEhost;
    	ca_uint32_t	*pNet = (ca_uint32_t *) IEEEnet;

    	*pNet = dbr_htonl (*pHost);
}

#endif /* IEEE float and little endian */


#endif /* CONVERSION_REQUIRED */

