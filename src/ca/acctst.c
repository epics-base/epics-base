
/*
 * CA test/debug routine
 */

static char *sccsId = "$Id$";


#include		<stdio.h>
#include		<assert.h>

#include		"os_depen.h"

#include 		<cadef.h>


#define EVENT_ROUTINE	null_event
#define CONN_ROUTINE	conn

#define NUM		1

int	conn_get_cb_count;

#ifdef __STDC__
int	doacctst(char *pname);
void 	test_sync_groups(chid chix);
void 	multiple_sg_requests(chid chix, CA_SYNC_GID gid);
void 	null_event(struct event_handler_args args);
void 	write_event(struct event_handler_args args);
void 	conn(struct connection_handler_args args);
void 	conn_cb(struct event_handler_args args);
#else /*__STDC__*/
int	doacctst();
void 	test_sync_groups();
void 	multiple_sg_requests();
void 	null_event();
void 	write_event();
void 	conn();
void 	conn_cb();
#endif /*__STDC__*/


#ifdef vxWorks
#ifdef __STDC__
int acctst(char *pname)
#else /*__STDC__*/
int acctst(pname)
char *pname;
#endif /*__STDC__*/
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
#endif /*vxWorks*/

#ifdef UNIX
#ifdef __STDC__
main(int argc, char **argv)
#else /*__STDC__*/
main(argc, argv)
int argc;
char **argv;
#endif /*__STDC__*/
{
	if(argc == 2){
		doacctst(argv[1]);
	}
	else{
		printf("usage: %s <chan name>\n", argv[0]);
	}
	return 0;
}
#endif /*UNIX*/


#ifdef __STDC__
int doacctst(char *pname)
#else /*__STDC__*/
int doacctst(pname)
char *pname;
#endif /*__STDC__*/
{
	chid            chix1;
	chid            chix2;
	chid            chix3;
	chid            chix4;
	struct dbr_gr_float *ptr;
	struct dbr_gr_float *pgrfloat;
	long            status;
	long            i, j;
	evid            monix;
	float          *pfloat;
	double         *pdouble;
	char            pstring[NUM][MAX_STRING_SIZE];


	SEVCHK(ca_task_initialize(), "Unable to initialize");

	printf("begin\n");
#ifdef VMS
	lib$init_timer();
#endif /*VMS*/

	ptr = (struct dbr_gr_float *)
		malloc(dbr_size_n(DBR_GR_FLOAT, NUM));  

	for (i = 0; i < 10; i++) {

		status = ca_array_build(pname,	/* channel ASCII name	 */
					DBR_GR_FLOAT,	/* fetch external type	 */
					NUM,	/* array element cnt	 */
					&chix3,	/* ptr to chid		 */
					ptr	/* pointer to recv buf	 */
			);
		SEVCHK(status, NULL);

		SEVCHK(ca_build_and_connect(
					    pname,
					    TYPENOTCONN,
					    0,
					    &chix4,
					    NULL,
					    NULL,
					    NULL), NULL);
		SEVCHK(ca_build_and_connect(
					    pname,
					    TYPENOTCONN,
					    0,
					    &chix2,
					    NULL,
					    NULL,
					    NULL), NULL);
		SEVCHK(ca_build_and_connect(
					    pname,
					    TYPENOTCONN,
					    0,
					    &chix1,
					    NULL,
					    NULL,
					    NULL), NULL);

		printf("IO status is: %s\n",ca_message(ca_test_io()));

		printf("chix1 is on %s\n", ca_host_name(chix1));
		printf("chix2 is on %s\n", ca_host_name(chix2));
		printf("chix4 is on %s\n", ca_host_name(chix4));

		status = ca_pend_io(1000.0);
		SEVCHK(status, NULL);


		printf("IO status is: %s\n",ca_message(ca_test_io()));

		printf("chix1 is on %s\n", ca_host_name(chix1));
		printf("chix2 is on %s\n", ca_host_name(chix2));
		printf("chix4 is on %s\n", ca_host_name(chix4));

		SEVCHK(ca_clear_channel(chix4), NULL);
		SEVCHK(ca_clear_channel(chix3), NULL);
		SEVCHK(ca_clear_channel(chix2), NULL);
		SEVCHK(ca_clear_channel(chix1), NULL);

	}

	status = ca_array_build(
				pname,	/* channel ASCII name	 */
				DBR_GR_FLOAT,	/* fetch external type	 */
				NUM,	/* array element cnt	 */
				&chix3,	/* ptr to chid		 */
				ptr	/* pointer to recv buf	 */
		);
	SEVCHK(status, NULL);

	SEVCHK(ca_build_and_connect(
				    pname,
				    TYPENOTCONN,
				    0,
				    &chix4,
				    NULL,
				    CONN_ROUTINE,
				    NULL), NULL);
	SEVCHK(ca_build_and_connect(
				    pname,
				    TYPENOTCONN,
				    0,
				    &chix2,
				    NULL,
				    CONN_ROUTINE,
				    NULL), NULL);
	SEVCHK(ca_build_and_connect(
				    pname,
				    TYPENOTCONN,
				    0,
				    &chix1,
				    NULL,
				    CONN_ROUTINE,
				    NULL), NULL);

	status = ca_pend_io(1000.0);
	SEVCHK(status, NULL);

	if (INVALID_DB_REQ(chix1->type))
		printf("Failed to locate %s\n", pname);
	if (INVALID_DB_REQ(chix2->type))
		printf("Failed to locate %s\n", pname);
	if (INVALID_DB_REQ(chix3->type))
		printf("Failed to locate %s\n", pname);
	if (INVALID_DB_REQ(chix4->type))
		printf("Failed to locate %s\n", pname);
	/*
	 * SEVCHK(status,NULL); if(status == ECA_TIMEOUT) exit();
	 */

#ifdef VMS
	lib$show_timer();
#endif /*VMS*/

	pfloat = &ptr->value;
	for (i = 0; i < NUM; i++)
		printf("Value Returned from build %f\n", pfloat[i]);

#ifdef VMS
	lib$init_timer();
#endif /*VMS*/

	/*
	 * verify we dont jam up on many uninterrupted
	 * solicitations
	 */
	printf("Performing multiple get test...");
#ifdef UNIX
	fflush(stdout);
#endif /*UNIX*/
	{
		float	temp;
		for(i=0; i<10000; i++){
			SEVCHK(ca_get(DBR_FLOAT, chix4, &temp),NULL);
		}
		SEVCHK(ca_pend_io(2000.0), NULL);
	}
	printf("done.\n");

	/*
	 * verify we dont jam up on many uninterrupted requests 
	 */
	printf("Performing multiple put test...");
#ifdef UNIX
	fflush(stdout);
#endif /*UNIX*/
	for(i=0; i<10000; i++){
		double fval = 3.3;
		status = ca_put(DBR_DOUBLE, chix4, &fval);
		SEVCHK(status, NULL);
	}
	SEVCHK(ca_pend_io(2000.0), NULL);
	printf("done.\n");

	/*
	 * verify we dont jam up on many uninterrupted
	 * solicitations
	 */
	printf("Performing multiple get callback test...");
#ifdef UNIX
	fflush(stdout);
#endif /*UNIX*/
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

	test_sync_groups(chix1);

	/*
	 * verify we dont jam up on many uninterrupted
	 * solicitations
	 */
	printf("Performing multiple put callback test...");
#ifdef UNIX
	fflush(stdout);
#endif /*UNIX*/
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

	/*
	 * verify we can add many monitors at once
	 */
	printf("Performing multiple monitor test...");
#ifdef UNIX
	fflush(stdout);
#endif /*UNIX*/
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
			SEVCHK(ca_clear_event(mid[i]),NULL);
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
	pfloat = (float *) malloc(sizeof(float) * NUM);
	pdouble = (double *) malloc(sizeof(double) * NUM);
	pgrfloat = (struct dbr_gr_float *) malloc(sizeof(*pgrfloat) * NUM);


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

	for (i = 0; i < 10; i++)
		ca_get_callback(DBR_GR_FLOAT, chix1, ca_test_event, NULL);


	SEVCHK(ca_modify_user_name("Willma"), NULL);
	SEVCHK(ca_modify_host_name("Bed Rock"), NULL);

	assert(conn_get_cb_count == 3);

	printf("-- Put/Gets done- waiting for Events --\n");
	status = ca_pend_event(2000.0);
	if (status == ECA_TIMEOUT) {

		free(ptr);
		free(pfloat);
		free(pgrfloat);

		exit(0);
	} else
		SEVCHK(status, NULL);

	status = ca_task_exit();
	SEVCHK(status,NULL);

	return(0);
}



