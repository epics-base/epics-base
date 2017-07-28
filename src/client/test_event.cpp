/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *
 *  T E S T _ E V E N T . C
 *  Author: Jeffrey O. Hill
 *  simple stub for testing monitors
 */

#include "epicsStdioRedirect.h"

#define epicsExportSharedSymbols
#include "cadef.h"

extern "C" void epicsShareAPI ca_test_event ( struct event_handler_args args )
{
    chtype nativeType = ca_field_type ( args.chid );
    const char * pNativeTypeName = "<invalid>";
    if ( VALID_DB_REQ ( nativeType ) ) {
        pNativeTypeName = dbr_text[nativeType];
    }
    else {
        if ( nativeType == TYPENOTCONN ) {
            pNativeTypeName = "<disconnected>";
        }
    }

    printf ( "ca_test_event() for channel \"%s\" with native type %s\n", 
        ca_name(args.chid), pNativeTypeName );

    if ( ! ( CA_M_SUCCESS & args.status ) ) {
        printf ( "Invalid CA status \"%s\"\n", ca_message ( args.status ) );
        return;
    }

    if ( args.dbr ) {
        ca_dump_dbr ( args.type, args.count, args.dbr );
    }
}

/*
 * ca_dump_dbr()
 * dump the specified dbr type to stdout
 */
