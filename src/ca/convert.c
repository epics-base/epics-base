#include	<db_access.h>
#include	<net_convert.h>

#ifdef vxWorks
#  define memcpy(D,S,N)	bcopy(S,D,N)
#  define memset(D,V,N)	bfill(D,N,V)
#endif


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
**	cvrt_sts_int(s,d)
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

cvrt_sts_int(s,d,encode,num)
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
**	cvrt_gr_int(s,d)
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

cvrt_gr_int(s,d,encode,num)
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
**	cvrt_ctrl_int(s,d)
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

cvrt_ctrl_int(s,d,encode,num)
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
