

#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#define epicsExportSharedSymbols
#define instantiateRecourceLib
#include "resourceLib.h"

void empty();

class albert : public intId<unsigned,8>, public tsSLNode<albert> {
public:
	albert (resTable< albert, intId<unsigned,8> > &atIn, unsigned idIn) : 
		intId<unsigned,8>(idIn), at(atIn) 
    {
        assert (at.add (*this)==0);
    }
	void show (unsigned /* level */) 
	{
	}
	void destroy()
	{
		at.remove(*this);
		delete this;
	}
private:
	resTable< albert, intId<unsigned,8> > &at;
};

class fred : public intId<unsigned,8>, public tsSLNode<fred> {
public:
	fred (const char *pNameIn, unsigned idIn) : 
			intId<unsigned,8>(idIn), pName(pNameIn) {}
	void show (unsigned) 
	{
		printf("fred %s\n", pName);
	}
	void destroy()
	{
		// always on stack so noop
	}
private:
	const char * const pName;
};

class jane : public stringId, public tsSLNode<jane> {
public:
	jane (const char *pNameIn) : stringId (pNameIn) {}

	void testTraverse();

	void destroy()
	{
		// always on stack so noop
	}
};

//
// jane::testTraverse()
//
void jane::testTraverse()
{
	printf("Traverse Test\n");
	this->show(10);
}

//
// explicitly instantiates on compilers that support this
//
#if defined(EXPL_TEMPL)
        //
        // From Stroustrups's "The C++ Programming Language"
        // Appendix A: r.14.9
        //
        // This explicitly instantiates the template class's member
        // functions into "templInst.o"
        //
	template class resTable<fred,intId<unsigned,8> >;	
	template class resTable<jane,stringId>;	
#endif

