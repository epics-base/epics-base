
/*
 *
 * escape into C to call signal because of a brain dead
 * signal() func proto supplied in signal.h by gcc 2.7.2
 *
 */

#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sigPipeIgnore.h>

typedef void (*pSigFunc) ();

static pSigFunc pReplacedFunc;

/*
 * ignoreSigPipe ()
 */
static void ignoreSigPipe (int param)
{
	if (pReplacedFunc) {
		(*pReplacedFunc) (param);
	}
}

/*
 * installSigPipeIgnore ()
 */
void installSigPipeIgnore (void)
{
	static int init;

	if (init) {
		return;
	}

	pReplacedFunc = signal (SIGPIPE, ignoreSigPipe);
	if (pReplacedFunc == SIG_ERR) {
		char *pFmt = "replace of SIGPIPE failed beacuse\n";
		fprintf (stderr, pFmt, __FILE__, strerror(errno));
	}

	init = 1;
}



