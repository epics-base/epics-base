/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/



#include "tsSLList.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>

/*
 * gnuc does not provide this under sunos4
 */
#if !defined(CLOCKS_PER_SEC) && defined(SUNOS4)
#	define CLOCKS_PER_SEC 1000000
#endif

class fred : public tsSLNode<fred> {
public:
	fred() : count(0) {}
	void inc () {count++;}
private:
	unsigned count;
};

class jane : public fred, public tsSLNode<jane> {
public:
	jane() {}
private:
};

#define LOOPCOUNT 100000

int main ()
{
	tsSLList<fred>	list;
	fred		*pFred;
	unsigned 	i;
	clock_t		clk;
	clock_t		diff;
	double 		delay;

	for (i=0; i<LOOPCOUNT; i++) {
		pFred = new fred();
		list.add(*pFred);
	}

	clk = clock();
	{
		tsSLIter <fred> iter ( list.firstIter () );
		while ( iter.valid () ) {
			iter->inc ();
            iter++;
		}
	}
	diff = clock() - clk;
	delay = diff;
	delay = delay/CLOCKS_PER_SEC;
	delay = delay/LOOPCOUNT;
	printf("delay = %15.10f\n", delay);

	pFred = new fred();
	clk = clock();
	{
		tsSLIter <fred>	iter ( list.firstIter () );
		for ( i=0; i<LOOPCOUNT; i++ ) {
			iter->inc();
		}
	}
	diff = clock() - clk;
	delay = diff;
	delay = delay/CLOCKS_PER_SEC;
	delay = delay/LOOPCOUNT;
	printf("delay = %15.10f\n", delay);

	return 0;
}

