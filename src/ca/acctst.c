static char *sccsId = "$Id$\t$Date$";

/*
 * CA test/debug routine
 */

#if 1
#define CA_TEST_CHNL	"ca:ai_2000"
#define CA_TEST_CHNL4	"ca:ai_2000"
#else
#if 0
#define CA_TEST_CHNL	"ts2:ai0"
#define CA_TEST_CHNL4	"ts2:ai0"
#else
#define CA_TEST_CHNL	"mv16z:ai1"
#define CA_TEST_CHNL4	"mv16z:ai1"
#endif
#endif

/* System includes		 */
#if defined(UNIX)
#	include		<stdio.h>
#elif defined(vxWorks)
#	include		<vxWorks.h>
#	include		<taskLib.h>
#endif

#include 		<cadef.h>
#include		<db_access.h>
#include		<os_depen.h>


/*
 * #define EVENT_ROUTINE	ca_test_event #define CONN_ROUTINE	NULL
 */
#define EVENT_ROUTINE	null_event
#define CONN_ROUTINE	conn

#define NUM		1


#ifdef vxWorks
spacctst()
{
	int             acctst();

	return taskSpawn("acctst", 200, VX_FP_TASK, 20000, acctst);
}
#endif


#ifdef vxWorks
acctst()
#else
main()
#endif
{
	chid            chix1;
	chid            chix2;
	chid            chix3;
	chid            chix4;
	void            ca_test_event();
	void            null_event();
	struct dbr_gr_float *ptr;
	struct dbr_gr_float *pgrfloat;
	long            status;
	long            i, j;
	evid            monix;
	float          *pfloat;
	double         *pdouble;
	char            pstring[NUM][MAX_STRING_SIZE];
	void            write_event();
	void            conn();


	SEVCHK(ca_task_initialize(), "Unable to initialize");

	ca_printf("begin\n");
#ifdef VMS
	lib$init_timer();
#endif

	ptr = (struct dbr_gr_float *)
		malloc(dbr_size[DBR_GR_FLOAT] + 
			dbr_value_size[DBR_GR_FLOAT] * (NUM - 1));

	for (i = 0; i < 10; i++) {

		status = ca_array_build(CA_TEST_CHNL,	/* channel ASCII name	 */
					DBR_GR_FLOAT,	/* fetch external type	 */
					NUM,	/* array element cnt	 */
					&chix3,	/* ptr to chid		 */
					ptr	/* pointer to recv buf	 */
			);
		SEVCHK(status, NULL);

		SEVCHK(ca_build_and_connect(
					    CA_TEST_CHNL4,
					    TYPENOTCONN,
					    0,
					    &chix4,
					    NULL,
					    CONN_ROUTINE,
					    NULL), NULL);
		SEVCHK(ca_build_and_connect(
					    CA_TEST_CHNL,
					    TYPENOTCONN,
					    0,
					    &chix2,
					    NULL,
					    CONN_ROUTINE,
					    NULL), NULL);
		SEVCHK(ca_build_and_connect(
					    CA_TEST_CHNL,
					    TYPENOTCONN,
					    0,
					    &chix1,
					    NULL,
					    CONN_ROUTINE,
					    NULL), NULL);

		printf("IO status is: %s\n",ca_message(ca_test_io()));

		printf("chix1 is on %s\n", ca_host_name(chix1));
		printf("chix2 is on %s\n", ca_host_name(chix2));
		printf("chix4 is on %s\n", ca_host_name(chix4));

		status = ca_pend_io(10.0);
		SEVCHK(status, NULL);

		printf("IO status is: %s\n",ca_message(ca_test_io()));

		printf("chix1 is on %s\n", ca_host_name(chix1));
		printf("chix2 is on %s\n", ca_host_name(chix2));
		printf("chix4 is on %s\n", ca_host_name(chix4));

		SEVCHK(ca_clear_channel(chix4), NULL);
		SEVCHK(ca_clear_channel(chix2), NULL);
		SEVCHK(ca_clear_channel(chix3), NULL);
		SEVCHK(ca_clear_channel(chix1), NULL);

	}

	status = ca_array_build(
				CA_TEST_CHNL,	/* channel ASCII name	 */
				DBR_GR_FLOAT,	/* fetch external type	 */
				NUM,	/* array element cnt	 */
				&chix3,	/* ptr to chid		 */
				ptr	/* pointer to recv buf	 */
		);
	SEVCHK(status, NULL);

	SEVCHK(ca_build_and_connect(
				    CA_TEST_CHNL4,
				    TYPENOTCONN,
				    0,
				    &chix4,
				    NULL,
				    CONN_ROUTINE,
				    NULL), NULL);
	SEVCHK(ca_build_and_connect(
				    CA_TEST_CHNL,
				    TYPENOTCONN,
				    0,
				    &chix2,
				    NULL,
				    CONN_ROUTINE,
				    NULL), NULL);
	SEVCHK(ca_build_and_connect(
				    CA_TEST_CHNL,
				    TYPENOTCONN,
				    0,
				    &chix1,
				    NULL,
				    CONN_ROUTINE,
				    NULL), NULL);

	status = ca_pend_io(10.0);
	SEVCHK(status, NULL);

	if (INVALID_DB_REQ(chix1->type))
		ca_printf("Failed to locate %s\n", CA_TEST_CHNL);
	if (INVALID_DB_REQ(chix2->type))
		ca_printf("Failed to locate %s\n", CA_TEST_CHNL);
	if (INVALID_DB_REQ(chix3->type))
		ca_printf("Failed to locate %s\n", CA_TEST_CHNL);
	if (INVALID_DB_REQ(chix4->type))
		ca_printf("Failed to locate %s\n", CA_TEST_CHNL4);
	/*
	 * SEVCHK(status,NULL); if(status == ECA_TIMEOUT) exit();
	 */

#ifdef VMS
	lib$show_timer();
#endif

	pfloat = &ptr->value;
	for (i = 0; i < NUM; i++)
		ca_printf("Value Returned from build %f\n", pfloat[i]);

#ifdef VMS
	lib$init_timer();
#endif

	/*
	 * verify we dont jam up on many uninterrupted
	 * solicitations
	 */
	ca_printf("Performing multiple get test...");
#ifdef UNIX
	fflush(stdout);
#endif
	{
		struct dbr_ctrl_float temp;
		for(i=0; i<10000; i++){
		
			SEVCHK(ca_get(DBR_GR_FLOAT, chix4, &temp),NULL);
		}
		SEVCHK(ca_pend_io(200.0), NULL);
	}
	ca_printf("done.\n");

	/*
	 * verify we can add many monitors at once
	 */
	ca_printf("Performing multiple monitor test...");
#ifdef UNIX
	fflush(stdout);
#endif
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
		SEVCHK(ca_pend_io(100.0),NULL);

		for(i=0; i<NELEMENTS(mid); i++){
			SEVCHK(ca_clear_event(mid[i]),NULL);
		}

		/*
		 * force all of the clear event requests to
		 * complete
		 */
		SEVCHK(ca_get(DBR_FLOAT,chix4,&temp),NULL);
		SEVCHK(ca_pend_io(100.0),NULL);
	}
	ca_printf("done.\n");
	
	if (VALID_DB_REQ(chix4->type)) {
		status = ca_add_event(DBR_FLOAT, chix4, EVENT_ROUTINE, 0xaaaaaaaa, &monix);
		SEVCHK(status, NULL);
		SEVCHK(ca_clear_event(monix), NULL);
		status = ca_add_event(DBR_FLOAT, chix4, EVENT_ROUTINE, 0xaaaaaaaa, &monix);
		SEVCHK(status, NULL);
	}
	if (VALID_DB_REQ(chix4->type)) {
		status = ca_add_event(DBR_FLOAT, chix4, EVENT_ROUTINE, 0xaaaaaaaa, &monix);
		SEVCHK(status, NULL);
		SEVCHK(ca_clear_event(monix), NULL);
	}
	if (VALID_DB_REQ(chix3->type)) {
		status = ca_add_event(DBR_FLOAT, chix3, EVENT_ROUTINE, 0xaaaaaaaa, &monix);
		SEVCHK(status, NULL);
		status = ca_add_event(DBR_FLOAT, chix3, write_event, 0xaaaaaaaa, &monix);
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
				SEVCHK(ca_array_put(DBR_STRING, NUM, chix1, pstring), NULL)
					SEVCHK(ca_array_get(DBR_FLOAT, NUM, chix1, pfloat), NULL)
					SEVCHK(ca_array_get(DBR_DOUBLE, NUM, chix1, pdouble), NULL)
					SEVCHK(ca_array_get(DBR_GR_FLOAT, NUM, chix1, pgrfloat), NULL)
			}
		else
			abort();

	SEVCHK(ca_pend_io(4.0), NULL);

