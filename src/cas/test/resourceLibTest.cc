

#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <resourceLib.h>

class uintId {
public:
	uintId (unsigned idIn) : id(idIn) {}

	resourceTableID resourceHash(unsigned nBitsId) const
	{
		unsigned        src = this->id;
		resourceTableID	hashid;
	 
		hashid = src;
		src = src >> nBitsId;
		while (src) {
			hashid = hashid ^ src;
			src = src >> nBitsId;
		}
		//
		// the result here is always masked to the
		// proper size after it is returned to the resource class
		//
		return hashid;
	}

	int operator == (const uintId &idIn)
	{
		return this->id == idIn.id;
	}
private:
	unsigned const id;
};

class fred : public uintId, tsSLNode<fred> {
public:
	fred (const char *pNameIn, unsigned idIn) : 
			pName(pNameIn), uintId(idIn) {}
private:
	const char * const pName;
};

main()
{
        unsigned        		i;
        clock_t         		start, finish;
        double          		duration;
        const int       		LOOPS = 500000;
	resourceTable<fred,uintId>	tbl;	
	fred				fred1("fred1",0x1000a432);
	fred				fred2("fred2",0x0000a432);
	fred				*pFred;
	uintId				id1(0x1000a432);
	uintId				id2(0x0000a432);
	int				status;
 
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

