/* dbCaLink.c */
/* share/src/db $Id$ */

/****************************************************************
*
*      Author:		Nicholas T. Karonis
*      Date:		01-01-92
*
*      Experimental Physics and Industrial Control System (EPICS)
*
*      Copyright 1991, the Regents of the University of California,
*      and the University of Chicago Board of Governors.
*
*      This software was produced under  U.S. Government contracts:
*      (W-7405-ENG-36) at the Los Alamos National Laboratory,
*      and (W-31-109-ENG-38) at Argonne National Laboratory.
*
*      Initial development by:
*              The Controls and Automation Group (AT-8)
*              Ground Test Accelerator
*              Accelerator Technology Division
*              Los Alamos National Laboratory
*
*      Co-developed with
*              The Controls and Computing Group
*              Accelerator Systems Division
*              Advanced Photon Source
*              Argonne National Laboratory
*
* Modification Log:
* -----------------
*
****************************************************************/

#include <errno.h>

#include <cadef.h>
#include <caerr.h>
#include <db_access.h>

/* needed for PVNAME_SZ and FLDNAME_SZ */
#include <dbDefs.h>

/* needed for typedef of caddr_t which is used somehwhere in link.h */
/* typedef char *  caddr_t */
#include <types.h>

/* needed for struct link */
#include <link.h>

/* needed for FastLock stuff */
#include <fast_lock.h>

/* needed for alarm handling */
#include <alarm.h>

/* needed for spawnTask priority parms */
#include <taskLib.h>

/* needed for #define memcpy() */
#include <string.h>

#include <task_params.h>

#include <errMdef.h>
#include <calink.h>

#define PVARNAMELENGTH ((PVNAME_SZ)+(FLDNAME_SZ)+2)
#define NODESPERCALLOC 10

#define BOOL char

/*******************/
/*                 */
/* Data Structures */
/*                 */
/*******************/

/* for input links */

struct input_pvar
{
    struct input_pvar *next;
    void *pdest_dbaddr;
    char source_name[PVARNAMELENGTH];
    chid cid;
    char dest_name[PVARNAMELENGTH];
    void *pvalue;
    short dest_old_dbr_type;
    short dest_old_dbr_sts_type; /* for checking in my_event_handler() */
    short revised_dest_new_dbr_type;
    unsigned short source_severity;
    int dest_old_dbr_element_size; /* for calloc() and memcpy() */
    FAST_LOCK lock;
    BOOL maximize_severity;
    BOOL needs_reading;
    long dest_nelements; /* for calloc() and check in my_event_handler() */
    long last_rcvd_nelements; /* set in my_event_handler() and used in */
			      /* dbCaGetLink()'s call to dbCaDbPut()   */
}; /* end struct input_pvar */

/* for output links */

struct output_pvar
{
    struct output_pvar *next;
    struct output_pvar *write_next;
    void *psource_dbaddr;
    void *pvalue;
    char dest_name[PVARNAMELENGTH];
    char source_name[PVARNAMELENGTH];
    chid cid;
    short source_old_dbr_type; /* for output demon ca_put() */
    short source_new_dbr_type; /* for dbCaPutLink() call to dbGetField() */
    short revised_source_new_dbr_type;
    FAST_LOCK lock;
    BOOL valid_value;
    BOOL on_writelist;
    long source_nelements; /* for calloc() and check on dbCaPutLink() */
    long last_nelements_written; /* set by dbCaPutLink(); ca_array_put() uses */
}; /* end struct output_pvar */

/*******************************/
/*                             */
/* Internally Global Variables */
/*                             */
/*******************************/

static struct input_pvar *Input_pvar_avail_hdr;
static struct input_pvar *Pvar_inputlist_hdr;

static struct output_pvar *Output_pvar_avail_hdr;
static struct output_pvar *Pvar_outputlist_hdr;
static struct output_pvar *Pvar_writelist_hdr;
static struct output_pvar *Pvar_disconnectedlist_hdr;

static FAST_LOCK Buffer;

/* Binary Semaphore to signal inspect Pvar_outputlist_hdr */
/* static FAST_LOCK LookAtBuffer; */
static SEM_ID LookAtBuffer;

/****************************************/
/*                                      */
/* Externally Visibile Remote Functions */
/*                                      */
/****************************************/

/* in dbCaDbLink.c */

long dbCaNameToAddr();
long dbCaCopyPvar();    
long dbCaMaximizeSeverity();
long dbCaDbPut();      
short dbCaNewDbfToNewDbr();       /* conversion */
/* these two should go away when channel access understands new db technology */
short dbCaNewDbrToOldDbr();       /* conversion */
short dbCaOldDbrToNewDbr();       /* conversion */
short dbCaOldDbrToOldDbrSts();    /* conversion */
/* these should be part of db access ... currently in dbCaDbLink.c */
char *dbCaRecnameFromDbCommon();
short dbCaDbfFromDbAddr();
long dbCaNelementsFromDbAddr();

/**************************************/
/*                                    */
/* Externally Visible Local Functions */
/*                                    */
/**************************************/

long dbCaAddInlink();   /* called for each input CA_LINK DURING rec init */
long dbCaAddOutlink();  /* called for each output CA_LINK DURING rec init */
long dbCaGetLink();         /* called by rec proc when inputing on CA_LINK */
void dbCaLinkInit();        /* called once BEFORE any rec init */
void dbCaProcessInlinks();  /* SPAWNED once AFTER all rec init */
void dbCaProcessOutlinks(); /* SPAWNED once AFTER all rec init */
long dbCaPutLink();         /* called by rec proc when outputing on CA_LINK */

/***************************/
/*                         */
/* Private Local Functions */
/*                         */
/***************************/

static long flush_channel_request_buffer();
static void init_global_vars();
static void my_connection_handler();
static void my_event_handler();
static struct input_pvar *pop_input_pvar();
static struct output_pvar *pop_output_pvar();
static long process_asynch_events();
static void process_asynch_events_task();
static void push_input_pvar();
static void push_output_pvar();
static long queue_add_event_from_input_pvar();
static long queue_ca_array_put();
static long queue_ca_build_and_connect();
static long queue_ca_search();
static long task_initialize_channel_access();

/* diagnostic tools */
static void stat_to_string();
static void severity_to_string();

/****************************************************************
*
* EXTERNALLY VISIBLE FUNCTIONS
*
****************************************************************/

/****************************************************************
*
* long dbCaAddInlink(plink, precord, dest_fieldname)
* struct link *plink;
* void *precord;
* char *dest_fieldname;
*
* Description:
*     During record initialization a record was found to have an input link
* whose source is in a different physical database (on another IOC).  This 
* function registers that input link as remote and changes he link type from
* PV_LINK to CA_LINK.
*
* Input:
*     struct link *plink        pointer to input link structure
*     char *dest_recname        name of destination record
*     char *dest_fieldname      name of destiantion field
*     short dest_new_dbf_type   dest DBF type using NEW db technology
*     int dest_fld_size         size of destination field
*
* Output: None.
*
* Returns:
*     any rc from dbNameToAddr()
*     S_dbCa_nullarg      - received a NULL pointer in one of the input args
*     S_dbCa_failedmalloc - could not dynamically allocate memory
*
* Notes:
*     In registering this input link, the db address of the destination process 
* variable is recorded in a separate list through a function call to 
* dbCaNameToAddr() found in dbCaLink.c.
*
****************************************************************/

