/*
 *
 *	CA performance test 
 *
 *	History 
 *	joh	09-12-89 Initial release
 *	joh	12-20-94 portability
 *
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <limits.h>

#include "epicsAssert.h"
#include "cadef.h"
#include "caProto.h"

#ifndef LOCAL
#define LOCAL static
#endif

#ifndef OK
#define OK 0
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef min
#define min(A,B) ((A)>(B)?(B):(A))
#endif

#ifndef NELEMENTS 
#define NELEMENTS(A) (sizeof (A) / sizeof ((A) [0]))
#endif

#ifdef vxWorks
#define ITERATION_COUNT 1000
#else
#define ITERATION_COUNT 10000
#endif

#define WAIT_FOR_ACK

typedef struct testItem {
	chid 			chix;
	char 			name[40];
	int 			type;
	int			count;
	union db_access_val	val;	
}ti;

ti itemList[ITERATION_COUNT];

enum appendNumberFlag {appendNumber, dontAppendNumber};

int catime (char *channelName, enum appendNumberFlag appNF);

typedef void tf (ti *pItems, unsigned iterations, unsigned *pInlineIter);

LOCAL void test (
	ti		*pItems,
	unsigned	iterations
);

LOCAL void printSearchStat(unsigned iterations);

LOCAL tf 	test_pend;
LOCAL tf 	test_search;
LOCAL tf	test_sync_search;
LOCAL tf	test_free;
LOCAL tf	test_wait;
LOCAL tf	test_put;
LOCAL tf	test_wait;
LOCAL tf	test_get;

void timeIt(
	tf		*pfunc,
	ti		*pItem,
	unsigned	iterations
);

#ifndef vxWorks
int main(int argc, char **argv)
{
	char	*pname;

	if(argc <= 1 || argc>3){
printf("usage: %s <channel name> [<if 2nd arg present append number to pv name>]\n", 
	argv[0]);
		return -1;
	}
	else{
		pname = argv[1];	
		if (argc==3) {
			catime(pname, appendNumber);
		}
		else {
			catime(pname, dontAppendNumber);
		}
	}
	return 0;
}
#endif


/*
 * catime ()
 */
int catime (char *channelName, enum appendNumberFlag appNF)
{
  	long		i;
	unsigned 	strsize;

  	SEVCHK (ca_task_initialize(),"Unable to initialize");

	if (appNF==appendNumber) {
		printf("Testing with %lu channels named %snnn\n", 
			(unsigned long) NELEMENTS(itemList), channelName);
	}
	else {
		printf("Testing with %lu channels named %s\n", 
			 (unsigned long) NELEMENTS(itemList), channelName);
	}

	strsize = sizeof(itemList[i].name)-1;
	for (i=0; i<NELEMENTS(itemList); i++) {
		if (appNF==appendNumber) {
			sprintf(itemList[i].name,"%.*s%lu",
				(int) (strsize - 15u), channelName, i);
		}
		else {
			strncpy (
				itemList[i].name, 
				channelName, 
				strsize);
		}
		itemList[i].name[strsize]= '\0';
		itemList[i].count = 1;
	}

	printf ("search test\n");
	timeIt (test_search, itemList, NELEMENTS(itemList));
	printSearchStat(NELEMENTS(itemList));

  	printf (
		"channel name=%s, native type=%d, native count=%d\n",
		ca_name(itemList[0].chix),
		ca_field_type(itemList[0].chix),
		ca_element_count(itemList[0].chix));

	printf ("\tpend event test\n");
  	timeIt (test_pend, NULL, 100);

  	for (i=0; i<NELEMENTS(itemList); i++) {
		itemList[i].val.fltval = 0.0f;
		itemList[i].type = DBR_FLOAT; 
  	}
	printf ("float test\n");
  	test (itemList, NELEMENTS(itemList));

  	for (i=0; i<NELEMENTS(itemList); i++) {
		strcpy(itemList[i].val.strval, "0.0");
		itemList[i].type = DBR_STRING; 
	}
	printf ("string test\n");
  	test (itemList, NELEMENTS(itemList));

  	for (i=0; i<NELEMENTS(itemList); i++) {
		itemList[i].val.intval = 0;
		itemList[i].type = DBR_INT; 
	}
	printf ("integer test\n");
  	test (itemList, NELEMENTS(itemList));

  	printf ("free test\n");
	timeIt (test_free, itemList, NELEMENTS(itemList));

	SEVCHK (ca_task_exit (), "Unable to free resources at exit");

  	return OK;
}

/*
 * printSearchStat()
 */
LOCAL void printSearchStat(unsigned iterations)
{
	ti	*pi;
	double 	X = 0u;
	double 	XX = 0u; 
	double 	max = DBL_MIN; 
	double 	min = DBL_MAX; 
	double 	mean;
	double 	stdDev;

	for (pi=itemList; pi<&itemList[iterations]; pi++) {
		double retry = pi->chix->retry;
		X += retry;
		XX += retry*retry;
		if (retry>max) {
			max = retry;
		}
		if (retry<min) {
			min = retry;
		}
	}

	mean = X/iterations;
	stdDev = sqrt(XX/iterations - mean*mean);
	printf ("Search tries per chan - mean=%f std dev=%f min=%f max=%f\n",
		mean, stdDev, min, max);
}


/*
 * test ()
 */
LOCAL void test (
	ti		*pItems,
	unsigned	iterations
)
{
	printf ("\tasync put test\n");
  	timeIt (test_put, pItems, iterations);
	printf ("\tasync get test\n");
  	timeIt (test_get, pItems, iterations);
	printf ("\tsynch get test\n");
  	timeIt (test_wait, pItems, iterations/100);
}


/*
 * timeIt ()
 */
