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

#include <epicsAssert.h>
#include <cadef.h>

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

#define ITERATION_COUNT 1000

#define WAIT_FOR_ACK

typedef struct testItem {
	chid 			chix;
	char 			name[40];
	int 			type;
	int			count;
	union db_access_val	val;	
}ti;

ti itemList[ITERATION_COUNT];

int catime (char *channelName);

typedef void tf (ti *pItems, unsigned iterations, unsigned *pInlineIter);

LOCAL void test (
	ti		*pItems,
	unsigned	iterations
);

LOCAL tf 	test_pend;
LOCAL tf 	test_search;
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

	if(argc == 2){
		pname = argv[1];	
		catime(pname);
	}
	else{
		printf("usage: %s <channel name>\n", argv[0]);
		return -1;
	}
	return 0;
}
#endif


/*
 * catime ()
 */
int catime (char *channelName)
{
  	long		i;
	unsigned 	strsize;

  	SEVCHK (ca_task_initialize(),"Unable to initialize");

	strsize = sizeof(itemList[i].name)-1;
	for (i=0; i<NELEMENTS(itemList); i++) {
		strncpy (
			itemList[i].name, 
			channelName, 
			strsize);
		itemList[i].name[strsize]= '\0';
		itemList[i].count = 1;
	}

	printf ("search test\n");
	timeIt (test_search, itemList, NELEMENTS(itemList));

  	printf (
		"channel name=%s, native type=%d, native count=%d\n",
		ca_name(itemList[0].chix),
		ca_field_type(itemList[0].chix),
		ca_element_count(itemList[0].chix));

	printf ("\tpend event test\n");
  	timeIt (test_pend, NULL, 100);

  	for (i=0; i<NELEMENTS(itemList); i++) {
		itemList[i].val.fltval = 0.0;
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
	printf ("interger test\n");
  	test (itemList, NELEMENTS(itemList));

  	printf ("free test\n");
	timeIt (test_free, itemList, NELEMENTS(itemList));

	SEVCHK (ca_task_exit (), "Unable to free resources at exit");

  	return OK;
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
  	timeIt (test_wait, pItems, iterations);
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
	printf ("Elapsed Per Item = %f\n", delay/(iterations*inlineIter));
}


/*
 * test_search ()
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
	ti	*pi;
	int	status;

	for (pi=pItems; pi<&pItems[iterations]; pi++) {
		status = ca_search (
				pi->name, 
				&pi->chix);
		SEVCHK (status, NULL);
  	}
	status = ca_pend_io(0.0);
    	SEVCHK (status, NULL);

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
	ti		*pi;
	int		status;

	for (pi=pItems; pi<&pItems[iterations]; pi++) {
		status = ca_clear_channel (pi->chix);
		SEVCHK (status, NULL);
	}
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