long dbCaAddInlink(plink, pdest_record, dest_fieldname)
struct link *plink;
void *pdest_record;
char *dest_fieldname;
{

struct input_pvar *pi;
char *dest_recname;
char errmsg[100];
long rc;

    if (plink)
    {
	if (pdest_record)
	{
	    if (dest_fieldname)
	    {
		if (pi = pop_input_pvar())
		{
		    if (dest_recname = dbCaRecnameFromDbCommon(pdest_record))
		    {
			/* moving dest name into struct input_pvar */
			strncpy(pi->dest_name, dest_recname, PVNAME_SZ);
			pi->dest_name[PVNAME_SZ] = '\0';
			strcat(pi->dest_name, ".");
			strncat(pi->dest_name, dest_fieldname, FLDNAME_SZ);

			/* in dbCaLink.c */
			rc = dbCaNameToAddr(pi->dest_name, 
			    &(pi->pdest_dbaddr)); 

			if (RTN_SUCCESS(rc))
			{
			    /* new DBF -> old DBR */
			    pi->dest_old_dbr_type = 
    dbCaNewDbrToOldDbr(dbCaNewDbfToNewDbr(dbCaDbfFromDbAddr(pi->pdest_dbaddr)));

			    if (dbr_type_is_valid(pi->dest_old_dbr_type))
			    {
				pi->revised_dest_new_dbr_type =
				    dbCaOldDbrToNewDbr(pi->dest_old_dbr_type);

				pi->dest_old_dbr_element_size =
				    (int) dbr_size[pi->dest_old_dbr_type];

				if ((pi->dest_nelements = 
				    dbCaNelementsFromDbAddr(pi->pdest_dbaddr)) 
					!= -1L)
				{
				    /* memory allocation */
				    if (pi->pvalue = 
			    (void *) calloc ((unsigned int) pi->dest_nelements, 
				    (unsigned) pi->dest_old_dbr_element_size)) 
				    {
/* printf("dbCaAddInlink(): nelements %d * dest old dbr size %d  = %d bytes at starting at >%x<\n", (unsigned int) pi->dest_nelements, (unsigned) pi->dest_old_dbr_element_size, (unsigned int) pi->dest_nelements * (unsigned) pi->dest_old_dbr_element_size, pi->pvalue); */

					/* old DBR -> old DBR_STS */
					pi->dest_old_dbr_sts_type =
				dbCaOldDbrToOldDbrSts(pi->dest_old_dbr_type);

					/* moving source name */
					/* into struct input_pvar */
					strncpy(pi->source_name, 
					    plink->value.pv_link.pvname, 
					    PVNAME_SZ);
					pi->source_name[PVNAME_SZ] = '\0';
					strcat(pi->source_name, ".");
					strncat(pi->source_name, 
					    plink->value.pv_link.fldname, 
					    FLDNAME_SZ);

					/* preserving MS/NMS specification */
					if (plink->value.pv_link.maximize_sevr)
					    pi->maximize_severity = TRUE;
					else
					    pi->maximize_severity = FALSE;

					/* pushing this input_pvar */
					/* onto the list           */

					pi->next = Pvar_inputlist_hdr;
					Pvar_inputlist_hdr = pi;

					/* changing the link type and */
					/* connecting this input_pvar */
					/* the the struct link   */

					plink->type = CA_LINK;
					plink->value.ca_link = (void *) pi;
				    }
				    else
				    {
					rc = S_dbCa_failedmalloc;
					sprintf(errmsg, 
		"ERROR: dbCaAddInlink() unable to calloc %d bytes for pvalue",
			pi->dest_nelements * pi->dest_old_dbr_element_size); 
					errMessage(S_dbCa_failedmalloc, errmsg);

					push_input_pvar(pi);
				    } /* endif */
				}
				else
				{
				    rc = S_dbCa_dbfailure;

				    errMessage(S_dbCa_dbfailure,
			    "ERROR: dbCaAddInlink() unable to find nelements");

				    push_input_pvar(pi);
				} /* endif */
			    }
			    else
			    {
				rc = S_dbCa_dbfailure;

				errMessage(S_dbCa_dbfailure,
		"ERROR: dbCaAddInlink() unable to convert new DBF->old DBR");

				push_input_pvar(pi);
			    } /* endif */
			}
			else
			{
			    rc = S_dbCa_dbfailure;

			    sprintf(errmsg,
		    "ERROR: dbCaAddInlink() unable to find db addr for >%s<", 
				pi->dest_name);
			    errMessage(S_dbCa_dbfailure, errmsg);

			    push_input_pvar(pi);
			} /* endif */
		    }
		    else
		    {
			rc = S_dbCa_dbfailure;
			errMessage(S_dbCa_dbfailure,
			    "ERROR: dbCaAddInlink() found NULL dest_recname");

			push_input_pvar(pi);
		    } /* endif */
		}
		else
		{
		    rc = S_dbCa_failedmalloc;
			errMessage(S_dbCa_failedmalloc, 
		    "ERROR: dbCaAddInlink() could not pop_input_pvar()");
		} /* endif */
	    }
	    else
	    {
		rc = S_dbCa_nullarg;
		errMessage(S_dbCa_nullarg, 
		    "ERROR: dbCaAddInlink() got NULL dest_fieldname");
	    } /* endif */
	}
	else
	{
	    rc = S_dbCa_nullarg;
	    errMessage(S_dbCa_nullarg,
		"ERROR: dbCaAddInlink() got NULL pdest_record");
	} /* endif */
    }
    else
    {
	rc = S_dbCa_nullarg;
	errMessage(S_dbCa_nullarg,
	    "ERROR: dbCaAddInlink() got NULL plink");
    } /* endif */

    return rc;

} /* end dbCaAddInlink() */

/****************************************************************
*
* Description:
*     During record initialization a record was found to have an output link
* whose destination is in a different physical database (on another IOC).  This 
* function registers that output link as remote and changes he link type from
* PV_LINK to CA_LINK.
*
* Input:
*     struct link *plink          pointer to output link structure
*     char *source_recname        name of source record
*     char *source_fieldname      name of source field
*     short source_new_dbf_type   source DBF type using NEW db technology
*     int source_fld_size         size of source field
*
* Output: None.
*
* Returns:
*     any rc from dbNameToAddr()
*     S_dbCa_nullarg      - received a NULL pointer in one of the input args
*     S_dbCa_failedmalloc - could not dynamically allocate memory
*
* Notes:
*     In registering this output link, the db address of the source process 
* variable is recorded in a separate list through a function call to 
* dbCaNameToAddr() found in dbCaLink.c.
*
****************************************************************/

long dbCaAddOutlink(plink, psource_record, source_fieldname)
struct link *plink;
void *psource_record;
char *source_fieldname;
{

struct output_pvar *po;
int source_old_dbr_element_size;
char *source_recname;
char errmsg[100];
long rc;

    if (plink)
    {
	if (psource_record)
	{
	    if (source_fieldname)
	    {
		if (po = pop_output_pvar())
		{
		    if (source_recname = 
			    dbCaRecnameFromDbCommon(psource_record))
		    {
			/* moving source name into struct output_pvar */

			strncpy(po->source_name, source_recname, PVNAME_SZ);
			po->source_name[PVNAME_SZ] = '\0';
			strcat(po->source_name, ".");
			strncat(po->source_name, source_fieldname, FLDNAME_SZ);

			/* in dbCaDblink.c */
			rc = dbCaNameToAddr(po->source_name, 
			    &(po->psource_dbaddr));

			if (RTN_SUCCESS(rc))
			{
			    /* new DBF -> old DBR */
			    po->source_old_dbr_type = 
			dbCaNewDbrToOldDbr(
			    dbCaNewDbfToNewDbr(
				dbCaDbfFromDbAddr(po->psource_dbaddr)));

			    if (dbr_type_is_valid(po->source_old_dbr_type))
			    {
				po->revised_source_new_dbr_type =
				    dbCaOldDbrToNewDbr(po->source_old_dbr_type);

				source_old_dbr_element_size =
				    (int) dbr_size[po->source_old_dbr_type];

				if ((po->source_nelements = 
				    dbCaNelementsFromDbAddr(po->psource_dbaddr))
					!= -1L)
				{
				    /* memory allocation */
				    if (po->pvalue = 
			(void *) calloc ((unsigned int) po->source_nelements, 
			    (unsigned) source_old_dbr_element_size))
				    {
					/* moving dest name into */
					/* struct output_pvar */

					strncpy(po->dest_name, 
					    plink->value.pv_link.pvname, 
					    PVNAME_SZ);
					po->dest_name[PVNAME_SZ] = '\0';
					strcat(po->dest_name, ".");
					strncat(po->dest_name, 
					    plink->value.pv_link.fldname, 
					    FLDNAME_SZ);

					/* for dbGetField() */
					po->source_new_dbr_type = 
					    dbCaNewDbfToNewDbr(
						dbCaDbfFromDbAddr(
						    po->psource_dbaddr));

					po->valid_value = FALSE;
					po->on_writelist = FALSE;

					/* pushing this output_pvar */
					/* onto the list */

					po->next = Pvar_outputlist_hdr;
					Pvar_outputlist_hdr = po;

					/* changing the link type and */
					/* connecting this output_pvar */
					/* the the struct link  */

					plink->type = CA_LINK;
					plink->value.ca_link = (void *) po;
				    }
				    else
				    {
					rc = S_dbCa_failedmalloc;

					sprintf(errmsg, 
		"ERROR: dbCaAddOutlink() could not calloc %d bytes for pvar", 
			    po->source_nelements*source_old_dbr_element_size);
					errMessage(S_dbCa_failedmalloc, errmsg);

					push_output_pvar(po);

				    } /* endif */
				}
				else
				{
				    rc = S_dbCa_dbfailure;

				    errMessage(S_dbCa_dbfailure,
			    "ERROR: dbCaAddOutlink() unable to find nelements");

				    push_output_pvar(po);

				} /* endif */
			    }
			    else
			    {
				rc = S_dbCa_dbfailure;

				errMessage(S_dbCa_dbfailure,
		"ERROR: dbCaAddOutlink() unable to convert new DBF->old DBR");

				push_output_pvar(po);

			    } /* endif */
			}
			else
			{
			    push_output_pvar(po);
			} /* endif */

		    }
		    else
		    {
			rc = S_dbCa_dbfailure;
			errMessage(S_dbCa_dbfailure,
			    "ERROR: dbCaAddInlink() found NULL dest_recname");

			push_output_pvar(po);

		    } /* endif */
		}
		else
		{
		    rc = S_dbCa_failedmalloc;

		    errMessage(S_dbCa_failedmalloc, 
		    "ERROR: dbCaAddOutlink() could not pop_output_pvar()");
		} /* endif */

	    }
	    else
	    {
		rc = S_dbCa_nullarg;
		errMessage(S_dbCa_nullarg,
		    "ERROR: dbCaAddOutlink() NULL source_fieldname");
	    } /* endif */
	}
	else
	{
	    rc = S_dbCa_nullarg;
	    errMessage(S_dbCa_nullarg,
		"ERROR: dbCaAddOutlink() NULL psource_record");
	} /* endif */
    }
    else
    {
	rc = S_dbCa_nullarg;
	errMessage(S_dbCa_nullarg,
	    "ERROR: dbCaAddOutlink() got NULL plink");
    } /* endif */

    return rc;

} /* end dbCaAddOutlink() */