void timeIt(
	tf		*pfunc,
	ti		*pItems,
	unsigned	iterations
)
{
	TS_STAMP	end_time;
	TS_STAMP	start_time;
	double		delay;
	int		status;
	unsigned	inlineIter;
	unsigned	nBytes;

	status = tsLocalTime(&start_time);
#if 0
	assert (status == S_ts_OK);
#endif
	(*pfunc) (pItems, iterations, &inlineIter);
	status = tsLocalTime(&end_time);
#if 0
	assert (status == S_ts_OK);
#endif
	TsDiffAsDouble(&delay,&end_time,&start_time);
	if (delay>0.0) {
		printf ("Elapsed Per Item = %12.8f sec, %10.1f Items per sec", 
			delay/(iterations*inlineIter),
			(iterations*inlineIter)/delay);
		if (pItems!=NULL) {
			nBytes = sizeof(caHdr) + OCT_ROUND(dbr_size[pItems[0].type]);
			printf(", %3.1f Mbps\n", 
				(iterations*inlineIter*nBytes*CHAR_BIT)/(delay*1e6));
		}
		else {
			printf ("\n");
		}
	}
	else {
		printf ("Elapsed Per Item = %12.8f sec\n", 
			delay/(iterations*inlineIter));
	}
}


/*
 * test_pend()
 */
LOCAL void test_pend(
ti		*pItems,
unsigned	iterations,
unsigned	*pInlineIter
)
{
	unsigned 	i;
	int		status;

	for (i=0; i<iterations; i++) {
		status = ca_pend_event(1e-9);
		if (status != ECA_TIMEOUT && status != ECA_NORMAL) {
			SEVCHK(status, NULL);
		}
		status = ca_pend_event(1e-9);
		if (status != ECA_TIMEOUT && status != ECA_NORMAL) {
			SEVCHK(status, NULL);
		}
		status = ca_pend_event(1e-9);
		if (status != ECA_TIMEOUT && status != ECA_NORMAL) {
			SEVCHK(status, NULL);
		}
		status = ca_pend_event(1e-9);
		if (status != ECA_TIMEOUT && status != ECA_NORMAL) {
			SEVCHK(status, NULL);
		}
		status = ca_pend_event(1e-9);
		if (status != ECA_TIMEOUT && status != ECA_NORMAL) {
			SEVCHK(status, NULL);
		}
  	}
	*pInlineIter = 5;
}


/*
 * test_search ()
 */
LOCAL void test_search(
ti		*pItems,
unsigned	iterations,
unsigned	*pInlineIter
)
{
	unsigned i;
	int status;

	for (i=0u; i<iterations; i++) {
		status = ca_search (pItems[i].name, &pItems[i].chix);
		SEVCHK (status, NULL);
  	}
	status = ca_pend_io(0.0);
    	SEVCHK (status, NULL);

	*pInlineIter = 1;
}


/*
 * test_sync_search()
 */
LOCAL void test_sync_search(
ti		*pItems,
unsigned	iterations,
unsigned	*pInlineIter
)
{
	unsigned i;
	int status;

	for (i=0u; i<iterations; i++) {
		status = ca_search (pItems[i].name, &pItems[i].chix);
		SEVCHK (status, NULL);
		status = ca_pend_io(0.0);
		SEVCHK (status, NULL);
  	}

	*pInlineIter = 1;
}


/*
 * test_free ()
 */
LOCAL void test_free(
ti		*pItems,
unsigned	iterations,
unsigned	*pInlineIter
)
{
	int status;
	unsigned i;

	for (i=0u; i<iterations; i++) {
		status = ca_clear_channel (pItems[i].chix);
		SEVCHK (status, NULL);
	}
	status = ca_flush_io();
	SEVCHK (status, NULL);
	*pInlineIter = 1;
}


/*
 * test_put ()
 */
LOCAL void test_put(
ti		*pItems,
unsigned	iterations,
unsigned	*pInlineIter
)
{
	ti		*pi;
	int		status;
	dbr_int_t	val;
  
	for (pi=pItems; pi<&pItems[iterations]; pi++) {
		status = ca_array_put(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_put(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_put(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_put(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_put(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_put(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_put(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_put(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_put(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_put(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
  	}
#ifdef WAIT_FOR_ACK
	status = ca_array_get (DBR_INT, 1, pItems[0].chix, &val);
    	SEVCHK (status, NULL);
  	status = ca_pend_io(100.0);
#endif
	status = ca_array_put(
			pItems[0].type,
			pItems[0].count,
			pItems[0].chix,
			&pItems[0].val);
    	SEVCHK (status, NULL);
  	status = ca_flush_io();
    	SEVCHK (status, NULL);

	*pInlineIter = 10;
}


/*
 * test_get ()
 */
LOCAL void test_get(
ti		*pItems,
unsigned	iterations,
unsigned	*pInlineIter
)
{
	ti	*pi;
	int	status;
  
	for (pi=pItems; pi<&pItems[iterations]; pi++) {
		status = ca_array_get(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_get(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_get(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_get(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_get(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_get(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_get(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_get(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_get(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_array_get(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
  	}
  	status = ca_pend_io(100.0);
    	SEVCHK (status, NULL);

	*pInlineIter = 10;
}



/*
 * test_wait ()
 */
LOCAL void test_wait (
ti		*pItems,
unsigned	iterations,
unsigned	*pInlineIter
)
{
	ti	*pi;
	int	status;
  
	for (pi=pItems; pi<&pItems[iterations]; pi++) {
		status = ca_array_get(
				pi->type,
				pi->count,
				pi->chix,
				&pi->val);
    		SEVCHK (status, NULL);
		status = ca_pend_io(100.0);
		SEVCHK (status, NULL);
  	}

	*pInlineIter = 1;
}

