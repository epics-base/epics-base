
/*
 * CA test/debug routine
 */

static char *sccsId = "@(#) $Id$";

/*
 * $Log$
 * Revision 1.43  1997/01/22 21:07:27  jhill
 * fixed array test
 *
 * Revision 1.42  1996/12/12 18:51:41  jhill
 * doc
 *
 * Revision 1.41  1996/12/11 01:10:33  jhill
 * added additional vector tests
 *
 * Revision 1.40  1996/11/22 19:07:01  jhill
 * included string.h
 *
 * Revision 1.39  1996/11/02 00:50:36  jhill
 * many pc port, const in API, and other changes
 *
 * Revision 1.37  1996/09/16 16:31:01  jhill
 * fixed NT warnings
 *
 * Revision 1.36  1996/07/24 21:55:33  jhill
 * fixed gnu warnings
 *
 * Revision 1.35  1996/07/01 19:49:15  jhill
 * turned on analog value wrap-around test
 *
 * Revision 1.34  1996/06/19 17:59:02  jhill
 * many 3.13 beta changes
 *
 * Revision 1.33  1995/12/19  19:29:04  jhill
 * changed put test
 *
 * Revision 1.32  1995/11/29  19:17:25  jhill
 * more tests
 *
 * Revision 1.31  1995/10/12  01:30:28  jhill
 * improved the test
 *
 * Revision 1.30  1995/09/29  21:47:58  jhill
 * MS windows changes
 *
 * Revision 1.29  1995/08/22  00:16:34  jhill
 * Added test of the duration of ca_pend_event()
 *
 */

/*
 * ANSI
 */
#include	<stdio.h>
#include	<stdlib.h>
#include	<math.h>
#include	<float.h>
#include	<string.h>
#include	<assert.h>

/*
 * CA 
 */
#include 	"cadef.h"

#define EVENT_ROUTINE	null_event
#define CONN_ROUTINE	conn

#define NUM		1

int	conn_cb_count;

#ifndef min
#define min(A,B) ((A)>(B)?(B):(A))
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NELEMENTS
#define NELEMENTS(A) (sizeof(A)/sizeof(A[0]))
#endif

int	doacctst(char *pname);
void 	test_sync_groups(chid chix);
void 	multiple_sg_requests(chid chix, CA_SYNC_GID gid);
void 	null_event(struct event_handler_args args);
void 	write_event(struct event_handler_args args);
void 	conn(struct connection_handler_args args);
void 	get_cb(struct event_handler_args args);
void 	accessSecurity_cb(struct access_rights_handler_args args);

void doubleTest(
chid		chan,
dbr_double_t 	beginValue, 
dbr_double_t	increment,
dbr_double_t 	epsilon,
unsigned 	iterations);

void floatTest(
chid		chan,
dbr_float_t 	beginValue, 
dbr_float_t	increment,
dbr_float_t 	epsilon,
unsigned 	iterations);