/****************************************************************
*
* long dbCaGetLink(plink)
* struct link *plink;
*
* Description:
*     A record is being processed that has an input link that refers to a 
* process variable on another physical database (another IOC).  This function 
* copies the value in the temporary store of the input node into the 
* destination.  If MS is specified on this link, the severity of the source 
* record is appropriately propogated to the destination record.
*
* Input:
*     struct link *plink   pointer to input link structure
*
* Output: None.
*
* Returns:
*     0                - Success
*     S_dbCa_nullarg   - received a NULL pointer in one of the input args
*     S_dbCa_foundnull - found a NULL pointer where one should not be
*     S_dbCa_notFound  - could not find source_pvarname in shared list
*     any rc from dbPut()
*
* Notes:
*     Presumably this link was properly registered during record initialization 
* by calling dbCaAddInlink().  If the monitor connection is broken, an alarm 
* condition is raised and no data is transferred.  The value is only copied 
* from the temporary store to the destination if its value changed since the 
* last read.  In copying the value from the temporary store to the destination, 
* a call is made to dbCaDbPut() in dbCaDbLink.c which ultimately calls dbPut().
*
****************************************************************/

long dbCaGetLink(plink)
struct link *plink;
{

struct input_pvar *pi;
long rc;

/* printf("ENTER dbCaGetLink()\n"); */
    if (plink)
    {
	if (pi = (struct input_pvar *) plink->value.ca_link)
	{

	    FASTLOCK(&(pi->lock));

	    if (ca_field_type(pi->cid) == TYPENOTCONN)
		/* channel is disconnected.  either    */
		/* NEVER connected or LOST connection. */

		/* in dbCaDblink.c */
		rc = dbCaMaximizeSeverity(pi->pdest_dbaddr, 
		    (unsigned short) VALID_ALARM, (unsigned short) LINK_ALARM);
	    else
	    {
/* printf("dbCaGetLink() found channel connected and needs_reading %c\n", (pi->needs_reading ? 'T' :'F')); */
		rc = 0L;

		if (pi->maximize_severity)   /* if input link is MS */
		    /* in dbCaDblink.c */
		    rc = dbCaMaximizeSeverity(pi->pdest_dbaddr, 
			    pi->source_severity, (unsigned short) LINK_ALARM);

		if (RTN_SUCCESS(rc) && pi->needs_reading)
		{
/* printf("dbCaGetLink() calling dbCaDbPut() sourceaddr >%x<\n", pi->pvalue); */
		    pi->needs_reading = FALSE;

		    /* in dbCaDbLink.c */
		    rc = dbCaDbPut(pi->pdest_dbaddr, 
			    pi->revised_dest_new_dbr_type, pi->pvalue, 
			    pi->last_rcvd_nelements);
		} /* endif */
	    } /* endif */

	    FASTUNLOCK(&(pi->lock));
	}
	else
	{
	    rc = S_dbCa_nullarg;
	    errMessage(S_dbCa_nullarg, "ERROR: dbCaGetLink() got NULL pi");
	} /* endif */
    }
    else
    {
	rc = S_dbCa_foundnull;
	errMessage(S_dbCa_foundnull, "ERROR: dbCaGetLink() found NULL plink");
    } /* endif */

/* printf("EXIT dbCaGetLink()\n"); */
    return rc;

} /* end dbCaGetLink() */

/****************************************************************
*
* void dbCaLinkInit(count)
* int count;
*
* Description:
*     This function is called twice during IOC initialization. The first time 
* it is called it initializes the all the appropriate memory.  The second time 
* it is called it spawns two new vxWorks tasks, one to process remote input 
* links and the other to process remote output links.
*
* Input:
*     int count    indicates which call this is (count = {1,2})
*
* Output:  None.
*
* Returns: None.
*
* Notes:
*     The first call is this function is made at the beginning of IOC 
* initialization and the second call is made at the very end of IOC 
* initialization.  During its first invocation, a call is made to 
* dbCaDbInlinkInit() in dbCaDblink.c to initialize that environment as well.
*
****************************************************************/

void dbCaLinkInit(count)
int count;
{

    if (count == 1)
	init_global_vars();

    if (count == 2)
    {
	taskSpawn(DB_CA_INPUT_NAME, DB_CA_INPUT_PRI, DB_CA_INPUT_OPT,
	    DB_CA_INPUT_STACK, (FUNCPTR) dbCaProcessInlinks);

	taskSpawn(DB_CA_OUTPUT_NAME, DB_CA_OUTPUT_PRI, DB_CA_OUTPUT_OPT,
	    DB_CA_OUTPUT_STACK, (FUNCPTR) dbCaProcessOutlinks);
    } /* endif */

} /* end dbCaLinkInit() */

/****************************************************************
*
* void dbCaProcessInlinks()
*
* Description:
*     This function is spawned during IOC initialization (initRecSup()) AFTER 
* processing all record initialization routines.  It process input links that 
* originate on remote process variables.
*
*     This function establishes monitors on the remote source process 
* variables and registers the my_event_handler() and the event handler routine.
* It then waits forever for events to occur.
*
* Input:   None.
*
* Output:  None.
*
* Returns: None.
*
* Notes:
*     If the database on this IOC does NOT have any input links that refer to 
* remote pvars, this function ends immediately and the spawned vxWorks task 
* dies.  Either this function terminates immediately or it never terminates.
*
****************************************************************/

void dbCaProcessInlinks()
{

struct input_pvar *pi;
long status;

    if (Pvar_inputlist_hdr)
    {
	/* have at least one remote db input link */

	status = task_initialize_channel_access();

	/* queueing ca_search()'s */

	for (pi = Pvar_inputlist_hdr; 
	    RTN_SUCCESS(status) && pi; 
		pi = pi->next)
	    status = queue_ca_search(pi->source_name, &(pi->cid));

	if (RTN_SUCCESS(status))
	{
	    status = flush_channel_request_buffer();

	    for (pi = Pvar_inputlist_hdr; 
		RTN_SUCCESS(status) && pi; 
		    pi = pi->next)
		status = queue_add_event_from_input_pvar(pi);

	    if (RTN_SUCCESS(status))
		if (RTN_SUCCESS(flush_channel_request_buffer()))
		    process_asynch_events((float) 0);
	} /* endif */
    } /* endif */

} /* end dbCaProcessInlinks() */

/****************************************************************
*
* void dbCaProcessOutlinks()
*
* Description:
*     This function is spawned during IOC initialization (initRecSup()) AFTER 
* processing all record initialization routines.  It process output links that 
* refer to remote process variables.  This function is the output demon.
*
*     This function establishes the connections to the remote destination 
* process variables and registers my_connection_handler() as the connection
* hanlder routine for each connection.  It then waits to be signaled.  Once 
* signaled, it writes all the values in Write List returns the nodes in the 
* Write List back to the Output List.
*
* Input:   None.
*
* Output:  None.
*
* Returns: None.
*
* Notes:
*     If the database on this IOC does NOT have any output links that refer
* to remote pvars, this function ends immediately and the spawned vxWorks task 
* dies.  Either this function terminates immediately or it never termintates.
*
*     This function is signaled during record processing to inspect the nodes
* on the Write List.  There is no guarantee that the connections will be in
* tact.  Nodes whose connections are broken are placed onto the Disconnected
* List (temporarily) and returned to the Write List (not the Output List)
* at the end.  When connections are re-established, the connection handler
* my_connection_handler() signals this function to inspect the Write List
* again.
*
****************************************************************/

void dbCaProcessOutlinks()
{

struct output_pvar *po;
long status;
BOOL done;


    if (Pvar_outputlist_hdr)
    {
	/* have at least one remote db output link */

	status = task_initialize_channel_access();

	taskSpawn(DB_CA_PROC_ASYNCH_EV_TASK_NAME, DB_CA_PROC_ASYNCH_EV_TASK_PRI,
	    DB_CA_PROC_ASYNCH_EV_TASK_OPT, DB_CA_PROC_ASYNCH_EV_TASK_STACK, 
	    (FUNCPTR) process_asynch_events_task, taskIdSelf());

	/* queueing ca_build_and_connect()'s */

	for (po = Pvar_outputlist_hdr; 
	    RTN_SUCCESS(status)&& po; 
		po = po->next)
	    status = queue_ca_build_and_connect(po);

	if (RTN_SUCCESS(status))
	    status = flush_channel_request_buffer();

/* printf("\n-- ENTERING OUTLINK MAIN LOOP\n\n"); */

	while (RTN_SUCCESS(status))
	{

	    /* P(LookAtBuffer) */
	    /* FASTLOCK(&LookAtBuffer); */
	    semTake(LookAtBuffer, WAIT_FOREVER);
/* printf("Output demon just woke up\n"); */

	    /* grabbing ALL the nodes in the writelist list ... one at a time */

	    done = FALSE;

	    while (!done && RTN_SUCCESS(status))
	    {
		FASTLOCK(&Buffer);
		if (po = Pvar_writelist_hdr)
		    Pvar_writelist_hdr = po->write_next;
		FASTUNLOCK(&Buffer);

		if (po)
		{
		    FASTLOCK(&(po->lock));

		    if (po->valid_value)
		    {
			/* we have a valid value here ... needs to be written */
			if (ca_field_type(po->cid) == TYPENOTCONN)
			{
			    /* raising an alarm disconnected */
			    status = dbCaMaximizeSeverity(po->psource_dbaddr, 
						(unsigned short) VALID_ALARM, 
						(unsigned short) LINK_ALARM);

			    /* pushing this node onto the disconnected list */
			    po->write_next = Pvar_disconnectedlist_hdr;
			    Pvar_disconnectedlist_hdr = po;
			}
			else
			{
			    po->on_writelist = FALSE;
/* printf("Output demon just before ca_array_put()\n"); */
			    status = queue_ca_array_put(po);
			    if (RTN_SUCCESS(status))
				status = flush_channel_request_buffer();
			} /* endif */
		    }
		    else
			/* the value here is invalid ...     */
			/* simply remove node from writelist */
			po->on_writelist = FALSE;

		    FASTUNLOCK(&(po->lock));
		}
		else
		    done = TRUE;
	    } /* endwhile */

	    /* moving the nodes on the disconnected */
	    /* list back onto the writelist         */
	    if (RTN_SUCCESS(status) && Pvar_disconnectedlist_hdr)
	    {

		/* there are nodes on the disconnected list */

		FASTLOCK(&Buffer);

		if (Pvar_writelist_hdr)
		{
		    /* moving nodes one at a time */
		    while (Pvar_disconnectedlist_hdr)
		    {
			po = Pvar_disconnectedlist_hdr;
			Pvar_disconnectedlist_hdr = po->write_next;

			po->write_next = Pvar_writelist_hdr;
			Pvar_writelist_hdr = po;
		    } /* endwhile */
		}
		else
		{
		    /* moving the whole list at once */
		    Pvar_writelist_hdr = Pvar_disconnectedlist_hdr;
		    Pvar_disconnectedlist_hdr = (struct output_pvar *) NULL;
		} /* endif */

		FASTUNLOCK(&Buffer);

	    } /* endif */
	} /* endwhile */
    } /* endif */

/* printf("dbCaProcessOutlinks() FOUND NO REMOTE OUTPUT PVARS\n"); */

} /* end dbCaProcessOutlinks() */

