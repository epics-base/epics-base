


#include "tsSLList.h"
#include <assert.h>
#include <stdio.h>

class fred : public tsSLNode<fred> {
public:
	fred(const char * const pNameIn) : pName(pNameIn){}
	void show () {printf("%s\n", pName);}
private:
	const char * const pName;
};

class jane : public fred, public tsSLNode<jane> {
public:
	jane(const char * const pNameIn) : fred(pNameIn){}
private:
};

int main ()
{
	tsSLList<fred>	list;
	fred		*pFred;
	fred		*pFredII;
	fred		*pFredBack;
	tsSLList<jane>	janeList;
	jane		*pJane;

	pFred = new fred("A");
	pFredII = new fred("B");

	list.add(*pFred);
	list.add(*pFredII);
	{
        tsSLIter<fred> iter1( pFredII );
		tsSLIter<fred> iter2( list.first () );
		tsSLIter<fred> iter3( pFred );
		assert ( iter1 == iter2 );
		iter2++;
		assert ( iter3 == iter2 );
        list.remove ( *pFredII ); // removes pFred
	}
	list.add ( *pFred );
	pFredBack = list.get();
	assert (pFredBack == pFred);
	pFredBack = list.get();
	assert (pFredBack == pFredII);
	list.add(*pFredII);
	list.add(*pFred);
    while ( list.get () );
	pFredBack = list.get();
	assert (pFredBack == 0);
	list.add(*pFred);
	list.add(*pFredII);
	list.add(* new fred("C"));
	list.add(* new fred("D"));

	{
		tsSLIter<fred> iter ( list.first() );
		while ( iter ) {
			iter->show();
            ++iter;
		}
	}

	pJane = new jane("JA");
	janeList.add(*pJane);	
	pJane = new jane("JB");
	janeList.add(*pJane);	

	{
		tsSLIter<jane> iter ( janeList.first() );
		while ( iter ) {
			iter->show();
            ++iter;
		}
	}

	{
		tsSLIter <fred> iter ( list.first () );
		while ( iter ) {
			iter->show();
            iter++;
		}
	}

    while ( list.get () );

	{
		tsSLIter<fred> iter (list.first());
        assert ( ! iter );
	}

	return 0;
}