#ifdef __STDC__
void null_event(struct event_handler_args args)
#else /*__STDC__*/
void null_event(args)
struct event_handler_args args;
#endif /*__STDC__*/
{
	static int      i;

	if (i++ > 1000) {
		printf("1000 occured\n");
		i = 0;
	}
}


#ifdef __STDC__
void write_event(struct event_handler_args args)
#else /*__STDC__*/
void write_event(args)
struct event_handler_args args;
#endif /*__STDC__*/
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

#ifdef __STDC__
void conn(struct connection_handler_args args)
#else /*__STDC__*/
void conn(args)
struct connection_handler_args args;
#endif /*__STDC__*/
{

	if (args.op == CA_OP_CONN_UP)
		printf("Channel On Line [%s]\n", ca_name(args.chid));
	else if (args.op == CA_OP_CONN_DOWN)
		printf("Channel Off Line [%s]\n", ca_name(args.chid));
	else
		printf("Ukn conn ev\n");

	ca_get_callback(DBR_GR_FLOAT, args.chid, conn_cb, NULL);
}

#ifdef __STDC__
void conn_cb(struct event_handler_args args)
#else /*__STDC__*/
void conn_cb(args)
struct event_handler_args args;
#endif /*__STDC__*/
{
	if(!(args.status & CA_M_SUCCESS)){
		printf("Get cb failed because \"%s\"\n", ca_message(args.status));
	}
	conn_get_cb_count++;
}


/*
 * test_sync_groups()
 */
#ifdef __STDC__
void test_sync_groups(chid chix)
#else /*__STDC__*/
void test_sync_groups(chix)
chid chix;
#endif /*__STDC__*/
{
	int		status;
	CA_SYNC_GID	gid1;
	CA_SYNC_GID	gid2;

	printf("Performing sync group test...");
#ifdef UNIX
	fflush(stdout);
#endif /*UNIX*/

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
#ifdef __STDC__
void multiple_sg_requests(chid chix, CA_SYNC_GID gid)
#else /*__STDC__*/
void multiple_sg_requests(chix, gid)
chid 		chix;
CA_SYNC_GID	gid;
#endif /*__STDC__*/
{
	int		status;
	unsigned	i;
	static float 	fvalput	 = 3.3;
	static float	fvalget;

	for(i=0; i<1000; i++){
		status = ca_sg_array_put(
					gid,
					DBR_FLOAT, 
					1,
					chix, 
					&fvalput);
		SEVCHK(status, NULL);

		status = ca_sg_array_get(
					gid,
					DBR_FLOAT, 
					1,
					chix, 
					&fvalget);
		SEVCHK(status, NULL);
	}
}