/****************************************************************
*
* long dbCaPutLink(plink, poptions, pnrequest)
* struct link *plink;
* long *poptions;
* long *pnrequest;
*
* Description:
*     A record is being processed that has an output link that refers to a 
* process variable on another physical database (another IOC).  This function 
* copies the value from the source pvar into the temporary store of the output 
* node.  It then places the output node onto the Write List and signals the
* output demon (dbCaProcessOutlinks()).
*
* Input:
*     struct link *plink   pointer to output link structure
*     long *poptions       as passed to dbGetField()
*     long *pnrequest      as passed to dbGetField()
*
* Output: None.
*
* Returns:
*     0                - Success or raised alarm
*     S_dbCa_nullarg   - received a NULL pointer in one of the input args
*     S_dbCa_notFound  - could not find source_pvarname in shared list
*     S_dbCa_foundnull - found a NULL pointer where one should not be
*     any rc from dbGetField()
*
* Notes:
*     Presumably this link was properly registered during record initialization 
* by calling dbCaAddOutlink().  In copying the value from the source pvar to
* the temporary store, a call is made to dbCaCopyPvar() in dbCaDblink.c which
* ultimately makes a call to dbGetField(), hence the need for the two arguments
* poptions and pnrequest.
* 
*     The connection handler (my_connection_handler()) is called whenever
* the connection status of the channel changes.  If the connection status
* goes from disconnected->connected, the connection hanlder checks if the
* value has ever been written to the remote pvar.  If it has, it immediately
* schedules the nodes value to be written again, even though the record was
* not processed, and signals the output demon.  Hence the need to record the
* fact that the value was ever written.
*
*     As this function is called during record processing, it cannot wait for
* anything for an extended period of time.  Before copying the value to the
* temporary store, it must lock the node on the Output List.  This function
* is contending for the lock with the output demon and the connection handler.
* The output demon locks the node and then issues a ca_put() ... that could
* take too long.  Therefore, a newly locking mechanism is employed here,
* FASTLOCKNOWAIT.  It attempts to grab the LOCK.  If it succeeds it returns
* TRUE.  If it fails it returns FALSE.  In either case, it returns immediately.
*
****************************************************************/

long dbCaPutLink(plink, poptions, pnrequest)
struct link *plink;
long *poptions;
long *pnrequest;
{

struct output_pvar *po;
char errmsg[100];
long rc;

    if (plink)
    {
	if (poptions)
	{
	    if (pnrequest)
	    {
		if (po = (struct output_pvar *) plink->value.ca_link)
		{
		    if (po->source_nelements <= *pnrequest)
		    {
			/* NOTE: there are three unlocks associated with */
			/* this lock.  only ONE unlock will be executed  */
			/* there are three of them for an optimization   */
			/* reason. We want to assure that the node is    */
			/* unlocked BEFORE V(LookAtBuffer), otherwise the */
			/* other task will wake up and HAVE to switch back */
			/* to this task in order to unlock the node and then */
			/* switch back again to resume processing.       */

			if (FASTLOCKNOWAIT(&(po->lock)))
			{
			    /* copying pvar value to output_pvar struct */
			    /* in dbCaDblink.c */
/* printf("dbCaPutLink() got LOCK and calling dbCaCopyPvar()\n"); */
			    rc = dbCaCopyPvar(po->psource_dbaddr, 
				    po->revised_source_new_dbr_type, 
				    po->pvalue, poptions, pnrequest);

			    if (RTN_SUCCESS(rc))
			    {
				/* successful copy */

				/* checking if just overwrote last value that */
				/* was never sent and raising an alarm if     */
				/* necessary.                                 */
				if (po->valid_value && po->on_writelist)
				    /* in dbCaDblink.c */
				rc = dbCaMaximizeSeverity(po->psource_dbaddr, 
					    (unsigned short) VALID_ALARM, 
					    (unsigned short) LINK_ALARM);

				po->valid_value = TRUE;
				po->last_nelements_written = *pnrequest;

				if (!(po->on_writelist))
				{
				    /* not on writelist ... placing there */
				    po->on_writelist = TRUE;

				    /* placing node onto write list */
				    FASTLOCK(&Buffer);
				    po->write_next = Pvar_writelist_hdr;
				    Pvar_writelist_hdr = po;
				    FASTUNLOCK(&Buffer);

				    /* V(LookAtBuffer) awakens */
				    /* ProcessOutlinks()       */

/* printf("dbCaPutLink() V(LookAtBuffer)\n"); */
				    /* FASTUNLOCK(&LookAtBuffer); */
				    semGive(LookAtBuffer);
				} /* endif */
			    }
			    else
			    {
				/* failed copy */
				po->valid_value = FALSE;
				
				/* raising an alarm to reflect failed copy   */
				/* have (arbitrarily) decided to change the  */
				/* rc in doing so ... could just as easily   */
				/* not have changed the rc and returned what */
				/* was returned from dbCaCopyPvar() instead  */
				    /* in dbCaDblink.c */
				rc = dbCaMaximizeSeverity(po->psource_dbaddr, 
					    (unsigned short) VALID_ALARM, 
					    (unsigned short) LINK_ALARM);
			    } /* endif */

			    FASTUNLOCK(&(po->lock));
			}
			else
{
/* printf("dbCaPutLink() could not get LOCK\n"); */
			    /* could not get lock immediately      */
			    /* aborting write and raising an alarm */

			    /* in dbCaDblink.c */
			    rc = dbCaMaximizeSeverity(po->psource_dbaddr, 
				    (unsigned short) VALID_ALARM, 
				    (unsigned short) LINK_ALARM);
}
		    }
		    else
		    {
			rc = S_dbCa_dbfailure;
			sprintf(errmsg,
			     "ERROR: dbCaPutLink() requested %ld max %ld",
			     *pnrequest, po->source_nelements);
			errMessage(S_dbCa_dbfailure, errmsg);
		    } /* endif */
		}
		else
		{
		    rc = S_dbCa_foundnull;
		    errMessage(S_dbCa_foundnull, 
			"ERROR: dbCaPutLink() found NULL po");
		} /* endif */
	    }
	    else
	    {
		rc = S_dbCa_nullarg;
		errMessage(S_dbCa_nullarg,
		    "ERROR: dbCaPutLink() got NULL pnrequest");
	    } /* endif */
	}
	else
	{
	    rc = S_dbCa_nullarg;
	    errMessage(S_dbCa_nullarg,
		"ERROR: dbCaPutLink() got NULL poptions");
	} /* endif */
    }
    else
    {
	rc = S_dbCa_nullarg;
	errMessage(S_dbCa_nullarg,
	    "ERROR: dbCaPutLink() got NULL plink");
    } /* endif */

    return rc;

} /* end dbCaPutLink() */

/****************************************************************
*
* PRIVATE FUNCTIONS
*
****************************************************************/

/****************************************************************
*
* static long flush_channel_request_buffer()
*
* Description:
*     Flushes the channel access buffer by calling ca_flush_io().
*
* Input:  None.
*
* Output: None.
*
* Returns:
*     0                 - Success
*     S_dbCa_unknownECA - unknown status rc from ca_flush_io()
*
* Notes:  None.
*
****************************************************************/

static long flush_channel_request_buffer()
{

int status;
char errmsg[100];
long rc;

    status = ca_flush_io();

    switch (status)
    {
	case ECA_NORMAL:     
	    rc = 0L; 
	    break;
	default:
	    rc = S_dbCa_unknownECA;
	    sprintf(errmsg, 
    "ERROR: flush_channel_request_buffer() unrecognizable status returned %d", 
		status);     
	    errMessage(S_dbCa_unknownECA, errmsg);
	    break;
    } /* end switch() */

    return rc;

} /* end flush_channel_request_buffer() */

/****************************************************************
*
* static void init_global_vars()
*
* Description:
*     Initializes the linked list headers to NULL.  Initializes the
* necessary binary semaphores appropriately.
*
* Input:   None.
*
* Output:  None.
*
* Returns: None.
*
* Notes:   None.
*
****************************************************************/