extern "C" void epicsShareAPI ca_dump_dbr ( 
    chtype type, unsigned count, const void * pbuffer )
{
    unsigned i;
    char tsString[50];

    if ( INVALID_DB_REQ ( type ) ) {
        printf ( "bad DBR type %ld\n", type );
    }

    printf ( "%s\t", dbr_text[type] );

    switch ( type ) {
    case DBR_STRING:
    {
        dbr_string_t    *pString = (dbr_string_t *) pbuffer;

        for(i=0; i<count && (*pString)[0]!='\0'; i++) {
            if(count!=1 && (i%5 == 0)) printf("\n");
            printf("%s ", *pString);
            pString++;
        }
        break;
    }
    case DBR_SHORT:
    {
        dbr_short_t *pvalue = (dbr_short_t *)pbuffer;
        for (i = 0; i < count; i++,pvalue++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%d ",* (short *)pvalue);
        }
        break;
    }
    case DBR_ENUM:
    {
        dbr_enum_t *pvalue = (dbr_enum_t *)pbuffer;
        for (i = 0; i < count; i++,pvalue++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%d ",*pvalue);
        }
        break;
    }
    case DBR_FLOAT:
    {
        dbr_float_t *pvalue = (dbr_float_t *)pbuffer;
        for (i = 0; i < count; i++,pvalue++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%6.4f ",*(float *)pvalue);
        }
        break;
    }
    case DBR_CHAR:
    {
        dbr_char_t  *pvalue = (dbr_char_t *) pbuffer;

        for (i = 0; i < count; i++,pvalue++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%u ",*pvalue);
        }
        break;
    }
    case DBR_LONG:
    {
        dbr_long_t *pvalue = (dbr_long_t *)pbuffer;
        for (i = 0; i < count; i++,pvalue++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%d ",*pvalue);
        }
        break;
    }
    case DBR_DOUBLE:
    {
        dbr_double_t *pvalue = (dbr_double_t *)pbuffer;
        for (i = 0; i < count; i++,pvalue++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%6.4f ",(float)(*pvalue));
        }
        break;
    }
    case DBR_STS_STRING:
    case DBR_GR_STRING:
    case DBR_CTRL_STRING:
    {
        struct dbr_sts_string *pvalue 
          = (struct dbr_sts_string *) pbuffer;
        printf("%2d %2d",pvalue->status,pvalue->severity);
        printf("\tValue: %s",pvalue->value);
        break;
    }
    case DBR_STS_ENUM:
    {
        struct dbr_sts_enum *pvalue
          = (struct dbr_sts_enum *)pbuffer;
        dbr_enum_t *pEnum = &pvalue->value;
        printf("%2d %2d",pvalue->status,pvalue->severity);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pEnum++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%u ",*pEnum);
        }
        break;
    }
    case DBR_STS_SHORT:
    {
        struct dbr_sts_short *pvalue
          = (struct dbr_sts_short *)pbuffer;
        dbr_short_t *pshort = &pvalue->value;
        printf("%2d %2d",pvalue->status,pvalue->severity);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pshort++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%u ",*pshort);
        }
        break;
    }
    case DBR_STS_FLOAT:
    {
        struct dbr_sts_float *pvalue
          = (struct dbr_sts_float *)pbuffer;
        dbr_float_t *pfloat = &pvalue->value;
        printf("%2d %2d",pvalue->status,pvalue->severity);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pfloat++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%6.4f ",*pfloat);
        }
        break;
    }
    case DBR_STS_CHAR:
    {
        struct dbr_sts_char *pvalue
          = (struct dbr_sts_char *)pbuffer;
        dbr_char_t *pchar = &pvalue->value;

        printf("%2d %2d",pvalue->status,pvalue->severity);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pchar++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%u ", *pchar);
        }
        break;
    }
    case DBR_STS_LONG:
    {
        struct dbr_sts_long *pvalue
          = (struct dbr_sts_long *)pbuffer;
        dbr_long_t *plong = &pvalue->value;
        printf("%2d %2d",pvalue->status,pvalue->severity);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,plong++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%d ",*plong);
        }
        break;
    }
    case DBR_STS_DOUBLE:
    {
        struct dbr_sts_double *pvalue
          = (struct dbr_sts_double *)pbuffer;
        dbr_double_t *pdouble = &pvalue->value;
        printf("%2d %2d",pvalue->status,pvalue->severity);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pdouble++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%6.4f ",(float)(*pdouble));
        }
        break;
    }
    case DBR_TIME_STRING:
    {
        struct dbr_time_string *pvalue 
          = (struct dbr_time_string *) pbuffer;

                epicsTimeToStrftime(tsString,sizeof(tsString),
                    "%Y/%m/%d %H:%M:%S.%06f",&pvalue->stamp);
        printf("%2d %2d",pvalue->status,pvalue->severity);
        printf("\tTimeStamp: %s",tsString);
        printf("\tValue: ");
        printf("%s",pvalue->value);
        break;
    }
    case DBR_TIME_ENUM:
    {
        struct dbr_time_enum *pvalue
          = (struct dbr_time_enum *)pbuffer;
        dbr_enum_t *pshort = &pvalue->value;

                epicsTimeToStrftime(tsString,sizeof(tsString),
                    "%Y/%m/%d %H:%M:%S.%06f",&pvalue->stamp);
        printf("%2d %2d",pvalue->status,pvalue->severity);
        printf("\tTimeStamp: %s",tsString);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pshort++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%d ",*pshort);
        }
        break;
    }
    case DBR_TIME_SHORT:
    {
        struct dbr_time_short *pvalue
          = (struct dbr_time_short *)pbuffer;
        dbr_short_t *pshort = &pvalue->value;
        epicsTimeToStrftime(tsString,sizeof(tsString),
            "%Y/%m/%d %H:%M:%S.%06f",&pvalue->stamp);
        printf("%2d %2d",
            pvalue->status,
            pvalue->severity);
        printf("\tTimeStamp: %s",tsString);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pshort++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%d ",*pshort);
        }
        break;
    }
    case DBR_TIME_FLOAT:
    {
        struct dbr_time_float *pvalue
          = (struct dbr_time_float *)pbuffer;
        dbr_float_t *pfloat = &pvalue->value;

                epicsTimeToStrftime(tsString,sizeof(tsString),
                    "%Y/%m/%d %H:%M:%S.%06f",&pvalue->stamp);
        printf("%2d %2d",pvalue->status,pvalue->severity);
        printf("\tTimeStamp: %s",tsString);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pfloat++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%6.4f ",*pfloat);
        }
        break;
    }
    case DBR_TIME_CHAR:
    {
        struct dbr_time_char *pvalue
          = (struct dbr_time_char *)pbuffer;
        dbr_char_t *pchar = &pvalue->value;

                epicsTimeToStrftime(tsString,sizeof(tsString),
                    "%Y/%m/%d %H:%M:%S.%06f",&pvalue->stamp);
        printf("%2d %2d",pvalue->status,pvalue->severity);
        printf("\tTimeStamp: %s",tsString);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pchar++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%d ",(short)(*pchar));
        }
        break;
    }
    case DBR_TIME_LONG:
    {
        struct dbr_time_long *pvalue
          = (struct dbr_time_long *)pbuffer;
        dbr_long_t *plong = &pvalue->value;

                epicsTimeToStrftime(tsString,sizeof(tsString),
                    "%Y/%m/%d %H:%M:%S.%06f",&pvalue->stamp);
        printf("%2d %2d",pvalue->status,pvalue->severity);
        printf("\tTimeStamp: %s",tsString);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,plong++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%d ",*plong);
        }
        break;
    }
    case DBR_TIME_DOUBLE:
    {
        struct dbr_time_double *pvalue
          = (struct dbr_time_double *)pbuffer;
        dbr_double_t *pdouble = &pvalue->value;

                epicsTimeToStrftime(tsString,sizeof(tsString),
                    "%Y/%m/%d %H:%M:%S.%06f",&pvalue->stamp);
        printf("%2d %2d",pvalue->status,pvalue->severity);
        printf("\tTimeStamp: %s",tsString);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pdouble++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%6.4f ",(float)(*pdouble));
        }
        break;
    }
    case DBR_GR_SHORT:
    {
        struct dbr_gr_short *pvalue
          = (struct dbr_gr_short *)pbuffer;
        dbr_short_t *pshort = &pvalue->value;
        printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
            pvalue->units);
        printf("\n\t%8d %8d %8d %8d %8d %8d",
          pvalue->upper_disp_limit,pvalue->lower_disp_limit,
          pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
          pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pshort++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%d ",*pshort);
        }
        break;
    }
    case DBR_GR_FLOAT:
    {
        struct dbr_gr_float *pvalue
          = (struct dbr_gr_float *)pbuffer;
        dbr_float_t *pfloat = &pvalue->value;
        printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
            pvalue->units);
        printf(" %3d\n\t%8.3f %8.3f %8.3f %8.3f %8.3f %8.3f",
          pvalue->precision,
          pvalue->upper_disp_limit,pvalue->lower_disp_limit,
          pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
          pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pfloat++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%6.4f ",*pfloat);
        }
        break;
    }
    case DBR_GR_ENUM:
    {
        struct dbr_gr_enum *pvalue
          = (struct dbr_gr_enum *)pbuffer;
        printf("%2d %2d",pvalue->status,
            pvalue->severity);
        printf("\tValue: %d",pvalue->value);
        if(pvalue->no_str>0) {
            printf("\n\t%3d",pvalue->no_str);
            for (i = 0; i < (unsigned) pvalue->no_str; i++)
                printf("\n\t%.26s",pvalue->strs[i]);
        }
        break;
    }
    case DBR_CTRL_ENUM:
    {
        struct dbr_ctrl_enum *pvalue
          = (struct dbr_ctrl_enum *)pbuffer;
        printf("%2d %2d",pvalue->status,
            pvalue->severity);
        printf("\tValue: %d",pvalue->value);
        if(pvalue->no_str>0) {
            printf("\n\t%3d",pvalue->no_str);
            for (i = 0; i < (unsigned) pvalue->no_str; i++)
                printf("\n\t%.26s",pvalue->strs[i]);
        }
        break;
    }
    case DBR_GR_CHAR:
    {
        struct dbr_gr_char *pvalue
          = (struct dbr_gr_char *)pbuffer;
        dbr_char_t *pchar = &pvalue->value;
        printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
            pvalue->units);
        printf("\n\t%8d %8d %8d %8d %8d %8d",
          pvalue->upper_disp_limit,pvalue->lower_disp_limit,
          pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
          pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pchar++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%u ",*pchar);
        }
        break;
    }
    case DBR_GR_LONG:
    {
        struct dbr_gr_long *pvalue
          = (struct dbr_gr_long *)pbuffer;
        dbr_long_t *plong = &pvalue->value;
        printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
            pvalue->units);
        printf("\n\t%8d %8d %8d %8d %8d %8d",
          pvalue->upper_disp_limit,pvalue->lower_disp_limit,
          pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
          pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,plong++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%d ",*plong);
        }
        break;
    }
    case DBR_GR_DOUBLE:
    {
        struct dbr_gr_double *pvalue
          = (struct dbr_gr_double *)pbuffer;
        dbr_double_t *pdouble = &pvalue->value;
        printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
            pvalue->units);
        printf(" %3d\n\t%8.3f %8.3f %8.3f %8.3f %8.3f %8.3f",
          pvalue->precision,
          (float)(pvalue->upper_disp_limit),
          (float)(pvalue->lower_disp_limit),
          (float)(pvalue->upper_alarm_limit),
          (float)(pvalue->upper_warning_limit),
          (float)(pvalue->lower_warning_limit),
          (float)(pvalue->lower_alarm_limit));
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pdouble++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%6.4f ",(float)(*pdouble));
        }
        break;
    }
    case DBR_CTRL_SHORT:
    {
        struct dbr_ctrl_short *pvalue
          = (struct dbr_ctrl_short *)pbuffer;
        dbr_short_t *pshort = &pvalue->value;
        printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
            pvalue->units);
        printf("\n\t%8d %8d %8d %8d %8d %8d",
          pvalue->upper_disp_limit,pvalue->lower_disp_limit,
          pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
          pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
        printf(" %8d %8d",
          pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pshort++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%d ",*pshort);
        }
        break;
    }
    case DBR_CTRL_FLOAT:
    {
        struct dbr_ctrl_float *pvalue
          = (struct dbr_ctrl_float *)pbuffer;
        dbr_float_t *pfloat = &pvalue->value;
        printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
            pvalue->units);
        printf(" %3d\n\t%8.3f %8.3f %8.3f %8.3f %8.3f %8.3f",
          pvalue->precision,
          pvalue->upper_disp_limit,pvalue->lower_disp_limit,
          pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
          pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
        printf(" %8.3f %8.3f",
          pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pfloat++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%6.4f ",*pfloat);
        }
        break;
    }
    case DBR_CTRL_CHAR:
    {
        struct dbr_ctrl_char *pvalue
          = (struct dbr_ctrl_char *)pbuffer;
        dbr_char_t *pchar = &pvalue->value;
        printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
            pvalue->units);
        printf("\n\t%8d %8d %8d %8d %8d %8d",
          pvalue->upper_disp_limit,pvalue->lower_disp_limit,
          pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
          pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
        printf(" %8d %8d",
          pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pchar++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%4d ",(short)(*pchar));
        }
        break;
    }
    case DBR_CTRL_LONG:
    {
        struct dbr_ctrl_long *pvalue
          = (struct dbr_ctrl_long *)pbuffer;
        dbr_long_t *plong = &pvalue->value;
        printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
            pvalue->units);
        printf("\n\t%8d %8d %8d %8d %8d %8d",
          pvalue->upper_disp_limit,pvalue->lower_disp_limit,
          pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
          pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
        printf(" %8d %8d",
          pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,plong++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%d ",*plong);
        }
        break;
    }
    case DBR_CTRL_DOUBLE:
    {
        struct dbr_ctrl_double *pvalue
          = (struct dbr_ctrl_double *)pbuffer;
        dbr_double_t *pdouble = &pvalue->value;
        printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
            pvalue->units);
        printf(" %3d\n\t%8.3f %8.3f %8.3f %8.3f %8.3f %8.3f",
          pvalue->precision,
          (float)(pvalue->upper_disp_limit),
          (float)(pvalue->lower_disp_limit),
          (float)(pvalue->upper_alarm_limit),
          (float)(pvalue->upper_warning_limit),
          (float)(pvalue->lower_warning_limit),
          (float)(pvalue->lower_alarm_limit));
        printf(" %8.3f %8.3f",
          (float)(pvalue->upper_ctrl_limit),
          (float)(pvalue->lower_ctrl_limit));
        if(count==1) printf("\tValue: ");
        for (i = 0; i < count; i++,pdouble++){
            if(count!=1 && (i%10 == 0)) printf("\n");
            printf("%6.6f ",(float)(*pdouble));
        }
        break;
    }
    case DBR_STSACK_STRING:
    {
		struct dbr_stsack_string *pvalue
		  = (struct dbr_stsack_string *)pbuffer;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf(" %2d %2d",pvalue->ackt,pvalue->acks);
		printf(" %s",pvalue->value);
		break;
    }
    case DBR_CLASS_NAME:
    {
        dbr_class_name_t * pvalue =
            ( dbr_class_name_t * ) pbuffer;
        printf ( "%s", *pvalue );
        break;
    }
    default:
        printf ( 
            "unsupported by ca_dbrDump()" );
        break;
    }
    printf("\n");
}

