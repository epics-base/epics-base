


#include <tsSLList.h>
#include <assert.h>

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
	tsSLIter<fred>	iter(list);
	fred		*pFred;
	fred		*pFredII;
	fred		*pFredBack;
	tsSLList<jane>	janeList;
	tsSLIter<jane>	janeIter(janeList);
	jane		*pJane;

	pFred = new fred("A");
	pFredII = new fred("B");

	list.add(*pFred);
	list.add(*pFredII);
	pFredBack = list.first();
	assert(pFredBack == pFredII);
	list.remove(*pFred, *pFredII);
	list.add(*pFred);
	pFredBack = list.get();
	assert (pFredBack == pFred);
	pFredBack = list.get();
	assert (pFredBack == pFredII);
	list.add(*pFred);
	list.add(*pFredII);
	list.add(* new fred("C"));
	list.add(* new fred("D"));

	while (pFredBack = iter()) {
		pFredBack->show();
	}

	pJane = new jane("JA");
	janeList.add(*pJane);	
	pJane = new jane("JB");
	janeList.add(*pJane);	

	while (pJane = janeIter()) {
		pJane->show();
	}

	while (pFredBack = iter()) {
		pFredBack->show();
	}
}

