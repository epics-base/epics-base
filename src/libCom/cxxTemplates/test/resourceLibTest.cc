

#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <resourceLib.h>

#ifdef SUNOS4
#ifndef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC 1000000
#endif
#endif

class fred : public uintId, tsSLNode<fred> {
public:
	fred (const char *pNameIn, unsigned idIn) : 
			pName(pNameIn), uintId(idIn) {}
	void show (unsigned) 
	{
		printf("fred %s\n", pName);
	}
private:
	const char * const pName;
};

main()
{
        unsigned        	i;
        clock_t         	start, finish;
        double          	duration;
        const int       	LOOPS = 500000;
	resTable<fred,uintId>	tbl;	
	fred			fred1("fred1",0x1000a432);
	fred			fred2("fred2",0x0000a432);
	fred			*pFred;
	uintId			id1(0x1000a432);
	uintId			id2(0x0000a432);
	int			status;
 
	status = tbl.init(8);
	if (status) {
		return -1;
	}
 
        status = tbl.add(fred1);
        assert (!status);
        status = tbl.add(fred2);
        assert (!status);
 
        start = clock();
        for(i=0; i<LOOPS; i++){
		pFred = tbl.lookup(id1);	
		assert(pFred==&fred1);
		pFred = tbl.lookup(id1);	
		assert(pFred==&fred1);
		pFred = tbl.lookup(id1);	
		assert(pFred==&fred1);
		pFred = tbl.lookup(id1);	
		assert(pFred==&fred1);
		pFred = tbl.lookup(id1);	
		assert(pFred==&fred1);
		pFred = tbl.lookup(id1);	
		assert(pFred==&fred1);
		pFred = tbl.lookup(id1);	
		assert(pFred==&fred1);
		pFred = tbl.lookup(id1);	
		assert(pFred==&fred1);
		pFred = tbl.lookup(id1);	
		assert(pFred==&fred1);
		pFred = tbl.lookup(id2);	
		assert(pFred==&fred2);
        }
        finish = clock();
 
        duration = finish-start;
        duration /= CLOCKS_PER_SEC;
        printf("It took %15.10f total sec\n", duration);
        duration /= LOOPS;
        duration /= 10;
	duration *= 1e6;
        printf("It took %15.10f u sec per hash lookup\n", duration);
 
	tbl.show(10u);

	tbl.remove(id1);
	tbl.remove(id2);
 
        return 0;
}

