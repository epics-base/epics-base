


#include <tsSLList.h>
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

main ()
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
		tsSLIter<fred> iter(list);
		pFredBack = iter();
		assert(pFredBack == pFredII);
	}
	list.remove(*pFredII);  // removes *pFred !!
	list.add(*pFred);
	pFredBack = list.get();
	assert (pFredBack == pFred);
	pFredBack = list.get();
	assert (pFredBack == pFredII);
	list.add(*pFredII);
	list.add(*pFred);
	{
		tsSLIterRm<fred> iter(list);
		while ( (pFredBack = iter()) ) {
			iter.remove();
		}
	}
	pFredBack = list.get();
	assert (pFredBack == 0);
	list.add(*pFred);
	list.add(*pFredII);
	list.add(* new fred("C"));
	list.add(* new fred("D"));

	{
		tsSLIter<fred> iter(list);
		while ( (pFredBack = iter()) ) {
			pFredBack->show();
		}
	}

	pJane = new jane("JA");
	janeList.add(*pJane);	
	pJane = new jane("JB");
	janeList.add(*pJane);	

	{
		tsSLIter<jane>	janeIter(janeList);
		while ( (pJane = janeIter()) ) {
			pJane->show();
		}
	}

	{
		tsSLIter<fred> iter(list);
		while ( (pFredBack = iter()) ) {
			pFredBack->show();
		}
	}

	{
		tsSLIterRm<fred> iter(list);
		while ( (pFredBack = iter()) ) {
			iter.remove();
		}
	}

	{
		tsSLIter<fred> iter(list);
		pFredBack = iter();
		assert(pFredBack==NULL);
	}
}

