/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/


#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define epicsExportSharedSymbols
#define instantiateRecourceLib
#include "resourceLib.h"

#if defined(__GNUC__) && ( __GNUC__<2 || (__GNUC__==2 && __GNUC__<8) )
typedef intId<unsigned,8,16> testIntId;
#else
typedef intId<unsigned,8> testIntId;
#endif

#define verify(exp) ((exp) ? (void)0 : \
    epicsAssert(__FILE__, __LINE__, #exp, epicsAssertAuthor))

static void empty()
{
}

class albert : public testIntId, public tsSLNode<albert> {
public:
	albert (resTable< albert, testIntId > &atIn, unsigned idIn) : 
		testIntId(idIn), at(atIn) 
    {
        verify (at.add (*this)==0);
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
	resTable< albert, testIntId > &at;
};

class fred : public testIntId, public tsSLNode<fred> {
public:
	fred (const char *pNameIn, unsigned idIn) : 
			testIntId(idIn), pName(pNameIn) {}
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

int main()
{
	unsigned i;
	clock_t start, finish;
	double duration;
	const unsigned LOOPS = 500000;
	resTable < fred, testIntId > intTbl;	
	resTable < jane, stringId> strTbl;	
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
	testIntId intId0 (0);
	testIntId intId1 (0x1000a432);
	testIntId intId2 (0x0000a432);
	testIntId intId3 (1);
	testIntId intId4 (2);
	testIntId intId5 (3);
	testIntId intId6 (4);
	testIntId intId7 (5);
	testIntId intId8 (6);
	testIntId intId9 (7);

	stringId strId1("rrrrrrrrrrrrrrrrrrrrrrrrrr1");
    strTbl.verify ();
	stringId strId2("rrrrrrrrrrrrrrrrrrrrrrrrrr2");
    strTbl.verify ();

	intTbl.setTableSize ( 100000 );

	verify (intTbl.add(fred0)==0);
    intTbl.verify ();
	verify (intTbl.add(fred1)==0);
    intTbl.verify ();
	verify (intTbl.add(fred2)==0);
    intTbl.verify ();
	verify (intTbl.add(fred3)==0);
    intTbl.verify ();
	verify (intTbl.add(fred4)==0);
    intTbl.verify ();

	intTbl.setTableSize ( 200000 );

	verify (intTbl.add(fred5)==0);
    intTbl.verify ();
	verify (intTbl.add(fred6)==0);
    intTbl.verify ();
	verify (intTbl.add(fred7)==0);
    intTbl.verify ();
	verify (intTbl.add(fred8)==0);
    intTbl.verify ();
	verify (intTbl.add(fred9)==0);
    intTbl.verify ();

	start = clock();
	for (i=0; i<LOOPS; i++) {
		pFred = intTbl.lookup(intId1);	
		verify(pFred==&fred1);
		pFred = intTbl.lookup(intId2);	
		verify (pFred==&fred2);
		pFred = intTbl.lookup(intId3);	
		verify (pFred==&fred3);
		pFred = intTbl.lookup(intId4);	
		verify (pFred==&fred4);
		pFred = intTbl.lookup(intId5);	
		verify (pFred==&fred5);
		pFred = intTbl.lookup(intId6);	
		verify (pFred==&fred6);
		pFred = intTbl.lookup(intId7);	
		verify (pFred==&fred7);
		pFred = intTbl.lookup(intId8);	
		verify (pFred==&fred8);
		pFred = intTbl.lookup(intId9);	
		verify (pFred==&fred9);
		pFred = intTbl.lookup(intId0);	
		verify (pFred==&fred0);
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
	pFred = intTbl.lookup(intId1);	
	verify (pFred==0);
	pFred = intTbl.lookup(intId2);	
	verify (pFred==0);
    
	verify (strTbl.add(jane1)==0);
	verify (strTbl.add(jane2)==0);

	start = clock();
	for(i=0; i<LOOPS; i++){
		pJane = strTbl.lookup(strId1);	
		verify (pJane==&jane1);
		pJane = strTbl.lookup(strId1);	
		verify (pJane==&jane1);
		pJane = strTbl.lookup(strId1);	
		verify (pJane==&jane1);
		pJane = strTbl.lookup(strId1);	
		verify (pJane==&jane1);
		pJane = strTbl.lookup(strId1);	
		verify (pJane==&jane1);
		pJane = strTbl.lookup(strId1);	
		verify (pJane==&jane1);
		pJane = strTbl.lookup(strId1);	
		verify (pJane==&jane1);
		pJane = strTbl.lookup(strId1);	
		verify (pJane==&jane1);
		pJane = strTbl.lookup(strId1);	
		verify (pJane==&jane1);
		pJane = strTbl.lookup(strId2);	
		verify (pJane==&jane2);
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
        strlen ( "0" );
        strlen ( "1" );
        strlen ( "2" );
        strlen ( "3" );
        strlen ( "4" );
        strlen ( "5" );
        strlen ( "6" );
        strlen ( "7" );
        strlen ( "8" );
        strlen ( "9" );
	}
	finish = clock();
	duration = finish-start;
	duration /= CLOCKS_PER_SEC;
	printf("It took %15.10f total sec for minimal subroutines\n", duration);
	duration /= LOOPS;
	duration /= 10;
	duration *= 1e6;
	printf("It took %15.10f u sec to call a minimal subroutine\n", duration);
    
	//
	// hash distribution test
	//
    static const unsigned elementCount = 0x8000;
    albert *pAlbert[elementCount];
	resTable< albert, testIntId > alTbl;	

	for (i=0; i<elementCount; i++) {
		pAlbert[i] = new albert (alTbl, i);
		verify ( pAlbert[i] );
	}
	alTbl.verify ();
	alTbl.show (1u);

    resTableIter < albert, testIntId > alTblIter ( alTbl.firstIter() );
    albert *pa;
    i=0;
    while ( ( pa = alTblIter.pointer() ) ) {
        i++;
        alTblIter++;
    }
    verify ( i == elementCount );
	alTbl.verify ();

	for ( i = 0; i < elementCount; i++ ) {
		verify ( pAlbert[i] == alTbl.lookup( pAlbert[i]->getId() ) );
	}
	alTbl.verify ();

    for ( i = 0; i < elementCount; i += 2 ) {
        verify ( pAlbert[i] == alTbl.remove ( pAlbert[i]->getId() ) );
	}
	alTbl.verify ();

    for ( i = 0; i < elementCount; i += 2 ) {
        verify ( 0 == alTbl.lookup ( pAlbert[i]->getId() ) );
	}
	alTbl.verify ();

    for ( i = 1; i < elementCount; i += 2 ) {
        verify ( pAlbert[i] == alTbl.lookup ( pAlbert[i]->getId() ) );
 	}
	alTbl.verify ();

	return 0;
}