#ifdef vxWorks
#include <vxWorks.h>
#include <taskLib.h>
int acctst(char *pname)
{

	return taskSpawn(
			"acctst", 
			200, 
			VX_FP_TASK, 
			20000, 
			doacctst,
			(int)pname,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
}
#else /* not vxWorks */
int main(int argc, char **argv)
{
	if(argc == 2){
		doacctst(argv[1]);
	}
	else{
		printf("usage: %s <chan name>\n", argv[0]);
	}
	return 0;
}
#endif /*vxWorks*/


int doacctst(char *pname)
{
	chid            chix1;
	chid            chix2;
	chid            chix3;
	chid            chix4;
	struct dbr_gr_float *ptr = NULL;
	struct dbr_gr_float *pgrfloat = NULL;
	dbr_float_t	*pfloat = NULL;
	dbr_double_t	*pdouble = NULL;
	long            status;
	long            i, j;
	evid            monix;
	char            pstring[NUM][MAX_STRING_SIZE];
	unsigned	size;
	unsigned	monCount=0u;

	SEVCHK(ca_task_initialize(), "Unable to initialize");

	conn_cb_count = 0;

	printf("begin\n");

	printf("CA Client V%s\n", ca_version());

	/*
	 * CA pend event delay accuracy test
	 */
	{
		TS_STAMP	end_time;
		TS_STAMP	start_time;
		dbr_double_t	delay;
		dbr_double_t	request = 0.5;
		dbr_double_t	accuracy;

		tsLocalTime(&start_time);
		status = ca_pend_event(request);
		if (status != ECA_TIMEOUT) {
			SEVCHK(status, NULL);
		}
		tsLocalTime(&end_time);
		TsDiffAsDouble(&delay,&end_time,&start_time);
		accuracy = 100.0*(delay-request)/request;
		printf("CA pend event delay accuracy = %f %%\n",
			accuracy);
		assert (fabs(accuracy) < 10.0);
	}

	size = dbr_size_n(DBR_GR_FLOAT, NUM);
	ptr = (struct dbr_gr_float *) malloc(size);  

	/*
	 * verify that we dont print a disconnect message when 
	 * we delete the last channel
	 * (this fails if we see a disconnect message)
	 */
	status = ca_search( pname, &chix3);
	SEVCHK(status, NULL);
	status = ca_pend_io(1000.0);
	SEVCHK(status, NULL);
	status = ca_clear_channel(chix3);
	SEVCHK(status, NULL);

	/*
	 * verify lots of disconnects 
	 * verify channel connected state variables
	 */
	printf("Connect/disconnect test");
	fflush(stdout);
	for (i = 0; i < 10; i++) {

		status = ca_search(
			    pname,
			    &chix3);
		SEVCHK(status, NULL);

		status = ca_search(
			    pname,
			    &chix4);
		SEVCHK(status, NULL);

		status = ca_search(
			    pname,
			    &chix2);
		SEVCHK(status, NULL);

		status = ca_search(
			    pname,
			    &chix1);
		SEVCHK(status, NULL);

		if (ca_test_io() == ECA_IOINPROGRESS) {
			assert(INVALID_DB_REQ(chix1->type) == TRUE);
			assert(INVALID_DB_REQ(chix2->type) == TRUE);
			assert(INVALID_DB_REQ(chix3->type) == TRUE);
			assert(INVALID_DB_REQ(chix4->type) == TRUE);

			assert(ca_state(chix1) == cs_never_conn);
			assert(ca_state(chix2) == cs_never_conn);
			assert(ca_state(chix3) == cs_never_conn);
			assert(ca_state(chix4) == cs_never_conn);
		}

		status = ca_pend_io(1000.0);
		SEVCHK(status, NULL);

		printf(".");
		fflush(stdout);

		assert(ca_test_io() == ECA_IODONE);

		assert(ca_state(chix1) == cs_conn);
		assert(ca_state(chix2) == cs_conn);
		assert(ca_state(chix3) == cs_conn);
		assert(ca_state(chix4) == cs_conn);

		SEVCHK(ca_clear_channel(chix4), NULL);
		SEVCHK(ca_clear_channel(chix3), NULL);
		SEVCHK(ca_clear_channel(chix2), NULL);
		SEVCHK(ca_clear_channel(chix1), NULL);
	}
	printf("\n");

	/*
	 * look for problems with ca_search(), ca_clear_channel(),
	 * ca_change_connection_event(), and ca_pend_io(() combo
	 */
	status = ca_search(pname,& chix3);
	SEVCHK(status, NULL);

	status = ca_replace_access_rights_event(chix3, accessSecurity_cb);
	SEVCHK(status, NULL);

	/*
	 * verify clear before connect
	 */
	status = ca_search(pname, &chix4);
	SEVCHK(status, NULL);

	status = ca_clear_channel(chix4);
	SEVCHK(status, NULL);

	status = ca_search(pname, &chix4);
	SEVCHK(status, NULL);

	status = ca_replace_access_rights_event(chix4, accessSecurity_cb);
	SEVCHK(status, NULL);

	status = ca_search(pname, &chix2);
	SEVCHK(status, NULL);

	status = ca_replace_access_rights_event(chix2, accessSecurity_cb);
	SEVCHK(status, NULL);

	status = ca_search(pname, &chix1);
	SEVCHK(status, NULL);

	status = ca_replace_access_rights_event(chix1, accessSecurity_cb);
	SEVCHK(status, NULL);

	status = ca_change_connection_event(chix1,conn);
	SEVCHK(status, NULL);

	status = ca_change_connection_event(chix1,NULL);
	SEVCHK(status, NULL);

	status = ca_change_connection_event(chix1,conn);
	SEVCHK(status, NULL);

	status = ca_change_connection_event(chix1,NULL);
	SEVCHK(status, NULL);

	status = ca_pend_io(1000.0);
	SEVCHK(status, NULL);

	assert (ca_state(chix1) == cs_conn);
	assert (ca_state(chix2) == cs_conn);
	assert (ca_state(chix3) == cs_conn);
	assert (ca_state(chix4) == cs_conn);

	assert (INVALID_DB_REQ(chix1->type) == FALSE);
	assert (INVALID_DB_REQ(chix2->type) == FALSE);
	assert (INVALID_DB_REQ(chix3->type) == FALSE);
	assert (INVALID_DB_REQ(chix4->type) == FALSE);

	/*
	 * clear chans before starting another test 
	 */
	status = ca_clear_channel(chix1);
	SEVCHK(status, NULL);
	status = ca_clear_channel(chix2);
	SEVCHK(status, NULL);
	status = ca_clear_channel(chix3);
	SEVCHK(status, NULL);
	status = ca_clear_channel(chix4);
	SEVCHK(status, NULL);

	/*
	 * verify ca_pend_io() does not see old search requests
	 * (that did not specify a connection handler)
	 */
	status = ca_search_and_connect(pname, &chix1, NULL, NULL);
	SEVCHK(status, NULL);
	status = ca_pend_io(1e-16);
	if (status==ECA_TIMEOUT) {
		assert(ca_state(chix1)==cs_never_conn);

		printf("waiting on pend io verify connect...");
		fflush(stdout);
		while (ca_state(chix1)!=cs_conn) {
			ca_pend_event(0.1);
		}
		printf("done\n");

		/*
		 * we end up here if the channel isnt on the same host
		 */
		status = ca_search_and_connect(pname, &chix2, NULL, NULL);
		SEVCHK(status, NULL);
		status = ca_pend_io(1e-16);
		if (status==ECA_TIMEOUT) {
			assert(ca_state(chix2)==cs_never_conn);
		}
		else {
			assert(ca_state(chix2)==cs_conn);
		}
		status = ca_clear_channel(chix2);
		SEVCHK(status, NULL);
	}
	else {
		assert(ca_state(chix1)==cs_conn);
	}
	status = ca_clear_channel(chix1);
	SEVCHK(status, NULL);

	/*
	 * verify connection handlers are working
	 */
	status = ca_search_and_connect(pname, &chix1, conn, NULL);
	SEVCHK(status, NULL);
	status = ca_search_and_connect(pname, &chix2, conn, NULL);
	SEVCHK(status, NULL);
	status = ca_search_and_connect(pname, &chix3, conn, NULL);
	SEVCHK(status, NULL);
	status = ca_search_and_connect(pname, &chix4, conn, NULL);
	SEVCHK(status, NULL);

	printf("waiting on conn handler call back connect...");
	fflush(stdout);
	while (conn_cb_count != 4) {
		ca_pend_event(0.1);
	}
	printf("done\n");

	printf("Read Access=%d Write Access=%d\n", 
		ca_read_access(chix1),
		ca_write_access(chix1));

	/*
	 * ca_pend_io() must block
	 */
	if(ca_read_access(chix4)){
		dbr_float_t	req;
		dbr_float_t	resp;

		printf ("get TMO test ...");
		fflush(stdout);
		req = 56.57f;
		resp = -99.99f;
		SEVCHK(ca_put(DBR_FLOAT, chix4, &req),NULL);
		SEVCHK(ca_get(DBR_FLOAT, chix4, &resp),NULL);
		status = ca_pend_io(1.0e-12);
		if (status==ECA_NORMAL) {
			if (resp != req) {
				printf (
	"get block test failed - val written %f\n", req);
				printf (
	"get block test failed - val read %f\n", resp);
				assert(0);
			}
		}
		else if (resp != -99.99f) {
			printf (
	"CA didnt block for get to return?\n");
		}
			
		req = 33.44f;
		resp = -99.99f;
		SEVCHK (ca_put(DBR_FLOAT, chix4, &req),NULL);
		SEVCHK (ca_get(DBR_FLOAT, chix4, &resp),NULL);
		SEVCHK (ca_pend_io(2000.0),NULL);
		if (resp != req) {
			printf (
	"get block test failed - val written %f\n", req);
			printf (
	"get block test failed - val read %f\n", resp);
			assert(0);
		}
		printf ("done\n");
	}

	/*
	 * Verify that we can do IO with the new types for ALH
	 */
#if 0
	if(ca_read_access(chix4)&&ca_write_access(chix4)){
	{
		dbr_put_ackt_t acktIn=1u;
		dbr_put_acks_t acksIn=1u;
		struct dbr_stsack_string stsackOut;

		SEVCHK (ca_put(DBR_PUT_ACKT, chix4, &acktIn),NULL);
		SEVCHK (ca_put(DBR_PUT_ACKS, chix4, &acksIn),NULL);
		SEVCHK (ca_get(DBR_STSACK_STRING, chix4, &stsackOut),NULL);
		SEVCHK (ca_pend_io(2000.0),NULL);
	}
#endif

	/*
	 * Verify that we can write and then read back
	 * the same analog value (DBR_FLOAT)
	 */
	if(	(ca_field_type(chix1)==DBR_DOUBLE || 
		ca_field_type(chix1)==DBR_FLOAT) && 
		ca_read_access(chix1) && 
		ca_write_access(chix1)){

		dbr_float_t incr;
		dbr_float_t epsil;
		dbr_float_t base;
		unsigned long iter;

		printf ("float test ...");
		fflush(stdout);
		epsil = FLT_EPSILON*4.0F;
		base = FLT_MIN;
		for (i=FLT_MIN_EXP; i<FLT_MAX_EXP; i+=FLT_MAX_EXP/10) {
			incr = (dbr_float_t) ldexp (0.5F,i);
			if (fabs(incr)>FLT_MAX/10.0) {
				iter = (unsigned long) (FLT_MAX/fabs(incr));
			}
			else {
				iter = 10.0;
			}
			floatTest(chix1, base, incr, epsil, iter);
		}
		base = FLT_MAX;
		for (i=FLT_MIN_EXP; i<FLT_MAX_EXP; i+=FLT_MAX_EXP/10) {
			incr =  (dbr_float_t) - ldexp (0.5F,i);
			if (fabs(incr)>FLT_MAX/10.0) {
				iter = (unsigned long) (FLT_MAX/fabs(incr));
			}
			else {
				iter = 10.0;
			}
			floatTest(chix1, base, incr, epsil, iter);
		}
		base = - FLT_MAX;
		for (i=FLT_MIN_EXP; i<FLT_MAX_EXP; i+=FLT_MAX_EXP/10) {
			incr = (dbr_float_t) ldexp (0.5F,i);
			if (fabs(incr)>FLT_MAX/10.0) {
				iter = (unsigned long) (FLT_MAX/fabs(incr));
			}
			else {
				iter = 10.0;
			}
			floatTest(chix1, base, incr, epsil, iter);
		}
		printf ("done\n");
	}

	/*
	 * Verify that we can write and then read back
	 * the same analog value (DBR_DOUBLE)
	 */
	if(	ca_field_type(chix1)==DBR_DOUBLE &&
		ca_read_access(chix1) && 
		ca_write_access(chix1)){

		dbr_double_t incr;
		dbr_double_t epsil;
		dbr_double_t base;
		unsigned long iter;

		printf ("double test ...");
		fflush(stdout);
		epsil = DBL_EPSILON*4;
		base = DBL_MIN;
		for (i=DBL_MIN_EXP; i<DBL_MAX_EXP; i+=DBL_MAX_EXP/10) {
			incr = ldexp (0.5,i);
			if (fabs(incr)>DBL_MAX/10.0) {
				iter = (unsigned long) (DBL_MAX/fabs(incr));
			}
			else {
				iter = 10.0;
			}
			doubleTest(chix1, base, incr, epsil, iter);
		}
		base = DBL_MAX;
		for (i=DBL_MIN_EXP; i<DBL_MAX_EXP; i+=DBL_MAX_EXP/10) {
			incr =  - ldexp (0.5,i);
			if (fabs(incr)>DBL_MAX/10.0) {
				iter = (unsigned long) (DBL_MAX/fabs(incr));
			}
			else {
				iter = 10.0;
			}
			doubleTest(chix1, base, incr, epsil, iter);
		}
		base = - DBL_MAX;
		for (i=DBL_MIN_EXP; i<DBL_MAX_EXP; i+=DBL_MAX_EXP/10) {
			incr = ldexp (0.5,i);
			if (fabs(incr)>DBL_MAX/10.0) {
				iter = (unsigned long) (DBL_MAX/fabs(incr));
			}
			else {
				iter = 10.0;
			}
			doubleTest(chix1, base, incr, epsil, iter);
		}
		printf ("done\n");
	}

	/*
	 * verify we dont jam up on many uninterrupted
	 * solicitations
	 */
	if(ca_read_access(chix4)){
		dbr_float_t	temp;

		printf("Performing multiple get test...");
		fflush(stdout);
		for(i=0; i<10000; i++){
			SEVCHK(ca_get(DBR_FLOAT, chix4, &temp),NULL);
		}
		SEVCHK(ca_pend_io(2000.0), NULL);
		printf("done.\n");
	}
	else{
		printf("Skipped multiple get test - no read access\n");
	}

	/*
	 * verify we dont jam up on many uninterrupted requests 
	 */
	if(ca_write_access(chix4)){
		printf("Performing multiple put test...");
		fflush(stdout);
		for(i=0; i<10000; i++){
			dbr_double_t fval = 3.3;
			status = ca_put(DBR_DOUBLE, chix4, &fval);
			SEVCHK(status, NULL);
		}
		SEVCHK(ca_pend_io(2000.0), NULL);
		printf("done.\n");
	}
	else{
		printf("Skipped multiple put test - no write access\n");
	}

	/*
	 * verify we dont jam up on many uninterrupted
	 * solicitations
	 */
	if(ca_read_access(chix1)){
		unsigned	count=0u;
		printf("Performing multiple get callback test...");
		fflush(stdout);
		for(i=0; i<10000; i++){
			status = ca_array_get_callback(
					DBR_FLOAT, 
					1,
					chix1, 
					null_event, 
					&count);
	
			SEVCHK(status, NULL);
		}
		SEVCHK(ca_flush_io(), NULL);
		while (count<10000u) {
			ca_pend_event(1.0);
			printf("waiting...");
			fflush(stdout);
		}
		printf("done.\n");
	}
	else{
		printf("Skipped multiple get cb test - no read access\n");
	}

	/*
	 * verify we dont jam up on many uninterrupted
	 * put callback solicitations
	 */
	if(ca_write_access(chix1) && ca_v42_ok(chix1)){
		unsigned	count=0u;
		printf("Performing multiple put callback test...");
		fflush(stdout);
		for(i=0; i<10000; i++){
			dbr_float_t	fval = 3.3F;
			status = ca_array_put_callback(
					DBR_FLOAT, 
					1,
					chix1, 
					&fval,
					null_event, 
					&count);
		
			SEVCHK(status, NULL);
		}
		SEVCHK(ca_flush_io(), NULL);
		while (count<10000u) {
			ca_pend_event(1.0);
			printf("waiting...");
			fflush(stdout);
		}

		printf("done.\n");
	}
	else{
		printf("Skipped multiple put cb test - no write access\n");
	}

	/*
	 * verify that we detect that a large string has been written
	 */
	if(ca_write_access(chix1)){
		dbr_string_t	stimStr;
		dbr_string_t	respStr;
		memset(stimStr, 'a', sizeof(stimStr));
		status = ca_array_put(DBR_STRING, 1u, chix1, stimStr);
		assert(status==ECA_STRTOBIG);
		sprintf(stimStr, "%u", 8u);
		status = ca_array_put(DBR_STRING, 1u, chix1, stimStr);
		assert(status==ECA_NORMAL);
		status = ca_array_get(DBR_STRING, 1u, chix1, respStr);
		assert(status==ECA_NORMAL);
		status = ca_pend_io(0.0);
		assert(status==ECA_NORMAL);
		printf(
"Test fails if stim \"%s\" isnt roughly equiv to resp \"%s\"\n",
			stimStr, respStr);
	}
	else{
		printf("Skipped bad string test - no write access\n");
	}

	if(ca_v42_ok(chix1)){
		test_sync_groups(chix1);
	}

	/*
	 * verify we can add many monitors at once
	 */
	printf("Performing multiple monitor test...");
	fflush(stdout);
	{
		unsigned	count=0u;
		evid		mid[1000];
		dbr_float_t	temp;

		for(i=0; i<NELEMENTS(mid); i++){
			SEVCHK(ca_add_event(DBR_GR_FLOAT, chix4, null_event,
				&count, &mid[i]),NULL);
		}
		/*
		 * force all of the monitors requests to
		 * complete
		 *
		 * NOTE: this hopefully demonstrates that when the
		 * server is very busy with monitors the client 
		 * is still able to punch through with a request.
		 */
		SEVCHK(ca_get(DBR_FLOAT,chix4,&temp),NULL);
		SEVCHK(ca_pend_io(1000.0),NULL);

		for(i=0; i<NELEMENTS(mid); i++){
			status = ca_clear_event(mid[i]);
			if(status != ECA_NORMAL){
				printf(
			"Clear of event %ld %x failed because \"%s\"\n",
					i,
					mid[i]->id,
					ca_message(status));
			}
			SEVCHK(status,NULL);
		}

		/*
		 * force all of the clear event requests to
		 * complete
		 */
		SEVCHK(ca_get(DBR_FLOAT,chix4,&temp),NULL);
		SEVCHK(ca_pend_io(1000.0),NULL);
	}
	printf("done.\n");
	
	if (VALID_DB_REQ(chix4->type)) {
		status = ca_add_event(
				DBR_FLOAT, 
				chix4, 
				EVENT_ROUTINE, 
				&monCount, 
				&monix);
		SEVCHK(status, NULL);
		SEVCHK(ca_clear_event(monix), NULL);
		status = ca_add_event(
				DBR_FLOAT, 
				chix4, 
				EVENT_ROUTINE, 
				&monCount, 
				&monix);
		SEVCHK(status, NULL);
	}
	if (VALID_DB_REQ(chix4->type)) {
		status = ca_add_event(
				DBR_FLOAT, 
				chix4, 
				EVENT_ROUTINE, 
				&monCount, 
				&monix);
		SEVCHK(status, NULL);
		SEVCHK(ca_clear_event(monix), NULL);
	}
	if (VALID_DB_REQ(chix3->type)) {
		status = ca_add_event(
				DBR_FLOAT, 
				chix3, 
				EVENT_ROUTINE, 
				&monCount, 
				&monix);
		SEVCHK(status, NULL);
		status = ca_add_event(
				DBR_FLOAT, 
				chix3, 
				write_event, 
				&monCount, 
				&monix);
		SEVCHK(status, NULL);
	}

	pfloat = (dbr_float_t *) calloc(sizeof(*pfloat),NUM);
	pdouble = (dbr_double_t *) calloc(sizeof(*pdouble),NUM);
	pgrfloat = (struct dbr_gr_float *) calloc(sizeof(*pgrfloat),NUM);

	if (VALID_DB_REQ(chix1->type))
		if (pfloat)
			for (i = 0; i < NUM; i++) {
				for (j = 0; j < NUM; j++)
					sprintf(&pstring[j][0], "%ld", j + 100l);
				SEVCHK(ca_array_put(
						DBR_STRING, 
						NUM, 
						chix1, 
						pstring), 
						NULL)
				SEVCHK(ca_array_get(
						DBR_FLOAT, 
						NUM, 
						chix1, 
						pfloat), 
						NULL)
				SEVCHK(ca_array_get(
						DBR_DOUBLE, 
						NUM, 
						chix1, 
						pdouble), 
						NULL)
				SEVCHK(ca_array_get(
						DBR_GR_FLOAT, 
						NUM, 
						chix1, 
						pgrfloat), 
						NULL)
			}
		else
			assert(0);

	SEVCHK(ca_pend_io(4000.0), NULL);

	/*
	 * array test
	 * o verifies that we can at least write and read back the same array
	 * if multiple elements are present
	 */
	if (VALID_DB_REQ(chix1->type)) {
		if (ca_element_count(chix1)>1u && ca_read_access(chix1)) {
			dbr_float_t	*pRF, *pWF, *pEF, *pT1, *pT2;

			printf("Performing %u element array test...",
					ca_element_count(chix1));
			fflush(stdout);

			pRF = (dbr_float_t *) calloc(ca_element_count(chix1), 
						sizeof(*pRF));
			assert(pRF!=NULL);

			pWF = (dbr_float_t *)calloc(ca_element_count(chix1), 
						sizeof(*pWF));
			assert(pWF!=NULL);

			/*
			 * write some random numbers into the array
			 */
			if (ca_write_access(chix1)) {
				pT1 = pWF;
				pEF = &pWF[ca_element_count(chix1)];
				while(pT1<pEF) {
					*pT1++ = rand();
				}
				status = ca_array_put(
						DBR_FLOAT, 
						ca_element_count(chix1), 
						chix1, 
						pWF); 
				SEVCHK(status, "array write request failed");
			}

			/*
			 * read back the array
			 */
			if (ca_read_access(chix1)) {
				status = ca_array_get(
						DBR_FLOAT, 
						ca_element_count(chix1), 
						chix1, 
						pRF); 
				SEVCHK(status, "array read request failed");
				status = ca_pend_io(30.0);
				SEVCHK(status, "array read failed");
			}

			/*
			 * verify read response matches values written
			 */
			if (ca_read_access(chix1) && ca_write_access(chix1)) {
				pEF = &pRF[ca_element_count(chix1)];
				pT1 = pRF;
				pT2 = pWF;
				while (pT1<pEF) {
					assert (*pT1 == *pT2);
					pT1++;
					pT2++;
				}
			}
			printf("done\n");
			free(pRF);
			free(pWF);
		}
	}

	for (i = 0; i < NUM; i++) {
		printf("Float value Returned from put/get %f\n", pfloat[i]);
		printf("Double value Returned from put/get %f\n", pdouble[i]);
		printf("GR Float value Returned from put/get %f\n", pgrfloat[i].value);
	}

#if 0
	for (i = 0; i < 10; i++)
		ca_get_callback(DBR_GR_FLOAT, chix1, ca_test_event, NULL);
#endif


	SEVCHK(ca_modify_user_name("Willma"), NULL);
	SEVCHK(ca_modify_host_name("Bed Rock"), NULL);

	{
		TS_STAMP	end_time;
		TS_STAMP	start_time;
		dbr_double_t	delay;
		dbr_double_t	request = 15.0;
		dbr_double_t	accuracy;

		tsLocalTime(&start_time);
		printf("waiting for events for %f sec\n", request);
		status = ca_pend_event(request);
		if (status != ECA_TIMEOUT) {
			SEVCHK(status, NULL);
		}
		tsLocalTime(&end_time);
		TsDiffAsDouble(&delay,&end_time,&start_time);
		accuracy = 100.0*(delay-request)/request;
		printf("CA pend event delay accuracy = %f %%\n",
			accuracy);
		assert (fabs(accuracy) < 10.0);
	}

	{
		TS_STAMP	end_time;
		TS_STAMP	start_time;
		dbr_double_t	delay;

		tsLocalTime(&start_time);
		printf("entering ca_task_exit()\n");
		status = ca_task_exit();
		SEVCHK(status,NULL);
		tsLocalTime(&end_time);
		TsDiffAsDouble(&delay,&end_time,&start_time);
		printf("in ca_task_exit() for %f sec\n", delay);
	}

	if (ptr){
		free (ptr);
	}
	if (pfloat) {
		free(pfloat);
	}
	if (pdouble) {
		free(pdouble);
	}
	if (pgrfloat) {
		free(pgrfloat);
	}

	return(0);
}

void floatTest(
chid		chan,
dbr_float_t 	beginValue, 
dbr_float_t	increment,
dbr_float_t 	epsilon,
unsigned 	iterations)
{
	unsigned	i;
	dbr_float_t	fval;
	dbr_float_t	fretval;
	int		status;

	fval = beginValue;
	for (i=0; i<iterations; i++) {
		fretval = FLT_MAX;
		status = ca_put (DBR_FLOAT, chan, &fval);
		SEVCHK (status, NULL);
		status = ca_get (DBR_FLOAT, chan, &fretval);
		SEVCHK (status, NULL);
		status = ca_pend_io (100.0);
		SEVCHK (status, NULL);
		if (fabs(fval-fretval) > epsilon) {
			printf ("float test failed val written %f\n", fval);
			printf ("float test failed val read %f\n", fretval);
			assert(0);
		}

		fval += increment;
	}
}

void doubleTest(
chid		chan,
dbr_double_t 	beginValue, 
dbr_double_t	increment,
dbr_double_t 	epsilon,
unsigned 	iterations)
{
	unsigned	i;
	dbr_double_t	fval;
	dbr_double_t	fretval;
	int		status;

	fval = beginValue;
	for (i=0; i<iterations; i++) {
		fretval = DBL_MAX;
		status = ca_put (DBR_DOUBLE, chan, &fval);
		SEVCHK (status, NULL);
		status = ca_get (DBR_DOUBLE, chan, &fretval);
		SEVCHK (status, NULL);
		status = ca_pend_io (100.0);
		SEVCHK (status, NULL);
		if (fabs(fval-fretval) > epsilon) {
			printf ("float test failed val written %f\n", fval);
			printf ("float test failed val read %f\n", fretval);
			assert(0);
		}

		fval += increment;
	}
}

void null_event(struct event_handler_args args)
{
	unsigned	*pInc = (unsigned *) args.usr;

	if (pInc) {
		(*pInc)++;
		if (*pInc%1000u == 0u) {
			printf("1000 occurred\n");
		}
	}
#if 0
	if (ca_state(args.chid)==cs_conn) {
		status = ca_put(DBR_FLOAT, args.chid, &fval);
		SEVCHK(status, "put failed in null_event()");
	}
	else {
		printf("null_event() called for disconnected %s\n",
				ca_name(args.chid));
	}
#endif
}


void write_event(struct event_handler_args args)
{
	int		status;
	dbr_float_t	*pFloat = (dbr_float_t *) args.dbr;
	dbr_float_t     a;

	if (!args.dbr) {
		return;
	}

	a = *pFloat;
	a += 10.1F;

	status = ca_array_put(
			DBR_FLOAT, 
			1,
			args.chid, 
			&a);
	SEVCHK(status,NULL);
	SEVCHK(ca_flush_io(), NULL);
}

void conn(struct connection_handler_args args)
{

	if (args.op == CA_OP_CONN_UP)
		printf("Channel On Line [%s]\n", ca_name(args.chid));
	else if (args.op == CA_OP_CONN_DOWN)
		printf("Channel Off Line [%s]\n", ca_name(args.chid));
	else
		printf("Ukn conn ev\n");

	ca_get_callback(DBR_GR_FLOAT, args.chid, get_cb, NULL);
}

void get_cb (struct event_handler_args args)
{
	if(!(args.status & CA_M_SUCCESS)){
		printf("Get cb failed because \"%s\"\n", 
			ca_message(args.status));
	}
	conn_cb_count++;
}


/*
 * test_sync_groups()
 */
void test_sync_groups(chid chix)
{
	int		status;
	CA_SYNC_GID	gid1=0;
	CA_SYNC_GID	gid2=0;

	printf("Performing sync group test...");
	fflush(stdout);

	status = ca_sg_create(&gid1);
	SEVCHK(status, NULL);

	multiple_sg_requests(chix, gid1);
	status = ca_sg_reset(gid1);
	SEVCHK(status, NULL);

	status = ca_sg_create(&gid2);
	SEVCHK(status, NULL);

	multiple_sg_requests(chix, gid2);
	multiple_sg_requests(chix, gid1);
	status = ca_sg_test(gid2);
	SEVCHK(status, "SYNC GRP2");
	status = ca_sg_test(gid1);
	SEVCHK(status, "SYNC GRP1");
	status = ca_sg_block(gid1, 500.0);
	SEVCHK(status, "SYNC GRP1");
	status = ca_sg_block(gid2, 500.0);
	SEVCHK(status, "SYNC GRP2");

	status = ca_sg_delete(gid2);
	SEVCHK(status, NULL);
	status = ca_sg_create(&gid2);
	SEVCHK(status, NULL);

	multiple_sg_requests(chix, gid1);
	multiple_sg_requests(chix, gid2);
	status = ca_sg_block(gid1, 15.0);
	SEVCHK(status, "SYNC GRP1");
	status = ca_sg_block(gid2, 15.0);
	SEVCHK(status, "SYNC GRP2");
	status = ca_sg_delete(gid1);
	SEVCHK(status, NULL);
	status = ca_sg_delete(gid2);
	SEVCHK(status, NULL);

	printf("done\n");
}



/*
 * multiple_sg_requests()
 */
void multiple_sg_requests(chid chix, CA_SYNC_GID gid)
{
	int			status;
	unsigned		i;
	static dbr_float_t	fvalput	 = 3.3F;
	static dbr_float_t	fvalget;

	for(i=0; i<1000; i++){
		if(ca_write_access(chix)){
			status = ca_sg_array_put(
					gid,
					DBR_FLOAT, 
					1,
					chix, 
					&fvalput);
			SEVCHK(status, NULL);
		}

		if(ca_read_access(chix)){
			status = ca_sg_array_get(
					gid,
					DBR_FLOAT, 
					1,
					chix, 
					&fvalget);
			SEVCHK(status, NULL);
		}
	}
}


/*
 * accessSecurity_cb()
 */
void 	accessSecurity_cb(struct access_rights_handler_args args)
{
#	ifdef DEBUG
		printf(	"%s on %s has %s/%s access\n",
			ca_name(args.chid),
			ca_host_name(args.chid),
			ca_read_access(args.chid)?"read":"noread",
			ca_write_access(args.chid)?"write":"nowrite");
#	endif
}