int main()
{
	unsigned i;
	clock_t start, finish;
	double duration;
	const unsigned LOOPS = 50000;
	resTable<fred, intId<unsigned,8> > intTbl (8);	
	resTable<jane, stringId> strTbl (8);	
	fred fred0("fred0",0);
	fred fred1("fred1",0x1000a432);
	fred fred2("fred2",0x0000a432);
	fred fred3("fred3",1);
	fred fred4("fred4",2);
	fred fred5("fred5",3);
	fred fred6("fred6",4);
	fred fred7("fred7",5);
	fred fred8("fred8",6);
	fred fred9("fred9",7);
	jane jane1("rrrrrrrrrrrrrrrrrrrrrrrrrr1");
	jane jane2("rrrrrrrrrrrrrrrrrrrrrrrrrr2");
	fred *pFred;
	jane *pJane;
	intId<unsigned,8> intId0 (0);
	intId<unsigned,8> intId1 (0x1000a432);
	intId<unsigned,8> intId2 (0x0000a432);
	intId<unsigned,8> intId3 (1);
	intId<unsigned,8> intId4 (2);
	intId<unsigned,8> intId5 (3);
	intId<unsigned,8> intId6 (4);
	intId<unsigned,8> intId7 (5);
	intId<unsigned,8> intId8 (6);
	intId<unsigned,8> intId9 (7);

	stringId strId1("rrrrrrrrrrrrrrrrrrrrrrrrrr1");
	stringId strId2("rrrrrrrrrrrrrrrrrrrrrrrrrr2");

	assert (intTbl.add(fred0)==0);
	assert (intTbl.add(fred1)==0);
	assert (intTbl.add(fred2)==0);
	assert (intTbl.add(fred3)==0);
	assert (intTbl.add(fred4)==0);
	assert (intTbl.add(fred5)==0);
	assert (intTbl.add(fred6)==0);
	assert (intTbl.add(fred7)==0);
	assert (intTbl.add(fred8)==0);
	assert (intTbl.add(fred9)==0);

	start = clock();
	for (i=0; i<LOOPS; i++) {
		pFred = intTbl.lookup(intId1);	
		assert(pFred==&fred1);
		pFred = intTbl.lookup(intId2);	
		assert(pFred==&fred2);
		pFred = intTbl.lookup(intId3);	
		assert(pFred==&fred3);
		pFred = intTbl.lookup(intId4);	
		assert(pFred==&fred4);
		pFred = intTbl.lookup(intId5);	
		assert(pFred==&fred5);
		pFred = intTbl.lookup(intId6);	
		assert(pFred==&fred6);
		pFred = intTbl.lookup(intId7);	
		assert(pFred==&fred7);
		pFred = intTbl.lookup(intId8);	
		assert(pFred==&fred8);
		pFred = intTbl.lookup(intId9);	
		assert(pFred==&fred9);
		pFred = intTbl.lookup(intId0);	
		assert(pFred==&fred0);
	}
	finish = clock();

	duration = finish-start;
	duration /= CLOCKS_PER_SEC;
	printf("It took %15.10f total sec for integer hash lookups\n", duration);
	duration /= LOOPS;
	duration /= 10;
	duration *= 1e6;
	printf("It took %15.10f u sec per integer hash lookup\n", duration);
 
	intTbl.show(10u);

	intTbl.remove(intId1);
	intTbl.remove(intId2);
    
	assert (strTbl.add(jane1)==0);
	assert (strTbl.add(jane2)==0);

	start = clock();
	for(i=0; i<LOOPS; i++){
		pJane = strTbl.lookup(strId1);	
		assert(pJane==&jane1);
		pJane = strTbl.lookup(strId1);	
		assert(pJane==&jane1);
		pJane = strTbl.lookup(strId1);	
		assert(pJane==&jane1);
		pJane = strTbl.lookup(strId1);	
		assert(pJane==&jane1);
		pJane = strTbl.lookup(strId1);	
		assert(pJane==&jane1);
		pJane = strTbl.lookup(strId1);	
		assert(pJane==&jane1);
		pJane = strTbl.lookup(strId1);	
		assert(pJane==&jane1);
		pJane = strTbl.lookup(strId1);	
		assert(pJane==&jane1);
		pJane = strTbl.lookup(strId1);	
		assert(pJane==&jane1);
		pJane = strTbl.lookup(strId2);	
		assert(pJane==&jane2);
	}
	finish = clock();

	duration = finish-start;
	duration /= CLOCKS_PER_SEC;
	printf("It took %15.10f total sec for string hash lookups\n", duration);
	duration /= LOOPS;
	duration /= 10;
	duration *= 1e6;
	printf("It took %15.10f u sec per string hash lookup\n", duration);
 
	strTbl.show(10u);
	strTbl.traverse(&jane::testTraverse);

	strTbl.remove(strId1);
	strTbl.remove(strId2);

	start = clock();
	for(i=0; i<LOOPS; i++){
		empty();
		empty();
		empty();
		empty();
		empty();
		empty();
		empty();
		empty();
		empty();
		empty();
	}
	finish = clock();
	duration = finish-start;
	duration /= CLOCKS_PER_SEC;
	printf("It took %15.10f total sec for empty subroutines\n", duration);
	duration /= LOOPS;
	duration /= 10;
	duration *= 1e6;
	printf("It took %15.10f u sec to call an empty subroutine\n", duration);
    
	//
	// hash distribution test
	//
    static const unsigned tableSize = 0x1000;
	resTable< albert, intId<unsigned,8> > alTbl (tableSize);	

	for (i=0; i<tableSize*8; i++) {
		albert *pa = new albert (alTbl, i);
		assert (pa);
	}
	alTbl.show(1u);

    resTableIter< albert, intId<unsigned,8> > alTblIter (alTbl);
    albert *pa;
    i=0;
    while ( (pa = alTblIter.next()) ) {
        i++;
    }
    assert (i==tableSize*8);

	return 0;
}

void empty()
{
}

