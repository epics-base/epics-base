
/*
 * CA test/debug routine
 */

static char *sccsId = "@(#) $Id$";

/*
 * $Log$
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

#ifdef VMS
#include <LIB$ROUTINES.H>
#endif

/*
 * ANSI
 */
#include	<stdio.h>
#include	<stdlib.h>
#include	<math.h>
#include	<float.h>

#include	"os_depen.h"

#include	<assert.h>
#include 	<cadef.h>

#define EVENT_ROUTINE	null_event
#define CONN_ROUTINE	conn

#define NUM		1

int	conn_cb_count;

#ifndef min
#define min(A,B) ((A)>(B)?(B):(A))
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

	SEVCHK(ca_task_initialize(), "Unable to initialize");

	conn_cb_count = 0;

	printf("begin\n");
#ifdef VMS
	lib$init_timer();
#endif /*VMS*/

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
		assert (abs(accuracy) < 10.0);
	}

	size = dbr_size_n(DBR_GR_FLOAT, NUM);
	ptr = (struct dbr_gr_float *) malloc(size);  

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

		status = ca_build_and_connect(
					    pname,
					    TYPENOTCONN,
					    0,
					    &chix1,
					    NULL,
					    NULL,
					    NULL);
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

	status = ca_search(pname,& chix3);
	SEVCHK(status, NULL);
	status = ca_replace_access_rights_event(chix3, accessSecurity_cb);

	/*
	 * verify clear before connect
	 */
	status = ca_search_and_connect(pname, &chix4, CONN_ROUTINE, NULL);
	SEVCHK(status, NULL);

	status = ca_clear_channel(chix4);
	SEVCHK(status, NULL);

	status = ca_search_and_connect(pname, &chix4, CONN_ROUTINE, NULL);
	SEVCHK(status, NULL);

	status = ca_replace_access_rights_event(chix4, accessSecurity_cb);
	SEVCHK(status, NULL);

	status = ca_search_and_connect(pname, &chix2, CONN_ROUTINE, NULL);
	SEVCHK(status, NULL);

	status = ca_replace_access_rights_event(chix2, accessSecurity_cb);
	SEVCHK(status, NULL);

	status = ca_search_and_connect(pname, &chix1, CONN_ROUTINE, NULL);
	SEVCHK(status, NULL);

	status = ca_replace_access_rights_event(chix1, accessSecurity_cb);
	SEVCHK(status, NULL);

	status = ca_pend_io(1000.0);
	SEVCHK(status, NULL);

	assert(ca_state(chix1) == cs_conn);
	assert(ca_state(chix2) == cs_conn);
	assert(ca_state(chix3) == cs_conn);
	assert(ca_state(chix4) == cs_conn);

	assert(INVALID_DB_REQ(chix1->type) == FALSE);
	assert(INVALID_DB_REQ(chix2->type) == FALSE);
	assert(INVALID_DB_REQ(chix3->type) == FALSE);
	assert(INVALID_DB_REQ(chix4->type) == FALSE);

#ifdef VMS
	lib$show_timer();
#endif /*VMS*/

#ifdef VMS
	lib$init_timer();