static void init_global_vars()
{

    /* Avail Headers */
    Input_pvar_avail_hdr = (struct input_pvar *) NULL;
    Output_pvar_avail_hdr = (struct output_pvar *) NULL;

    /* Input List and Output List */
    Pvar_inputlist_hdr = (struct input_pvar *) NULL;
    Pvar_outputlist_hdr = (struct output_pvar *) NULL;

    /* Write List and Disconnected List */
    Pvar_writelist_hdr = (struct output_pvar *) NULL;
    Pvar_disconnectedlist_hdr = (struct output_pvar *) NULL;

    /* Buffer LOCK */
    FASTLOCKINIT(&Buffer);

    /* Binary Semaphore: initial value = 0 */
    /* FASTLOCKINIT(&LookAtBuffer); */
    LookAtBuffer = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
    /* FASTLOCK(&LookAtBuffer); */
    /* semTake(LookAtBuffer, WAIT_FOREVER); */

} /* end init_global_vars() */

/****************************************************************
*
* static void my_connection_handler(arg)
* struct connection_handler_args arg;
*
* Description:
*     The connection status of one of the output channels has changed.  If
* the connection has gone from disconnected->connected, this routine re-writes
* the value to the destiantion pvar.
*
* Input:
*     struct connection_handler_args arg
*
*          from epicsH/cadef.h
*          struct  connection_handler_args
*          {
*              chid chid;   Channel id
*              long op;     External codes for channel access operations 
*          };
*
* Output:  None.
*
* Returns: None.
*
* Notes:
*     No matter where the node started when this function started, if the
* connection is now in tact and the node's value was ever writen, the node
* ends up on the Write List and the output demon is signaled.
*
****************************************************************/

static void my_connection_handler(arg)
struct connection_handler_args arg;
{

struct output_pvar *po;

/* printf("my_connection_handler() change in connection connection\n"); */
    if (arg.op == CA_OP_CONN_UP)
    {
/* printf("my_connection_handler() detected DOWN--->UP\n"); */
	/* connection on this channel re-established */

	if (po = (struct output_pvar *) ca_puser(arg.chid))
	{

	    FASTLOCK(&(po->lock));

	    if (po->valid_value)
	    {
/* printf("my_connection_handler() value needs to be written\n"); */
		/* This value has been ca_put()'d in the past.   */
		/* Need to do it now immediately to more closely */
		/* simulate database activities when all records */
		/* reside on a single IOC ... the source record  */
		/* (and hence the whole database) assumes that   */
		/* the target record has the last value written. */
		/* This helps to make that assumption reliable.  */

		if (!(po->on_writelist))
		{
/* printf("my_connection_handler() placing value onto writelist\n"); */
		    po->on_writelist = TRUE;

		    /* placing node onto write list */
		    FASTLOCK(&Buffer);
		    po->write_next = Pvar_writelist_hdr;
		    Pvar_writelist_hdr = po;
		    FASTUNLOCK(&Buffer);
		} /* endif */

		/* optimization to place unlock here ... before */
		/* V() ... but need it after *else* also        */

		FASTUNLOCK(&(po->lock));

/* printf("my_connection_handler() signalling V() output demon\n"); */
		/* V(LookAtBuffer) to awaken ProcessOutlinks() */
		/* FASTUNLOCK(&LookAtBuffer); */
		semGive(LookAtBuffer);
	    } 
	    else
		FASTUNLOCK(&(po->lock));
	}
	else
	    errMessage(S_dbCa_foundnull,
		"ERROR: my_connection_handler() found NULL po");
    } /* endif */

} /* end my_connection_handler() */

/****************************************************************
*
* static void my_event_handler(arg)
* struct event_handler_args arg;
*
* Description:
*     This function is the user-supplied event handler routine.  It gets 
* called whenever the an event occurs associated with one the remote source
* pvars.  It copies the value from the incoming event_handler_args to the
* temporary store of the input node.
*
* Input:
*     struct event_handler_args arg
*         from epicsH/cadef.h
*         struct  event_handler_args
*         {
*             void *usr;   User argument supplied when event added
*             chid chid;   Channel id
*             long type;   the type of the value returned (long aligned)
*             long count;  the element count of the item returned
*             void *dbr;   Pointer to the value returned
*         };
*
* Output:  None.
*
* Returns: None.
*
* Notes:
*     Presumably a monitor has been established on these remote source 
* pvars and a wait for events has already been issued by dbCaProcessInlinks().
*
*     After the value is copied to the temporary store, it is later read
* into the destination pvar during record processing via a call to 
* dbCaGetLink().  The indication that this node's value has been changed is
* an optimization to avoid moving redundant data during record processing.
*
*     arg.type is coming in as an old database DBR_STS_XXX type.  We cannot 
* convert it to new database DBR_XXX type here because this file only 
* understands old database types as a result of including db_access.h
*   
*     Because the arg.type is DBR_STS_XXX type, arg.dbr is a pointer to a data 
* structure that has short status, short severity, and xxxxx value.  Here are 
* the names of the different data structures based on the DBR_STS_XXX type as 
* copied from db_access.h
*
*      DBR_STS_STRING  returns a string status structure (dbr_sts_string)
*      DBR_STS_SHORT   returns a short status structure (dbr_sts_short)
*      DBR_STS_INT     returns a short status structure (dbr_sts_int)
*      DBR_STS_FLOAT   returns a float status structure (dbr_sts_float)
*      DBR_STS_ENUM    returns an enum status structure (dbr_sts_enum)
*      DBR_STS_CHAR    returns a char status structure (dbr_sts_char)
*      DBR_STS_LONG    returns a long status structure (dbr_sts_long)
*      DBR_STS_DOUBLE  returns a double status structure (dbr_sts_double)
*
*     Currently, DBR_STS_INT and DBR_STS_SHORT are defined to be the same
* thing, therefore they both cannot reside as distinct cases in the switch.
*
****************************************************************/

static void my_event_handler(arg)
struct event_handler_args arg;
{

struct input_pvar *pi;
char errmsg[100];

/* for debugging */
char buff[100];
char sevr_buff[20];
char print_line[500];

