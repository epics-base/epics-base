
#include <tsDLList.h>
#include <assert.h>
#include <time.h>
#include <stdio.h>

class fred : public tsDLNode<fred> {
public:
	fred() : count(0) {}
	void inc () {count++;}
private:
	unsigned count;
};

class jane : public fred, public tsDLNode<jane> {
public:
	jane() {}
private:
};

#define LOOPCOUNT 100000

int main ()
{
	tsDLList<fred>		list;
	tsDLFwdIter<fred>	iter(list);
	fred			*pFred;
	unsigned 		i;
	clock_t			clk;
	clock_t			diff;
	double 			delay;

	for (i=0; i<LOOPCOUNT; i++) {
		pFred = new fred();
		list.add(*pFred);
	}

	clk = clock();
	while ( (pFred = iter()) ) {
		pFred->inc();
	}
	diff = clock() - clk;
	delay = diff;
	delay = delay/CLOCKS_PER_SEC;
	delay = delay/LOOPCOUNT;
	printf("delay = %15.10f\n", delay);

	pFred = new fred();
	clk = clock();
	for (i=0; i<LOOPCOUNT; i++) {
		pFred->inc();
	}
	diff = clock() - clk;
	delay = diff;
	delay = delay/CLOCKS_PER_SEC;
	delay = delay/LOOPCOUNT;
	printf("delay = %15.10f\n", delay);

	return 0;
}

