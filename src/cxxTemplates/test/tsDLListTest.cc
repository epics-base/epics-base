


#include <tsDLList.h>
#include <assert.h>
#include <stdio.h>

class fred : public tsDLNode<fred> {
public:
	fred(const char * const pNameIn) : pName(pNameIn){}
	void show () {printf("%s\n", pName);}
private:
	const char * const pName;
};

class jane : public fred, public tsDLNode<jane> {
public:
	jane(const char * const pNameIn) : fred(pNameIn){}
private:
};

main ()
{
	tsDLList<fred>		list;
	tsDLFwdIter<fred>	iter(list);
	fred			*pFred;
	fred			*pFredII;
	fred			*pFredBack;
	tsDLList<jane>		janeList;
	tsDLFwdIter<jane>	janeFwdIter(janeList);
	tsDLBwdIter<jane>	janeBwdIter(janeList);
	jane			*pJane;

	pFred = new fred("A");
	pFredII = new fred("B");

	list.add(*pFred);
	list.add(*pFredII);
	pFredBack = iter();
	assert(pFredBack == pFred);
	pFredBack = iter();
	assert(pFredBack == pFredII);
	list.remove(*pFred);
	list.add(*pFred);
	pFredBack = list.get();
	assert (pFredBack == pFredII);
	pFredBack = list.get();
	assert (pFredBack == pFred);
	assert (list.count() == 0u);
	list.add(*pFred);
	list.add(*pFredII);
	list.add(* new fred("C"));
	list.add(* new fred("D"));

	iter.reset();
	while (pFredBack = iter()) {
		pFredBack->show();
	}

	pJane = new jane("JA");
	janeList.add(*pJane);	
	pJane = new jane("JB");
	janeList.add(*pJane);	

	while (pJane = janeFwdIter()) {
		pJane->show();
	}

	while (pJane = janeBwdIter()) {
		pJane->show();
	}

	iter = list;
	while (pFredBack = iter()) {
		pFredBack->show();
	}

	iter = list;
	while (pFredBack = iter()) {
		iter.remove();
	}
	assert(list.count()==0);

	janeFwdIter = janeList;
	while (pFredBack = janeFwdIter()) {
		janeFwdIter.remove();
	}
	assert(janeList.count()==0);

	pJane = new jane("JA");
	janeList.add(*pJane);	
	pJane = new jane("JB");
	janeList.add(*pJane);	
	janeBwdIter = janeList;
	while (pFredBack = janeBwdIter()) {
		janeBwdIter.remove();
	}
	assert(janeList.count()==0);
}