#endif /*VMS*/

	printf("Read Access=%d Write Access=%d\n", 
		ca_read_access(chix1),
		ca_write_access(chix1));

	/*
	 * Verify that we can write and then read back
	 * the same analog value (DBR_FLOAT)
	 */
	if(	(ca_field_type(chix1)==DBR_DOUBLE || 
		ca_field_type(chix1)==DBR_FLOAT) && 
		ca_read_access(chix1) && 
		ca_write_access(chix1)){

		dbr_double_t incr;
		dbr_double_t epsil;
		dbr_double_t base;
		unsigned long iter;

		printf ("float test ...");
		fflush(stdout);
		epsil = FLT_EPSILON*4;
		base = FLT_MIN;
		for (i=FLT_MIN_EXP; i<FLT_MAX_EXP; i+=FLT_MAX_EXP/10) {
			incr = ldexp (0.5,i);
			iter = FLT_MAX/fabs(incr);
			iter = min (iter,10);
			floatTest(chix1, base, incr, epsil, iter);
		}
		base = FLT_MAX;
		for (i=FLT_MIN_EXP; i<FLT_MAX_EXP; i+=FLT_MAX_EXP/10) {
			incr =  - ldexp (0.5,i);
			iter = FLT_MAX/fabs(incr);
			iter = min (iter,10);
			floatTest(chix1, base, incr, epsil, iter);
		}
		base = - FLT_MAX;
		for (i=FLT_MIN_EXP; i<FLT_MAX_EXP; i+=FLT_MAX_EXP/10) {
			incr = ldexp (0.5,i);
			iter = FLT_MAX/fabs(incr);
			iter = min (iter,10);
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
			iter = DBL_MAX/fabs(incr);
			iter = min (iter,10);
			doubleTest(chix1, base, incr, epsil, iter);
		}
		base = DBL_MAX;
		for (i=DBL_MIN_EXP; i<DBL_MAX_EXP; i+=DBL_MAX_EXP/10) {
			incr =  - ldexp (0.5,i);
			iter = DBL_MAX/fabs(incr);
			iter = min (iter,10);
			doubleTest(chix1, base, incr, epsil, iter);
		}
		base = - DBL_MAX;
		for (i=DBL_MIN_EXP; i<DBL_MAX_EXP; i+=DBL_MAX_EXP/10) {
			incr = ldexp (0.5,i);
			iter = DBL_MAX/fabs(incr);
			iter = min (iter,10);
			doubleTest(chix1, base, incr, epsil, iter);
		}
		printf ("done\n");
	}

	/*
	 * ca_pend_io() must block
	 */
	if(ca_read_access(chix4)){
		dbr_float_t	req = 3.3;
		dbr_float_t	resp = 0.0;

		printf ("get TMO test ...");
		fflush(stdout);
		SEVCHK(ca_put(DBR_FLOAT, chix4, &req),NULL);
		SEVCHK(ca_get(DBR_FLOAT, chix4, &resp),NULL);
		status = ca_pend_io(1.0e-12);
		if (status==ECA_NORMAL) {
			assert (resp == req);
		}
		else {
			assert (resp == 0.0);
		}
			
		resp = 0.0;
		SEVCHK (ca_put(DBR_FLOAT, chix4, &req),NULL);
		SEVCHK (ca_get(DBR_FLOAT, chix4, &resp),NULL);
		SEVCHK (ca_pend_io(2000.0),NULL);
		assert (resp == req);
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
	 * solicitations
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

	if(ca_v42_ok(chix1)){
		test_sync_groups(chix1);
	}

	/*
	 * verify we can add many monitors at once
	 */
	printf("Performing multiple monitor test...");
	fflush(stdout);
	{
		evid		mid[1000];
		dbr_float_t	temp;

		for(i=0; i<NELEMENTS(mid); i++){
			SEVCHK(ca_add_event(DBR_GR_FLOAT, chix4, null_event,
				NULL, &mid[i]),NULL);
		}
		/*
		 * force all of the monitors requests to
		 * complete
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
				NULL, 
				&monix);
		SEVCHK(status, NULL);
		SEVCHK(ca_clear_event(monix), NULL);
		status = ca_add_event(
				DBR_FLOAT, 
				chix4, 
				EVENT_ROUTINE, 
				NULL, 
				&monix);
		SEVCHK(status, NULL);
	}
	if (VALID_DB_REQ(chix4->type)) {
		status = ca_add_event(
				DBR_FLOAT, 
				chix4, 
				EVENT_ROUTINE, 
				NULL, 
				&monix);
		SEVCHK(status, NULL);
		SEVCHK(ca_clear_event(monix), NULL);
	}
	if (VALID_DB_REQ(chix3->type)) {
		status = ca_add_event(
				DBR_FLOAT, 
				chix3, 
				EVENT_ROUTINE, 
				NULL, 
				&monix);
		SEVCHK(status, NULL);
		status = ca_add_event(
				DBR_FLOAT, 
				chix3, 
				write_event, 
				NULL, 
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

#ifdef VMS
	lib$show_timer();
#endif /*VMS*/
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

	if (conn_cb_count != 3){
		printf ("!!!! Connect cb count = %d expected = 3 !!!!\n", 
			conn_cb_count);
	}

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
		assert (abs(accuracy) < 10.0);
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
	static int      i;
	unsigned	*pInc = (unsigned *) args.usr;

	if (pInc) {
		(*pInc)++;
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

	if (i++ > 1000) {
		printf("1000 occurred\n");
		i = 0;
	}
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

