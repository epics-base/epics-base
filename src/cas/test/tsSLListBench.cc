


#include <tsSLList.h>
#include <assert.h>
#include <time.h>

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

main ()
{
	tsSLList<fred>	list;
	tsSLIter<fred>	iter(list);
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
	iter = list;
	while (pFred = iter()) {
		pFred->inc();
	}
	diff = clock() - clk;
	delay = diff;
	delay = delay/CLOCKS_PER_SEC;
	delay = delay/LOOPCOUNT;
	printf("delay = %15.10lf\n", delay);

	pFred = new fred();
	clk = clock();
	iter = list;
	for (i=0; i<LOOPCOUNT; i++) {
		pFred->inc();
	}
	diff = clock() - clk;
	delay = diff;
	delay = delay/CLOCKS_PER_SEC;
	delay = delay/LOOPCOUNT;
	printf("delay = %15.10lf\n", delay);
}