    if (pi = (struct input_pvar *) arg.usr)
    {
	if (pi->pvalue)
	{
/* printf("my_event_handler(): GOT dbr type %d nelements %ld EXP dbr type %d max %ld destaddr >%x<\n", arg.type, arg.count, pi->dest_old_dbr_sts_type, pi->dest_nelements, pi->pvalue); */
	    /* checking that channel access gave us what we asked for */
	    if (arg.type == pi->dest_old_dbr_sts_type)
	    {
		/* checking that we did not receive */
		/* more than we allocated for       */
		if (arg.count <= pi->dest_nelements)
		{
/* sprintf(print_line, "my_event_handler(): spvar >%s< --> dpvar >%s< ", pi->source_name, pi->dest_name); */
		    FASTLOCK(&(pi->lock));

		    switch (arg.type)
		    {
			case DBR_STS_STRING: 
			    pi->needs_reading = TRUE;
			    pi->last_rcvd_nelements = arg.count;

/* bcopy( */
/* (char *) ((struct dbr_sts_string *) arg.dbr)->value, */
/* (char *) pi->pvalue,  */
/* (arg.count * pi->dest_old_dbr_element_size)); */
			    memcpy(
				(char *) pi->pvalue, 
			    (char *) ((struct dbr_sts_string *) arg.dbr)->value,
				(arg.count * pi->dest_old_dbr_element_size));
			    pi->source_severity = 
		(unsigned short) ((struct dbr_sts_string *) arg.dbr)->severity;

/* strcat(print_line, "DBR_STS_STRING "); */
/* sprintf(buff, ">%s< ",  */
/* (char *) ((struct dbr_sts_string *) arg.dbr)->value); */
/* strcat(print_line, buff); */
			    break;
			/* case DBR_STS_INT:  */
			case DBR_STS_SHORT: 
			    pi->needs_reading = TRUE;
			    pi->last_rcvd_nelements = arg.count;

/* bcopy( */
/* (char *) &(((struct dbr_sts_short *) arg.dbr)->value), */
/* (char *) pi->pvalue,  */
/* (arg.count * pi->dest_old_dbr_element_size)); */
			    memcpy(
				(char *) pi->pvalue, 
			(char *) &(((struct dbr_sts_short *) arg.dbr)->value),
				(arg.count * pi->dest_old_dbr_element_size));
			    pi->source_severity = 
		(unsigned short) ((struct dbr_sts_short *) arg.dbr)->severity;

/* strcat(print_line, "DBR_STS_SHORT "); */
/* sprintf(buff, ">%d< ",  */
/* (short) ((struct dbr_sts_short *) arg.dbr)->value); */
/* strcat(print_line, buff); */
			    break;
			case DBR_STS_FLOAT:
			    pi->needs_reading = TRUE;
			    pi->last_rcvd_nelements = arg.count;

/* bcopy( */
/* (char *) &(((struct dbr_sts_float *) arg.dbr)->value), */
/* (char *) pi->pvalue,  */
/* (arg.count * pi->dest_old_dbr_element_size)); */
			    memcpy(
				(char *) pi->pvalue, 
			(char *) &(((struct dbr_sts_float *) arg.dbr)->value),
				(arg.count * pi->dest_old_dbr_element_size));
			    pi->source_severity = 
		(unsigned short) ((struct dbr_sts_float *) arg.dbr)->severity;

/* strcat(print_line, "DBR_STS_FLOAT "); */
/* sprintf(buff, ">%f< ",  */
/* (float) ((struct dbr_sts_float *) arg.dbr)->value); */
/* strcat(print_line, buff); */
			    break;
			case DBR_STS_ENUM:
			    pi->needs_reading = TRUE;
			    pi->last_rcvd_nelements = arg.count;

/* bcopy( */
/* (char *) &(((struct dbr_sts_enum *) arg.dbr)->value), */
/* (char *) pi->pvalue,  */
/* (arg.count * pi->dest_old_dbr_element_size)); */
			    memcpy(
				(char *) pi->pvalue, 
			(char *) &(((struct dbr_sts_enum *) arg.dbr)->value),
				(arg.count * pi->dest_old_dbr_element_size));
			    pi->source_severity = 
		(unsigned short) ((struct dbr_sts_enum *) arg.dbr)->severity;

/* strcat(print_line, "DBR_STS_SHORT "); */
/* sprintf(buff, ">%d< ",  */
/* (short) ((struct dbr_sts_enum *) arg.dbr)->value); */
/* strcat(print_line, buff); */
			    break;
			case DBR_STS_CHAR:
			    pi->needs_reading = TRUE;
			    pi->last_rcvd_nelements = arg.count;

/* bcopy( */
/* (char *) &(((struct dbr_sts_char *) arg.dbr)->value), */
/* (char *) pi->pvalue,  */
/* (arg.count * pi->dest_old_dbr_element_size)); */
			    memcpy(
				(char *) pi->pvalue, 
			(char *) &(((struct dbr_sts_char *) arg.dbr)->value),
				(arg.count * pi->dest_old_dbr_element_size));
			    pi->source_severity = 
		(unsigned short) ((struct dbr_sts_char *) arg.dbr)->severity;

/* strcat(print_line, "DBR_STS_CHAR "); */
/* sprintf(buff, ">%c< ",  */
/* (unsigned char) ((struct dbr_sts_char *) arg.dbr)->value); */
			break;
			case DBR_STS_LONG:
			    pi->needs_reading = TRUE;
			    pi->last_rcvd_nelements = arg.count;

/* bcopy( */
/* (char *) &(((struct dbr_sts_long *) arg.dbr)->value), */
/* (char *) pi->pvalue,  */
/* (arg.count * pi->dest_old_dbr_element_size)); */
			    memcpy(
				(char *) pi->pvalue, 
			(char *) &(((struct dbr_sts_long *) arg.dbr)->value),
				(arg.count * pi->dest_old_dbr_element_size));
			    pi->source_severity = 
		(unsigned short) ((struct dbr_sts_long *) arg.dbr)->severity;

/* strcat(print_line, "DBR_STS_LONG "); */
/* sprintf(buff, ">%ld< ",  */
/* (long) ((struct dbr_sts_long *) arg.dbr)->value); */
			break;
			case DBR_STS_DOUBLE:
			    pi->needs_reading = TRUE;
			    pi->last_rcvd_nelements = arg.count;

/* bcopy( */
/* (char *) &(((struct dbr_sts_double *) arg.dbr)->value), */
/* (char *) pi->pvalue,  */
/* (arg.count * pi->dest_old_dbr_element_size)); */
			    memcpy(
				(char *) pi->pvalue, 
			(char *) &(((struct dbr_sts_double *) arg.dbr)->value),
				(arg.count * pi->dest_old_dbr_element_size));
			    pi->source_severity = 
		(unsigned short) ((struct dbr_sts_double *) arg.dbr)->severity;

/* strcat(print_line, "DBR_STS_DOUBLE "); */
/* sprintf(buff, ">%f< ",  */
    /* (double) ((struct dbr_sts_double *) arg.dbr)->value); */
/* strcat(print_line, buff); */
			    break;
			default:
			    strcat(print_line, "UNKNOWN ");
			    strcat(print_line, "**** ");
			    break;
		    } /* end switch() */

		    FASTUNLOCK(&(pi->lock));

/* severity_to_string(pi->source_severity, sevr_buff); */
/* sprintf(buff, "SEVR >%s< to dest >%s<", sevr_buff, pi->dest_name); */
/* strcat(print_line, buff); */
/* printf("%s\n", print_line); */
		}
		else
		{
		    sprintf(errmsg, 
    "ERROR: my_event_handler() ca delivered %ld elements while maximum is %ld",
		arg.count, pi->dest_nelements);

		    errMessage(S_dbCa_cafailure, errmsg);
		} /* endif */
	    }
	    else
	    {
		sprintf(errmsg, 
	"ERROR: my_event_handler() ca delivered DBR %d while %d was expected",
		    arg.type, pi->dest_old_dbr_sts_type);

		errMessage(S_dbCa_cafailure, errmsg);
	    } /* endif */

	} 
	else
	    errMessage(S_dbCa_foundnull, 
		"ERROR: my_event_handler() found NULL pi->pvalue");
    }
    else
	errMessage(S_dbCa_foundnull, "ERROR: my_event_handler() found NULL pi");

} /* end my_event_handler() */

/****************************************************************
*
* static struct input_pvar *pop_input_pvar()
*
* Description:
*     Pops a fresh input pvar node off the avail list.
*
* Input:  None.
*
* Output: None.
*
* Returns:
*     address of fresh node
*     NULL if unable to allocate a node
*
* Notes:
*     The LOCKs associated with the nodes are only initialized immediately
* after acquiring the memory for the node while the rest of the fields in 
* the structure are initialized every time the node is popped off the list.
*
*     It is also assumed that -1 is an invalid dest_old_dbr_sts_type as well
* as an invalid record severity.
*
****************************************************************/

static struct input_pvar *pop_input_pvar()
{

struct input_pvar *rc;
int i;

    if (Input_pvar_avail_hdr != (struct input_pvar *) NULL)
    {
        rc = Input_pvar_avail_hdr;
        Input_pvar_avail_hdr = rc->next;
    }
    else
    {
        if ((Input_pvar_avail_hdr 
	    = (struct input_pvar *) calloc 
		((unsigned) NODESPERCALLOC, 
		(unsigned) (sizeof (struct input_pvar)))) 
		    != (struct input_pvar *) NULL)
        {
            for (rc = Input_pvar_avail_hdr, i=0; i < (NODESPERCALLOC-1); i ++)
            {
		FASTLOCKINIT(&(rc->lock));
                rc->next = rc + 1;
                rc++;
            } /* endfor */
	    FASTLOCKINIT(&(rc->lock));
            rc->next = (struct input_pvar *) NULL;

            rc = Input_pvar_avail_hdr;
            Input_pvar_avail_hdr = rc->next;
        }
        else
            rc = (struct input_pvar *) NULL;
    } /* endif */

    if (rc)
    {
	rc->next = (struct input_pvar *) NULL;
	rc->pdest_dbaddr = (void *) NULL;
	rc->source_name[0] = '\0';
	rc->dest_name[0] = '\0';
	rc->pvalue = (void *) NULL;
	rc->maximize_severity = FALSE;
	rc->dest_old_dbr_element_size = 0;
	rc->dest_old_dbr_type = (short) -1; /* hopefully -1 is not    */
					    /* valid old_dbr_type */
	rc->revised_dest_new_dbr_type = (short) -1; /* hopefully -1 is not  */
						/* valid new_dbr_type    */
	rc->dest_old_dbr_sts_type = (short) -1; /* hopefully -1 is not    */
					        /* valid old_dbr_sts_type */
	rc->source_severity = (unsigned short) -1; /* hopefully -1 is not a */
						   /* valid status/severity */
	rc->needs_reading = FALSE;
	rc->dest_nelements = (long) -1;
	rc->last_rcvd_nelements = (long) -1;
    } /* endif */

    return rc;

} /* end pop_input_pvar() */

/****************************************************************
*
* static struct output_pvar *pop_output_pvar()
*
* Description:
*     Pops a fresh output pvar node off the avail list.
*
* Input:  None.
*
* Output: None.
*
* Returns:
*     address of fresh node
*     NULL if unable to allocate a node
*
* Notes:
*     The LOCKs associated with the nodes are only initialized immediately
* after acquiring the memory for the node while the rest of the fields in 
* the structure are initialized every time the node is popped off the list.
*
*     It is also assumed that -1 is an invalid source_old_dbr_type.
*
****************************************************************/

static struct output_pvar *pop_output_pvar()
{

struct output_pvar *rc;
int i;

    if (Output_pvar_avail_hdr != (struct output_pvar *) NULL)
    {
        rc = Output_pvar_avail_hdr;
        Output_pvar_avail_hdr = rc->next;
    }
    else
    {
        if ((Output_pvar_avail_hdr = 
	    (struct output_pvar *) calloc 
		((unsigned) NODESPERCALLOC,
		(unsigned) (sizeof (struct output_pvar)))) 
		    != (struct output_pvar *) NULL)
        {
            for (rc = Output_pvar_avail_hdr, i=0; i < (NODESPERCALLOC-1); i ++)
            {
		FASTLOCKINIT(&(rc->lock));
                rc->next = rc + 1;
                rc++;
            } /* endfor */
	    FASTLOCKINIT(&(rc->lock));
            rc->next = (struct output_pvar *) NULL;

            rc = Output_pvar_avail_hdr;
            Output_pvar_avail_hdr = rc->next;
        }
        else
            rc = (struct output_pvar *) NULL;
    } /* endif */

    if (rc)
    {
        rc->next = (struct output_pvar *) NULL;
        rc->write_next = (struct output_pvar *) NULL;
	rc->psource_dbaddr = (void *) NULL;
	rc->pvalue = (void *) NULL;
	rc->dest_name[0] = '\0';
	rc->source_name[0] = '\0';
	rc->source_old_dbr_type = (short) -1; /* hope -1 invalid dbr_type */
	rc->source_new_dbr_type = (short) -1; /* hope -1 invalid dbr_type */
	rc->revised_source_new_dbr_type = (short) -1; /* hope -1 invalid */
						      /* dbr_type */
	rc->source_nelements = -1L;
	rc->last_nelements_written = -1L;
	rc->valid_value = FALSE;
	rc->on_writelist = FALSE;
    } /* endif */

