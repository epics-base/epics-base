
/*
 * CA test/debug routine
 */

static char *sccsId = "@(#) $Id$";

#ifdef VMS
#include <LIB$ROUTINES.H>
#endif

#include	<stdio.h>
#include	<stdlib.h>
#include	<assert.h>

#include	"os_depen.h"

#include 	<cadef.h>

#define EVENT_ROUTINE	null_event
#define CONN_ROUTINE	conn

#define NUM		1

int	conn_get_cb_count;

int	doacctst(char *pname);
void 	test_sync_groups(chid chix);
void 	multiple_sg_requests(chid chix, CA_SYNC_GID gid);
void 	null_event(struct event_handler_args args);
void 	write_event(struct event_handler_args args);
void 	conn(struct connection_handler_args args);
void 	conn_cb(struct event_handler_args args);
void 	accessSecurity_cb(struct access_rights_handler_args args);


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
main(int argc, char **argv)
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
	float          *pfloat = NULL;
	double         *pdouble = NULL;
	long            status;
	long            i, j;
	evid            monix;
	char            pstring[NUM][MAX_STRING_SIZE];


	SEVCHK(ca_task_initialize(), "Unable to initialize");

	conn_get_cb_count = 0;

	printf("begin\n");
#ifdef VMS
	lib$init_timer();
#endif /*VMS*/

	ptr = (struct dbr_gr_float *)
		malloc(dbr_size_n(DBR_GR_FLOAT, NUM));  

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
	 * the same value
	 */
	if(	(ca_field_type(chix1)==DBR_FLOAT || 
		ca_field_type(chix1)==DBR_DOUBLE) &&
		ca_read_access(chix1) && 
		ca_write_access(chix1)){

		double 	dval = 3.3;
		float	fval = -8893.3;
		double	dret;
		float	fret;

		status = ca_put(DBR_DOUBLE, chix1, &dval);
		SEVCHK(status, NULL);
		status = ca_get(DBR_DOUBLE, chix1, &dret);
		SEVCHK(status, NULL);
		ca_pend_io(30.0);
		assert(dval - dret < .000001);

		status = ca_put(DBR_FLOAT, chix1, &fval);
		SEVCHK(status, NULL);
		status = ca_get(DBR_FLOAT, chix1, &fret);
		SEVCHK(status, NULL);
		ca_pend_io(30.0);
		assert(fval - fret < .0001);
	}

	/*
	 * verify we dont jam up on many uninterrupted
	 * solicitations
	 */
	if(ca_read_access(chix4)){
		float	temp;

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
			double fval = 3.3;
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
		printf("Performing multiple get callback test...");
		fflush(stdout);
		for(i=0; i<10000; i++){
			status = ca_array_get_callback(
					DBR_FLOAT, 
					1,
					chix1, 
					null_event, 
					NULL);
	
			SEVCHK(status, NULL);
		}
		SEVCHK(ca_flush_io(), NULL);
		printf("done.\n");
	}
	else{
		printf("Skipped multiple get cb test - no read access\n");
	}

	if(ca_v42_ok(chix1)){
		test_sync_groups(chix1);
	}

	/*
	 * verify we dont jam up on many uninterrupted
	 * solicitations
	 */
	if(ca_write_access(chix1) && ca_v42_ok(chix1)){
		printf("Performing multiple put callback test...");
		fflush(stdout);
		for(i=0; i<10000; i++){
			float fval = 3.3;
			status = ca_array_put_callback(
					DBR_FLOAT, 
					1,
					chix1, 
					&fval,
					null_event, 
					NULL);
		
			SEVCHK(status, NULL);
		}
		SEVCHK(ca_flush_io(), NULL);
		printf("done.\n");
	}
	else{
		printf("Skipped multiple put cb test - no write access\n");
	}

	/*
	 * verify we can add many monitors at once
	 */
	printf("Performing multiple monitor test...");
	fflush(stdout);
	{
		evid	mid[1000];
		float	temp;

		for(i=0; i<NELEMENTS(mid); i++){
			SEVCHK(ca_add_event(DBR_GR_FLOAT, chix4, null_event,
				(void *)0x55555555, &mid[i]),NULL);
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
			"Clear of event %d %x failed because \"%s\"\n",
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
				(void *)0xaaaaaaaa, 
				&monix);
		SEVCHK(status, NULL);
		SEVCHK(ca_clear_event(monix), NULL);
		status = ca_add_event(
				DBR_FLOAT, 
				chix4, 
				EVENT_ROUTINE, 
				(void *)0xaaaaaaaa, 
				&monix);
		SEVCHK(status, NULL);
	}
	if (VALID_DB_REQ(chix4->type)) {
		status = ca_add_event(
				DBR_FLOAT, 
				chix4, 
				EVENT_ROUTINE, 
				(void *)0xaaaaaaaa, 
				&monix);
		SEVCHK(status, NULL);
		SEVCHK(ca_clear_event(monix), NULL);
	}
	if (VALID_DB_REQ(chix3->type)) {
		status = ca_add_event(
				DBR_FLOAT, 
				chix3, 
				EVENT_ROUTINE, 
				(void *)0xaaaaaaaa, 
				&monix);
		SEVCHK(status, NULL);
		status = ca_add_event(
				DBR_FLOAT, 
				chix3, 
				write_event, 
				(void *)0xaaaaaaaa, 
				&monix);
		SEVCHK(status, NULL);
	}

	pfloat = (float *) calloc(sizeof(float),NUM);
	pdouble = (double *) calloc(sizeof(double),NUM);
	pgrfloat = (struct dbr_gr_float *) calloc(sizeof(*pgrfloat),NUM);

	if (VALID_DB_REQ(chix1->type))
		if (pfloat)
			for (i = 0; i < NUM; i++) {
				for (j = 0; j < NUM; j++)
					sprintf(&pstring[j][0], "%d", j + 100);
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

	if (conn_get_cb_count != 3){
		printf ("!!!! Connect cb count = %d expected = 3 !!!!\n", 
			conn_get_cb_count);
	}

	printf("-- Put/Gets done- waiting for Events --\n");
	status = ca_pend_event(10.0);
	if (status != ECA_TIMEOUT) {
		SEVCHK(status, NULL);
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

	status = ca_task_exit();
	SEVCHK(status,NULL);

	return(0);
}



void null_event(struct event_handler_args args)
{
	static int      i;

	if (i++ > 1000) {
		printf("1000 occurred\n");
		i = 0;
	}
}


void write_event(struct event_handler_args args)
{
	int		status;
	float           a = *(float *) args.dbr;

	a += 10.1;

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

	ca_get_callback(DBR_GR_FLOAT, args.chid, conn_cb, NULL);
}

void conn_cb(struct event_handler_args args)
{
	if(!(args.status & CA_M_SUCCESS)){
		printf("Get cb failed because \"%s\"\n", ca_message(args.status));
	}
	conn_get_cb_count++;
}


/*
 * test_sync_groups()
 */
void test_sync_groups(chid chix)
{
	int		status;
	CA_SYNC_GID	gid1;
	CA_SYNC_GID	gid2;

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
	status = ca_sg_block(gid1, 15.0);
	SEVCHK(status, "SYNC GRP1");
	status = ca_sg_block(gid2, 15.0);
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
	int		status;
	unsigned	i;
	static float 	fvalput	 = 3.3;
	static float	fvalget;

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

