/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include "tsBTree.h"
#include "tsSLList.h"

class A : public tsBTreeNode<A>, public tsSLNode<A> {
public:
	A() 
	{
		key = (unsigned) rand();
	}

	btCmp tsBTreeCompare(const A &item) const
	{
		if (this->key<=item.key) {
			return btLessOrEqual;
		}
		else {
			return btGreater;
		}
	}

	void show()
	{
		printf("A: %u\n", key);
	}
private:
	unsigned key;
};

#define LOOPCOUNT 10000u

int main ()
{
	unsigned i;
	tsBTree<A> tree; 
	tsSLList<A> list;
	A *pA;
	A a;
        clock_t clk;
        clock_t diff;
        double delay;
	
	tree.insert (a);
	list.add (a);

	for (i=0u; i<LOOPCOUNT; i++) {
		pA = new A;
		assert (pA);
		tree.insert (*pA);
		list.add (*pA);
	}

	clk = clock ();
	for (i=0u; i<LOOPCOUNT; i++) {
        bool success = tree.verify(a);
		assert ( success );
	}
	diff = clock () - clk;
        delay = diff;
        delay = delay/CLOCKS_PER_SEC;
        delay = delay/LOOPCOUNT;
        printf ("delay = %15.10f\n", delay);

	clk = clock ();
	while ( ( pA = list.get () ) ) {
        bool success = tree.remove(*pA);
		assert ( success );
	}
	diff = clock () - clk;
    delay = diff;
    delay = delay/CLOCKS_PER_SEC;
    delay = delay/LOOPCOUNT;
    printf ("delay = %15.10f\n", delay);

	tree.traverse (&A::show);

	return 0;
}