#ifdef VMS
	lib$show_timer();
#endif
	for (i = 0; i < NUM; i++) {
		ca_printf("Float value Returned from put/get %f\n", pfloat[i]);
		ca_printf("Double value Returned from put/get %f\n", pdouble[i]);
		ca_printf("GR Float value Returned from put/get %f\n", pgrfloat[i].value);
	}

	for (i = 0; i < 10; i++)
		ca_get_callback(DBR_GR_FLOAT, chix1, ca_test_event, NULL);

	ca_printf("-- Put/Gets done- waiting for Events --\n");
	status = ca_pend_event(60.0);
	if (status == ECA_TIMEOUT) {

		free(ptr);
		free(pfloat);
		free(pgrfloat);

		exit();
	} else
		SEVCHK(status, NULL);

}





void 
null_event()
{
	static int      i;

	if (i++ > 1000) {
		ca_printf("1000 occured\n");
		i = 0;
	}
}


void 
write_event(args)
	struct event_handler_args args;
{
	float           a = *(float *) args.dbr;

	a += 10.1;

	SEVCHK(ca_rput(args.chid, &a), "write fail in event");
	SEVCHK(ca_flush_io(), NULL);
	/*
	 * #ifdef vxWorks taskDelay(20); #endif
	 */

}

void 
conn(args)
	struct connection_handler_args args;
{

	if (args.op == CA_OP_CONN_UP)
		ca_printf("Channel On Line [%s]\n", ca_name(args.chid));
	else if (args.op == CA_OP_CONN_DOWN)
		ca_printf("Channel Off Line [%s]\n", ca_name(args.chid));
	else
		ca_printf("Ukn conn ev\n");
}
