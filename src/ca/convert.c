/*
 *	C O N V E R T . C
 *
 *	Author: D. Kersteins
 *
 *
 * 	NOTES: 
 *	1) converts arrays without subroutine call
 *	for improved efficiency -joh
 *
 *	2) These routines only do VAX conversions for now - joh
 *
 *	3) All routines in this file have an encode argument which
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
 *
 *
 */



#include	<db_access.h>
#include	<net_convert.h>

#ifdef vxWorks
#  define memcpy(D,S,N)	bcopy(S,D,N)
#  define memset(D,V,N)	bfill(D,N,V)
#endif


/*
 *	Routine we will do later
 *
 *
 *
 */
no_cvrt()
{
	ca_printf("Sorry, conversion for that type currently not implemented\n");
	exit();
}
cvrt_sts_char()		{no_cvrt();}
cvrt_sts_long()		{no_cvrt();}
cvrt_sts_double()	{no_cvrt();}

cvrt_time_string()	{no_cvrt();}
cvrt_time_short()	{no_cvrt();}
cvrt_time_float()	{no_cvrt();}
cvrt_time_enum()	{no_cvrt();}
cvrt_time_char()	{no_cvrt();}
cvrt_time_long()	{no_cvrt();}
cvrt_time_double()	{no_cvrt();}

cvrt_gr_char()		{no_cvrt();}
cvrt_gr_double()	{no_cvrt();}
cvrt_gr_long()		{no_cvrt();}
cvrt_gr_string()	{no_cvrt();}
cvrt_gr_enum()		{no_cvrt();}

cvrt_ctrl_char()	{no_cvrt();}
cvrt_ctrl_long()	{no_cvrt();}
cvrt_ctrl_string()	{no_cvrt();}
cvrt_ctrl_double()	{no_cvrt();}





/*
 *	CVRT_SHORT()
 *
 *
 *
 *
 */
cvrt_short(s,d,encode,num)
short			*s;			/* source			*/
short			*d;			/* destination			*/
int 			encode;			/* cvrt VAX to IEEE if T	*/
int			num;			/* number of values		*/
{
      	unsigned int	i;

      	for(i=0; i<num; i++){
      	  	*d++ = ntohs( *s++ );
	}
	return 1;
}


/*
 *	CVRT_CHAR()
 *
 *
 *
 *
 */
cvrt_char(s,d,encode,num)
char			*s;			/* source			*/
char			*d;			/* destination			*/
int 			encode;			/* cvrt VAX to IEEE if T	*/
int			num;			/* number of values		*/
{
      	unsigned int	i;

      	for(i=0; i<num; i++){
      	  	*d++ = *s++;
	}
	return 1;
}


/*
 *	CVRT_LONG()
 *
 *
 *
 *
 */
cvrt_long(s,d,encode,num)
long			*s;			/* source			*/
long			*d;			/* destination			*/
int 			encode;			/* cvrt VAX to IEEE if T	*/
int			num;			/* number of values		*/
{
      	unsigned int	i;

      	for(i=0; i<num; i++){
      	  	*d++ = ntohl( *s++ );
	}
	return 1;
}


/*
 *	CVRT_ENUM()
 *
 *
 *
 *
 */
