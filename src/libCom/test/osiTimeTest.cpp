#include <stdio.h>
#include <time.h>
#include <math.h>

#include "osiTime.h"
#include "osiThread.h"
#include "errlog.h"

extern "C" {
int osiTimeTest (void);
}

int osiTimeTest (void)
{
    unsigned i, error;
    osiTime begin = osiTime::getCurrent();
    const unsigned wasteTime = 100000u;
    const int nTimes = 100;
    TS_STAMP stamp;
    struct timespec ts;
    struct tm tmAnsi;
    tm_nano_sec ansiDate;
    unsigned long nanoSec;
    double diff;

    printf ("osiTime Test (%3d loops)\n========================\n\n", nTimes);

    for (int iTimes=0; iTimes < nTimes; ++iTimes) {
        for (i=0; i<wasteTime; i++) {
        	osiTime tmp = osiTime::getCurrent();
        }

        osiTime end = osiTime::getCurrent();

        diff = end - begin;

        if (iTimes == 0) {
		    printf ("Elapsed time per call to osiTime::getCurrent() "
					"(%d calls) = %f12 sec\n\n", wasteTime,
					diff/wasteTime);

			stamp = begin;
			ansiDate = begin;
			ts = begin;

			printf ("The following should be your local time\ndisplayed using "
				   "four different internal representations:\n\n");

			tsStampToTM (&tmAnsi, &nanoSec, &stamp);

            printf ("TS_STAMP = %s %lu nSec \n", asctime(&tmAnsi), nanoSec);
            printf ("struct tm = %s %f\n", asctime(&ansiDate.ansi_tm), 
					ansiDate.nSec/(double)osiTime::nSecPerSec);
			tmAnsi = *localtime (&ts.tv_sec);
            printf ("struct timespec = %s %f\n", asctime(&ansiDate.ansi_tm), 
					ts.tv_nsec/(double)osiTime::nSecPerSec);
			begin.show (0);
			printf ("\n");
		} else {
			if (iTimes % 10 == 0)
				printf (" ... now at loop %3d\n", iTimes);
		}		   

        osiTime copy = end;
		error = 0;

        if (!(copy==end)) {
			printf ("Loop #%3d: Failed test copy==end\n",iTimes);
			error = 1;
		}
		if (!(copy<=end)) {
			printf ("Loop #%3d: Failed test copy<=end %d\n",iTimes);
			error = 1;
		}
        if (!(copy>=end)) {
			printf ("Loop #%3d: Failed test copy>=end %d\n",iTimes);
			error = 1;
		}
        if (!(end>begin)) {
			printf ("Loop #%3d: Failed test end>begin %d\n",iTimes);
			error = 1;
		}
		if (!(end>=begin)) {
			printf ("Loop #%3d: Failed test end>=begin %d\n",iTimes);
			error = 1;
		}
        if (!(begin<end)) {
			printf ("Loop #%3d: Failed test begin<end %d\n",iTimes);
			error = 1;
		}
        if (!(begin<=end)) {
			printf ("Loop #%3d: Failed test begin<=end %d\n",iTimes);
			error = 1;
		}
        if (!(begin!=end)) {
			printf ("Loop #%3d: Failed test begin!=end %d\n",iTimes);
			error = 1;
		}
		if (!(end-begin==diff)) {
			printf ("Loop #%3d: Failed test end-begin==diff %d\n",iTimes);
			error = 1;
		}

        begin += diff;
        if (!(begin==end) && (fabs(begin-end) > 1.0e-6)) {
			printf ("Loop #%3d: Failed test (begin+=diff)==end by %12.9f\n",
					iTimes, fabs(begin-end));
			error = 1;
		}

        begin -= diff;
        if (!(begin+diff==end) && (fabs(begin+diff-end) > 1.0e-6)) {
			printf ("Loop #%3d: Failed test begin+diff==end\n",iTimes);
			error = 1;
		}

        //
        // test struct tm conversions
        //
        ansiDate = begin;
        begin = ansiDate;
        if (!(begin+diff==end) && (fabs(begin+diff-end) > 1.0e-6)) {
			printf ("Loop #%3d: Failed test begin+diff==end "
					"after tm conversions\n",iTimes);
			error = 1;
		}

        //
        // test struct timespec conversion
        //
        ts = begin;
        begin = ts;
        if (!(begin+diff==end) && (fabs(begin+diff-end) > 1.0e-6)) {
			printf ("Loop #%3d: Failed test begin+diff==end "
					"after timespec conversions\n",iTimes);
			error = 1;
		}
		
		if (error) {
			printf ("Loop #%3d: begin ", iTimes); begin.show(0);
			printf ("Loop #%3d: end   ", iTimes); end.show(0);
			printf ("Loop #%3d: diff  %12.9f\n\n", iTimes, diff);
		}
    }
    return 0;
}

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