    return rc;

} /* end pop_output_pvar() */

/****************************************************************
*
* static long process_asynch_events(seconds)
* float seconds;
*
* Description:
*     Queues a waits for events on remote monitors by calling ca_pend_event().
*
* Input:
*     float seconds   number of seconds to wait for events (0 means forever)
*
* Output: None.
*
* Returns:
*     0                     - Success
*     S_dbCa_ECA_EVDISALLOW - attempted to execute from within an event handler
*     S_dbCa_unknownECA     - unrecognizable status rc from ca_pend_event()
*
* Notes:
*     If seconds = 0 then no status is ever returned.
*
****************************************************************/

static long process_asynch_events(seconds)
float seconds;
{

int status;
char errmsg[100];
long rc;

    status = ca_pend_event(seconds);

    /* from here down won't get executed if     */
    /* ca_pend_event(0.0) i.e., waiting forever */

    switch (status)
    {
	case ECA_NORMAL:     
	    rc = 0L;
	    break;
	case ECA_TIMEOUT:    
	    rc = 0L;
	    break;
	case ECA_EVDISALLOW: 
	    rc = S_dbCa_ECA_EVDISALLOW;
	    errMessage(S_dbCa_ECA_EVDISALLOW, 
		"ERROR: process_asynch_events() cannot call inside evt hndlr");
	    break;
	default: 
	    rc = S_dbCa_unknownECA;
	    sprintf(errmsg, 
	    "ERROR: process_asynch_events() unrecognizable status returned %d", 
		status);     
	    errMessage(S_dbCa_unknownECA, errmsg);
	    break;
    } /* end switch() */

    return rc;

} /* end process_asynch_events() */

/****************************************************************
*
* static void process_asynch_events_task(parent_task_id)
* int parent_task_id;
*
* Description:
*     issues a single call to process_asynch_events(0.0).  this is a trick
*     suggested by Jeff Hill so that connections that are lost may be
*     re-connected when the other IOC is re-booted.  without this trick,
*     a lost connection to another IOC will remain lost, even if the other
*     IOC successfully re-boots.
*
* Input:
*     int parent_task_id   vxWorks taskid of dbCaProcessOutlinks
*
* Output: None.
*
* Returns:
*
* Notes:
*
****************************************************************/

static void process_asynch_events_task(parent_task_id)
int parent_task_id;
{

    ca_import(parent_task_id);

    /* kludge suggested by Jeff Hill so that we can have reliable */
    /* connection management for  vxWorks hosted CA clients       */
    process_asynch_events((float) 0.0);

} /* end process_asynch_events_task() */

/****************************************************************
*
* static void push_input_pvar(ip)
* struct input_pvar *ip;
*
* Description:
*     Pushes input pvar back onto avail stack.
*
* Input:
*     struct input_pvar *ip   node to be pushed
*
* Output:  None.
*
* Returns: None.
*
* Notes:   None.
*
****************************************************************/

static void push_input_pvar(ip)
struct input_pvar *ip;
{
    if (ip)
    {
	ip->next = Input_pvar_avail_hdr;
	Input_pvar_avail_hdr = ip;
    } /* endif */

} /* end push_input_pvar() */

/****************************************************************
*
* static void push_output_pvar(op)
* struct output_pvar *op;
*
* Description:
*     Pushes output pvar back onto avail stack.
*
* Input:
*     struct output_pvar *op   node to be pushed
*
* Output:  None.
*
* Returns: None.
*
* Notes:   None.
*
****************************************************************/

static void push_output_pvar(op)
struct output_pvar *op;
{
    if (op)
    {
        op->next = Output_pvar_avail_hdr;
        Output_pvar_avail_hdr = op;
    } /* endif */

} /* end push_output_pvar() */

/****************************************************************
*
* static long queue_add_event_from_input_pvar(pi)
* struct input_pvar *pi;
*
* Description:
*     Queues a monitor on a remote source process input variable and registers 
* my_event_handler() as the event hanlder by calling ca_add_event().
*
* Input:
*     struct input_pvar *pi     pointer to input pvar structure to estab monitor
*
* Output: None.
*
* Returns:
*     0                   - Success
*     S_dbCa_nullarg      - got NULL pi
*     S_dbCa_ECA_BADCHID  - ECA_BADCHID
*     S_dbCa_ECA_BADTYPE  - ECA_BADTYPE
*     S_dbCa_ECA_ALLOCMEM - ECA_ALLOCMEM
*     S_dbCa_ECA_ADDFAIL  - ECA_ADDFAIL
*     S_dbCa_unknownECA   - unknown rc from ca_add_event()
*
* Notes: None.
*
****************************************************************/

static long queue_add_event_from_input_pvar(pi)
struct input_pvar *pi;
{

int status;
char errmsg[100];
long rc;

    if (pi)
    {

	status = ca_add_event((chtype) pi->dest_old_dbr_sts_type,
		    pi->cid, my_event_handler, pi, (evid *) NULL);

	switch (status)
	{
	    case ECA_NORMAL: 
		rc = 0L;
		break;
	    case ECA_BADCHID:  
		rc = S_dbCa_ECA_BADCHID;
		errMessage(S_dbCa_ECA_BADCHID,
	"ERROR: queue_add_event_from_input_pvar() unconnected/corrupted CHID"); 
		break;
	    case ECA_BADTYPE:  
		rc = S_dbCa_ECA_BADTYPE;
		errMessage(S_dbCa_ECA_BADTYPE,
		"ERROR: queue_add_event_from_input_pvar() unknown GET_TYPE");
		break;
	    case ECA_ALLOCMEM: 
		rc = S_dbCa_ECA_ALLOCMEM;
		errMessage(S_dbCa_ECA_ALLOCMEM,
		"ERROR: queue_add_event_from_input_pvar() unable to alloc mem");
		break;
	    case ECA_ADDFAIL:  
		rc = S_dbCa_ECA_ADDFAIL;
		errMessage(S_dbCa_ECA_ADDFAIL,
	"ERROR: queue_add_event_from_input_pvar() local db event add failed");
		break;
	    default: 
		rc = S_dbCa_unknownECA;
		sprintf(errmsg, 
"ERROR: queue_add_event_from_input_pvar() unrecognizable status returned %d",
		    status);     
		errMessage(S_dbCa_unknownECA, errmsg);
	} /* end switch() */

    }
    else
    {
	rc = S_dbCa_nullarg;
	errMessage(S_dbCa_nullarg,
	    "ERROR: queue_add_event_from_input_pvar() got NULL pi");
    } /* endif */

    return rc;

} /* end queue_add_event_from_input_pvar() */

/****************************************************************
*
* static long queue_ca_build_and_connect(po)
* struct output_pvar *po;
*
* Description:
*     Queues a search and establishes connections to remote destination pvars 
* for output links and registers my_connection_handler() as the connection
* handler routine by calling ca_build_and_connect().
*
* Input:
*     struct output_pvar *po   pointer to output node that needs connection
*
* Output: None.
*
* Returns:
*     0                   - Success
*     S_dbCa_nullarg      - got NULL po
*     S_dbCa_ECA_BADTYPE  - ECA_BADTYPE
*     S_dbCa_ECA_STRTOBIG - ECA_STRTOBIG
*     S_dbCa_ECA_ALLOCMEM - ECA_ALLOCMEM
*     S_dbCa_ECA_GETFAIL  - ECA_GETFAIL
*     S_dbCa_unknownECA   - unknown rc from ca_build_and_connect()
*
* Notes:  None.
*
****************************************************************/

static long queue_ca_build_and_connect(po)
struct output_pvar *po;
{

int status;
char errmsg[100];
long rc;

    if (po)
    {
	/* pointing channel to this output_pvar struct */
	/* for later use by my_connection_handler()    */

	status = ca_build_and_connect(po->dest_name, (chtype) TYPENOTCONN,
		    (unsigned long) 0, &(po->cid), (void *) NULL, 
		    my_connection_handler, (void *) po);

	switch (status)
	{
	    case ECA_NORMAL:   
		rc = 0L;
		break;
	    case ECA_BADTYPE:  
		rc = S_dbCa_ECA_BADTYPE;
		errMessage(S_dbCa_ECA_BADTYPE,
		    "ERROR: queue_ca_build_and_connect() unknown GET_TYPE"); 
		break;
	    case ECA_STRTOBIG: 
		rc = S_dbCa_ECA_STRTOBIG;
		errMessage(S_dbCa_ECA_STRTOBIG,
		    "ERROR: queue_ca_build_and_connect() string too big");   
		break;
	    case ECA_ALLOCMEM: 
		rc = S_dbCa_ECA_ALLOCMEM;
		errMessage(S_dbCa_ECA_ALLOCMEM,
	    "ERROR: queue_ca_build_and_connect() unable to allocate memory");   
		break;
	    case ECA_GETFAIL:  
		rc = S_dbCa_ECA_GETFAIL;
		errMessage(S_dbCa_ECA_GETFAIL,
	    "ERROR: queue_ca_build_and_connect() a local database get failed"); 
		break;
	    default: 
		rc = S_dbCa_unknownECA;
		sprintf(errmsg, 
	"ERROR: queue_ca_build_and_connect() unrecognizable status returned %d",
		    status);     
		errMessage(S_dbCa_unknownECA, errmsg);
		break;
	} /* end switch() */
    }
    else
    {
	rc = S_dbCa_nullarg;
	errMessage(S_dbCa_nullarg,
	    "ERROR: queue_ca_build_and_connect() got NULL po");
    } /* endif */

    return rc;

} /* end queue_ca_build_and_connect() */