cvrt_enum(s,d,encode,num)
short			*s;			/* source			*/
short			*d;			/* destination			*/
int 			encode;			/* cvrt VAX to IEEE if T	*/
int			num;			/* number of values		*/
{
      	unsigned int	i;

      	for(i=0; i<num; i++){
      	  	*d++ = ntohs(*s++);
	}
	return 1;
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
cvrt_float(s,d,encode,num)
float			*s;			/* source		*/
float			*d;			/* destination		*/
int 			encode;			/* cvrt VAX to IEEE if T*/
int			num;			/* number of values	*/
{
      	unsigned int	i;

      	for(i=0; i<num; i++){
		if(encode){
	  		htonf(s, d);
		}
		else{
	  		ntohf(s, d);
		}
		/*
		 * incrementing these inside the MACRO could be unhealthy
		 */
		s++;
		d++;
	}

	return 1;
}


/*
 *	CVRT_DOUBLE()
 *
 *
 *
 *
 */
cvrt_double(s,d,encode,num)
double			*s;			/* source			*/
double			*d;			/* destination			*/
int 			encode;			/* cvrt VAX to IEEE if T	*/
int			num;			/* number of values		*/
{
      	unsigned int	i;

	ca_printf("CA: sorry no code for conversions of doubles\n");
	ca_printf("CA: setting your variable to nill\n");
      	for(i=0; i<num; i++){
	  	*d++ = 0.0;
	}

	return 1;
}


/*
 *	CVRT_STRING()
 *
 *
 *
 *
 */
cvrt_string(s,d,encode,num)
char			*s;			/* source			*/
char			*d;			/* destination			*/
int 			encode;			/* cvrt VAX to IEEE if T	*/
int			num;			/* number of values		*/
{
      	unsigned int	i;

	if(num == 1)
            	strcpy(d, s);
	else
		memcpy(d, s, (MAX_STRING_SIZE * num));
	return 1;
}






/****************************************************************************
**	cvrt_sts_string(s,d)
**		struct dbr_sts_string *s  	pointer to source struct
**		struct dbr_sts_string *d	pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**	      
**	converts fields of struct in VAX format to IEEE format
**	   or 
**	converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

cvrt_sts_string(s,d,encode,num)
struct dbr_sts_string	*s;			/* source		*/
struct dbr_sts_string	*d;			/* destination		*/
int encode;					/* do VAX to IEEE 	*/
int			num;			/* number of values	*/
{
			
    /* convert ieee to vax format or vax to ieee */
    d->status 		= ntohs(s->status);
    d->severity		= ntohs(s->severity);
    if (num == 1)	/* if single value */
	strcpy(d->value,s->value);
    else
	memcpy(d->value,s->value,(MAX_STRING_SIZE * num));

  return 1;
}


/****************************************************************************
**	cvrt_sts_short(s,d)
**		struct dbr_sts_int *s  	pointer to source struct
**		struct dbr_sts_int *d	pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	converts fields ofstruct in VAX format to ieee format
**	   or 
**	converts fields of struct in IEEE format to fields with VAX 
**		format
****************************************************************************/

cvrt_sts_short(s,d,encode,num)
struct dbr_sts_int	*s;			/* source		*/
struct dbr_sts_int	*d;			/* destination		*/
int 			encode;			/* if true, vax to ieee	*/
int			num;			/* number of values	*/
{
    register i;
    short	*sval_ptr,*dval_ptr;	/* ptrs to source, destination */

    /* convert vax to ieee or ieee to vax format -- same code*/
    d->status 		= ntohs(s->status);
    d->severity		= ntohs(s->severity);

    if (num == 1)	/* single value */
	d->value = ntohs(s->value);
    else		/* array chan-- multiple pts */
    {
	sval_ptr = &(s->value);
	dval_ptr = &(d->value);
	for (i = 0; i < num; i++){
	    *dval_ptr = ntohs(*sval_ptr);
	    sval_ptr++;
	    dval_ptr++;
	}
    }


  return 1;
}




/****************************************************************************
**	cvrt_sts_float(s,d)
**		struct dbr_sts_float *s  	pointer to source struct
**		struct dbr_sts_float *d		pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	   if encode 
**		converts struct in VAX format to ieee format
**	   else 
**		converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

cvrt_sts_float(s,d,encode,num)
struct dbr_sts_float	*s;			/* source		*/
struct dbr_sts_float	*d;			/* destination		*/
int 			encode;			/* it true, vax to ieee */
int			num;			/* number of values	*/
{
    register i;
    float *sval_ptr,*dval_ptr;

    d->status 		= ntohs(s->status);
    d->severity		= ntohs(s->severity);

    if(encode)					/* vax to ieee convert */
	if(num == 1){
    	    htonf(&s->value, &d->value);
	}
	else 
	{
	    sval_ptr = &(s->value);
	    dval_ptr = &(d->value);
	    for (i = 0;i < num; i++){
		htonf(sval_ptr,dval_ptr);
		sval_ptr++;
		dval_ptr++;
	   }
	}

    else					/* ieee to vax convert */
	if(num == 1){
	    ntohf(&s->value,&d->value);
	}
	else
	{
	    sval_ptr = &(s->value);
	    dval_ptr = &(d->value);
	    for (i = 0;i < num; i++){
		ntohf(sval_ptr,dval_ptr);
		dval_ptr++;
		sval_ptr++;
	    }
	}

  return 1;
}




/****************************************************************************
**	cvrt_sts_enum(s,d)
**		struct dbr_sts_enum *s  	pointer to source struct
**		struct dbr_sts_enum *d		pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	converts fields of struct in IEEE format to fields with VAX format
**		 or  
**	converts fields of struct in VAX format to fields with IEEE format
**	 
****************************************************************************/

cvrt_sts_enum(s,d,encode,num)
struct dbr_sts_enum	*s;			/* source		*/
struct dbr_sts_enum	*d;			/* destination		*/
int 			encode;			/* if true, vax to ieee */
int			num;			/* number of values	*/
{
    register i;
    short *dval_ptr,*sval_ptr;

    /* convert vax to ieee or ieee to vax --same code */
    d->status 		= ntohs(s->status);
    d->severity		= ntohs(s->severity);
    if (num == 1)
        d->value	= ntohs(s->value);
    else {
	sval_ptr = &(s->value);
	dval_ptr = &(d->value);
	for (i = 0; i < num; i++){
	   *dval_ptr 	= ntohs(*sval_ptr);
	    dval_ptr++;
	    sval_ptr++;
	}
    }
  return 1;
}




/****************************************************************************
**	cvrt_gr_short(s,d)
**		struct dbr_gr_int *s  	pointer to source struct
**		struct dbr_gr_int *d	pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	converts fields of struct in IEEE format to fields with VAX format
**		 or  
**	converts fields of struct in VAX format to fields with IEEE format
**	 
****************************************************************************/

cvrt_gr_short(s,d,encode,num)
struct dbr_gr_int	*s;			/* source		*/
struct dbr_gr_int	*d;			/* destination		*/
int 			encode;			/* if true, vax to ieee */
int			num;			/* number of values	*/
{
    register i;
    short *sval_ptr,*dval_ptr;

    /* convert ieee to vax or vax to ieee -- same code */
    d->status 			= ntohs(s->status);
    d->severity			= ntohs(s->severity);
    strcpy(d->units,s->units);

    d->upper_disp_limit 	= ntohs(s->upper_disp_limit);
    d->lower_disp_limit 	= ntohs(s->lower_disp_limit);
    d->upper_alarm_limit 	= ntohs(s->upper_alarm_limit);
    d->upper_warning_limit 	= ntohs(s->upper_warning_limit);
    d->lower_alarm_limit 	= ntohs(s->lower_alarm_limit);
    d->lower_warning_limit 	= ntohs(s->lower_warning_limit);

    if (num == 1)
    	d->value = ntohs(s->value);
    else {
	sval_ptr = &(s->value);
	dval_ptr = &(d->value);
	for (i = 0;i < num; i++){
	    *dval_ptr = ntohs(*sval_ptr);
	    dval_ptr++;
	    sval_ptr++;
	}
    }


  return 1;
}




/****************************************************************************
**	cvrt_gr_float(s,d)
**		struct dbr_gr_float *d		pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	   if encode 
**		converts struct in VAX format to ieee format
**	   else 
**		converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

cvrt_gr_float(s,d,encode,num)
struct dbr_gr_float	*s;			/* source		*/
struct dbr_gr_float	*d;			/* destination		*/
int			encode;			/* if true, vax to ieee */
int			num;			/* number of values	*/
{
    register i;
    float *sval_ptr,*dval_ptr;

    /* these are same for vax to ieee or ieee to vax */
    d->status 			= ntohs(s->status);
    d->severity			= ntohs(s->severity);
    d->precision		= ntohs(s->precision);
    strcpy(d->units,s->units);

    if (encode)			/* vax to ieee convert		*/
    {
	if (num == 1){
	    htonf(&s->value, &d->value);
	}
	else {
	    sval_ptr = &(s->value);
	    dval_ptr = &(d->value);
	    for (i = 0;i < num; i++){
		htonf(sval_ptr,dval_ptr);
		dval_ptr++;
		sval_ptr++;
	    }
	}
    	htonf(&s->upper_disp_limit,&d->upper_disp_limit);
    	htonf(&s->lower_disp_limit, &d->lower_disp_limit);
    	htonf(&s->upper_alarm_limit, &d->upper_alarm_limit);
    	htonf(&s->upper_warning_limit, &d->upper_warning_limit);
    	htonf(&s->lower_alarm_limit, &d->lower_alarm_limit);
    	htonf(&s->lower_warning_limit, &d->lower_warning_limit);
    }
    else			/* ieee to vax convert 	 	*/
    {
	if (num == 1){
	    ntohf(&s->value, &d->value);
	}
	else {
	    sval_ptr = &(s->value);
	    dval_ptr = &(d->value);
	    for (i = 0;i < num; i++){
		ntohf(sval_ptr,dval_ptr);
		dval_ptr++;
		sval_ptr++;
	    }
	}
    	ntohf(&s->upper_disp_limit,&d->upper_disp_limit);
    	ntohf(&s->lower_disp_limit, &d->lower_disp_limit);
    	ntohf(&s->upper_alarm_limit, &d->upper_alarm_limit);
    	ntohf(&s->upper_warning_limit, &d->upper_warning_limit);
    	ntohf(&s->lower_alarm_limit, &d->lower_alarm_limit);
    	ntohf(&s->lower_warning_limit, &d->lower_warning_limit);
    }
	
    
  return 1;
}




/****************************************************************************
**	cvrt_ctrl_short(s,d)
**		struct dbr_gr_int *s  	pointer to source struct
**		struct dbr_gr_int *d	pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	converts fields of struct in IEEE format to fields with VAX format
**		 or  
**	converts fields of struct in VAX format to fields with IEEE format
**	 
****************************************************************************/

cvrt_ctrl_short(s,d,encode,num)
struct dbr_ctrl_int	*s;			/* source		*/
struct dbr_ctrl_int	*d;			/* destination		*/
int			encode;			/* if true, vax to ieee */
int			num;			/* number of values	*/
{
    register i;
    short *sval_ptr,*dval_ptr;

    /* vax to ieee or ieee to vax -- same code */
    d->status 			= ntohs(s->status);
    d->severity			= ntohs(s->severity);
    strcpy(d->units,s->units);

    d->upper_disp_limit 	= ntohs(s->upper_disp_limit);
    d->lower_disp_limit 	= ntohs(s->lower_disp_limit);
    d->upper_alarm_limit 	= ntohs(s->upper_alarm_limit);
    d->upper_warning_limit 	= ntohs(s->upper_warning_limit);
    d->lower_alarm_limit 	= ntohs(s->lower_alarm_limit);
    d->lower_warning_limit 	= ntohs(s->lower_warning_limit);
    d->lower_ctrl_limit 	= ntohs(s->lower_ctrl_limit);
    d->upper_ctrl_limit 	= ntohs(s->upper_ctrl_limit);

    if (num == 1)
    	d->value = ntohs(s->value);
    else {
	sval_ptr = &(s->value);
	dval_ptr = &(d->value);
	for (i = 0;i < num; i++){
	    *dval_ptr = ntohs(*sval_ptr);
	    dval_ptr++;
	    sval_ptr++;
	}
    }
	

  return 1;
}




/****************************************************************************
**	cvrt_ctrl_float(s,d)
**		struct dbr_ctrl_float *s  	pointer to source struct
**		struct dbr_ctrl_float *d	pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	   if encode 
**		converts struct in VAX format to ieee format
**	   else 
**		converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

cvrt_ctrl_float(s,d,encode,num)
struct dbr_ctrl_float	*s;			/* source		*/
struct dbr_ctrl_float	*d;			/* destination		*/
int 			encode;			/* if true, vax to ieee */
int			num;			/* number of values	*/
{
    register i;
    float *sval_ptr,*dval_ptr;

    /* these are the same for ieee to vaax or vax to ieee */
    d->status 			= ntohs(s->status);
    d->severity			= ntohs(s->severity);
    d->precision		= ntohs(s->precision);
    strcpy(d->units,s->units);
    if (encode)			/* vax to ieee convert		*/
    {
	if (num == 1){
	    htonf(&s->value, &d->value);
	}
	else {
	    sval_ptr = &(s->value);
	    dval_ptr = &(d->value);
	    for (i = 0;i < num; i++){
		htonf(sval_ptr,dval_ptr);
		dval_ptr++;
		sval_ptr++;
	    }
	}
    	htonf(&s->upper_disp_limit,&d->upper_disp_limit);
    	htonf(&s->lower_disp_limit, &d->lower_disp_limit);
    	htonf(&s->upper_alarm_limit, &d->upper_alarm_limit);
    	htonf(&s->upper_warning_limit, &d->upper_warning_limit);
    	htonf(&s->lower_alarm_limit, &d->lower_alarm_limit);
    	htonf(&s->lower_warning_limit, &d->lower_warning_limit);
    	htonf(&s->lower_ctrl_limit, &d->lower_ctrl_limit);
    	htonf(&s->upper_ctrl_limit, &d->upper_ctrl_limit);
    }
    else			/* ieee to vax convert 	 	*/
    {
	if (num == 1){
	    ntohf(&s->value, &d->value);
	}
	else {
	    sval_ptr = &(s->value);
	    dval_ptr = &(d->value);
	    for (i = 0;i < num; i++){
		ntohf(sval_ptr,dval_ptr);
		dval_ptr++;
		sval_ptr++;
	    }
	}
    	ntohf(&s->lower_disp_limit, &d->lower_disp_limit);
    	ntohf(&s->upper_alarm_limit, &d->upper_alarm_limit);
    	ntohf(&s->upper_warning_limit, &d->upper_warning_limit);
    	ntohf(&s->lower_alarm_limit, &d->lower_alarm_limit);
    	ntohf(&s->lower_warning_limit, &d->lower_warning_limit);
    	ntohf(&s->lower_ctrl_limit, &d->lower_ctrl_limit);
    	ntohf(&s->upper_ctrl_limit, &d->upper_ctrl_limit);
    }


  return 1;
}


/****************************************************************************
**	cvrt_ctrl_enum(s,d)
**		struct dbr_ctrl_enum *s 	pointer to source struct
**		struct dbr_ctrl_enum *d		pointer to destination struct
**		int  encode;			boolean, if true vax to ieee
**							 else ieee to vax
**
**	   if encode 
**		converts struct in VAX format to ieee format
**	   else 
**		converts fields of struct in IEEE format to fields with VAX 
**		format;
****************************************************************************/

cvrt_ctrl_enum(s,d,encode,num)
struct dbr_ctrl_enum	*s;			/* source		*/
struct dbr_ctrl_enum	*d;			/* destination		*/
int 			encode;			/* if true, vax to ieee */
int			num;			/* number of values	*/
{
    register i;
    short *sval_ptr,*dval_ptr;

    /* vax to ieee or ieee to vax -- same code */
    d->status 			= ntohs(s->status);
    d->severity			= ntohs(s->severity);
    d->no_str			= ntohs(s->no_str);
    memcpy(d->strs,s->strs,sizeof(s->strs));

    if (num == 1)	/* single value */
	d->value = ntohs(s->value);
    else		/* array chan-- multiple pts */
    {
	sval_ptr = &(s->value);
	dval_ptr = &(d->value);
	for (i = 0; i < num; i++){
	    *dval_ptr = ntohs(*sval_ptr);
	    dval_ptr++;
	    sval_ptr++;
	}
    }

  return 1;
}