/****************************************************************
*
* Description:
*     Queues the writing of the value from the temporary store of an output
* node by calling ca_put().
*
* Input:
*     struct output_pvar *po   pointer to the node that has the output value	
*
* Output: None.
*
* Returns:
*     0                   - Success
*     S_dbCa_nullarg      - got NULL po
*     S_dbCa_ECA_BADCHID  - ECA_BADCHID
*     S_dbCa_ECA_BADTYPE  - ECA_BADTYPE
*     S_dbCa_ECA_BADCOUNT - ECA_BADCOUNT
*     S_dbCa_ECA_STRTOBIG - ECA_STRTOBIG
*     S_dbCa_ECA_PUTFAIL  - ECA_PUTFAIL
*     S_dbCa_unknownECA   - unknown rc from ca_put()
*
* Notes:  None.
*
****************************************************************/

static long queue_ca_array_put(po)
struct output_pvar *po;
{

int status;
char errmsg[100];
long rc;

    if (po)
    {
	status = ca_array_put(po->source_old_dbr_type, 
	    (unsigned long) po->last_nelements_written, po->cid, po->pvalue);

	switch (status)
	{
	    case ECA_NORMAL: 
		rc = 0L;
		break;
	    case ECA_BADCHID: 
		rc = S_dbCa_ECA_BADCHID;
		errMessage(S_dbCa_ECA_BADCHID,
		    "ERROR: queue_ca_put() unconnected/corrupted CHID");  
		break;
	    case ECA_BADTYPE: 
		rc = S_dbCa_ECA_BADTYPE;
		errMessage(S_dbCa_ECA_BADTYPE,
		    "ERROR: queue_ca_put() unknown GET_TYPE");            
		break;
	    case ECA_BADCOUNT: 
		rc = S_dbCa_ECA_BADCOUNT;
		errMessage(S_dbCa_ECA_BADCOUNT,
"ERROR: queue_ca_put() requested count larger than actual chnl elmnt count");  
		break;
	    case ECA_STRTOBIG: 
		rc = S_dbCa_ECA_STRTOBIG;
		errMessage(S_dbCa_ECA_STRTOBIG,
		    "ERROR: queue_ca_put() unusually large string supplied");
		break;
	    case ECA_PUTFAIL: 
		rc = S_dbCa_ECA_PUTFAIL;
		errMessage(S_dbCa_ECA_PUTFAIL,
		    "ERROR: queue_ca_put() a local database put failed"); 
		break;
	    default: 
		rc = S_dbCa_unknownECA;
		sprintf(errmsg, 
		    "ERROR: queue_ca_put() unrecognizable status returned %d", 
		    status);     
		errMessage(S_dbCa_unknownECA, errmsg);
	} /* end switch() */
    }
    else
    {
	rc = S_dbCa_nullarg;
	errMessage(S_dbCa_nullarg, "ERROR: queue_ca_put() got NULL po");
    } /* endif */

    return rc;

} /* end queue_ca_array_put() */

/****************************************************************
*
* static long queue_ca_search(name, pcid)
* char *name;
* chid *pcid;
*
* Description:
*     Queues a ca_search().
*
* Input:
*     char *name     name of remote pvar
*     chid *pcid     pointer to channel
*
* Output: None.
*
* Returns:
*     0 - Success
*     S_dbCa_nullarg - got NULL name or NULL pcid
*     S_dbCa_ECA_BADTYPE - ECA_BADTYPE
*     S_dbCa_ECA_STRTOBIG - ECA_STRTOBIG
*     S_dbCa_ECA_ALLOCMEM - ECA_ALLOCMEM
*     S_dbCa_ECA_GETFAIL - ECA_GETFAIL
*     S_dbCa_unknownECA - unknown rc from ca_search()
*
* Notes:  None.
*
****************************************************************/

static long queue_ca_search(name, pcid)
char *name;
chid *pcid;
{

int status;
char errmsg[100];
long rc;

    if (name)
    {
	if (pcid)
	{
	    status = ca_search(name, pcid);

	    switch (status)
	    {
		case ECA_NORMAL:   
		    rc = 0L;
		    break;
		case ECA_BADTYPE:  
		    rc = S_dbCa_ECA_BADTYPE;
		    errMessage(S_dbCa_ECA_BADTYPE,
			"ERROR: queue_ca_search() unknown GET_TYPE"); 
		    break;
		case ECA_STRTOBIG: 
		    rc = S_dbCa_ECA_STRTOBIG;
		    errMessage(S_dbCa_ECA_STRTOBIG,
			"ERROR: queue_ca_search() string too big");   
		    break;
		case ECA_ALLOCMEM: 
		    rc = S_dbCa_ECA_ALLOCMEM;
		    errMessage(S_dbCa_ECA_ALLOCMEM,
			"ERROR: queue_ca_search() unable to allocate memory");
		    break;
		case ECA_GETFAIL:  
		    rc = S_dbCa_ECA_GETFAIL;
		    errMessage(S_dbCa_ECA_GETFAIL,
			"ERROR: queue_ca_search() a local database get failed");
		    break;
		default: 
		    rc = S_dbCa_unknownECA;
		    sprintf(errmsg, 
		"ERROR: queue_ca_search() unrecognizable status returned %d",
			status);     
		    errMessage(S_dbCa_unknownECA, errmsg);
		    break;
	    } /* end switch() */
	}
	else
	{
	    rc = S_dbCa_nullarg;
	    errMessage(S_dbCa_nullarg, 
		"ERROR: queue_ca_search() got NULL pcid");
	} /* endif */
    }
    else
    {
	rc = S_dbCa_nullarg;
	errMessage(S_dbCa_nullarg, "ERROR: queue_ca_search() got NULL name");
    } /* endif */

    return rc;

} /* end queue_ca_search() */

/****************************************************************
*
* static long task_initialize_channel_access()
*
* Description:
*     Initializes channel access for calling task by calling 
* ca_task_initialize().  
*
* Input:  None.
*
* Output: None.
*
* Returns:
*     0 - Success
*     S_dbCa_ECA_ALLOCMEM - ECA_ALLOCMEM
*     S_dbCa_unknownECA - unknown rc from ca_task_initialize()
*
* Notes:  
*     Must be called exactly once by each task making calls to channel access.
*
****************************************************************/

static long task_initialize_channel_access()
{

int status;
char errmsg[100];
long rc;

    status = ca_task_initialize();

    switch (status)
    {
	case ECA_NORMAL:   
	    rc = 0L;
	    break;
	case ECA_ALLOCMEM: 
	    rc = S_dbCa_ECA_ALLOCMEM;
		    errMessage(S_dbCa_ECA_ALLOCMEM,
	"ERROR: task_initialize_channel_access() unable to allocate memory");
	    break;
	default: 
	    rc = S_dbCa_unknownECA;
	    sprintf(errmsg, 
    "ERROR: task_initialize_channel_access() unrecognizable status returned %d",
		status);     
	    errMessage(S_dbCa_unknownECA, errmsg);
	    break;
    } /* end switch() */

    return rc;

} /* end task_initialize_channel_access() */

/****************************************************************
*
* FOR DEBUGGING
*
****************************************************************/

static void severity_to_string(sevr, buff)
unsigned short sevr;
char *buff;
{
    
    if (buff)
    {
	switch (sevr)
	{
	    case NO_ALARM:    strcpy(buff, "NO_ALARM");          break;
	    case MINOR_ALARM: strcpy(buff, "MINOR_ALARM");       break;
	    case MAJOR_ALARM: strcpy(buff, "MAJOR_ALARM");       break;
	    case VALID_ALARM: strcpy(buff, "VALID_ALARM");       break;
	    default:          sprintf(buff, "UNKNOWN %d", sevr); break;
	} /* end switch() */
    } /* endif */

} /* end severity_to_string() */

static void stat_to_string(stat, buff)
unsigned short stat;
char *buff;
{

    if (buff)
    {
	switch (stat)
	{
	    case READ_ALARM:     strcpy(buff, "READ_ALARM"); break;
	    case WRITE_ALARM:    strcpy(buff, "WRITE_ALARM"); break;
	    case HIHI_ALARM:     strcpy(buff, "HIHI_ALARM"); break;
	    case HIGH_ALARM:     strcpy(buff, "HIGH_ALARM"); break;
	    case LOLO_ALARM:     strcpy(buff, "LOLO_ALARM"); break;
	    case LOW_ALARM:      strcpy(buff, "LOW_ALARM"); break;
	    case STATE_ALARM:    strcpy(buff, "STATE_ALARM"); break;
	    case COS_ALARM:      strcpy(buff, "COS_ALARM"); break;
	    case COMM_ALARM:     strcpy(buff, "COMM_ALARM"); break;
	    case TIMEOUT_ALARM:  strcpy(buff, "TIMEOUT_ALARM"); break;
	    case HW_LIMIT_ALARM: strcpy(buff, "HW_LIMIT_ALARM"); break;
	    case CALC_ALARM:     strcpy(buff, "CALC_ALARM"); break;
	    case SCAN_ALARM:     strcpy(buff, "SCAN_ALARM"); break;
	    case LINK_ALARM:     strcpy(buff, "LINK_ALARM"); break;
	    case SOFT_ALARM:     strcpy(buff, "SOFT_ALARM"); break;
	    case BAD_SUB_ALARM:  strcpy(buff, "BAD_SUB_ALARM"); break;
	    case UDF_ALARM:      strcpy(buff, "UDF_ALARM"); break;
	    default:             sprintf(buff, "UNKNOWN %d", stat); break;
	} /* end switch() */
    } /* endif */

} /* end stat_to_string() */

