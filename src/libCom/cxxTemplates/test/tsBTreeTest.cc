/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "tsBTree.h"

class A : public tsBTreeNode<A> {
public:
	A(const char *pNameIn) : pName(pNameIn) {}

	btCmp tsBTreeCompare(const A &item) const
	{
		int cmp = strcmp(this->pName, item.pName);
		if (cmp<=0) {
			return btLessOrEqual;
		}
		else {
			return btGreater;
		}
	}

	void show()
	{
		printf("A: %s\n", pName);
	}
private:
	const char *pName;
};

int main ()
{
	tsBTree<A> tree; 
	A a0 ("fred");
	A a1 ("jane");
	A a2 ("jane0");
	A a3 ("bill");
	A a4 ("jane");
	A a5 ("dan");
	A a6 ("joe");

	tree.insert (a0);
	tree.insert (a1);
	tree.insert (a2);
	tree.insert (a3);
	tree.insert (a4);
	tree.insert (a5);

	tree.traverse (&A::show);

	assert (tree.remove(a6)==tsbtrrNotFound);
	tree.insert (a6);
	assert (tree.remove(a6)==tsbtrrFound);
	assert (tree.remove(a5)==tsbtrrFound);
	assert (tree.remove(a5)==tsbtrrNotFound);
	assert (!tree.verify(a5));
	assert (tree.verify(a4));
	assert (tree.remove(a0)==tsbtrrFound);
	assert (!tree.verify(a0));
	assert (tree.remove(a0)==tsbtrrNotFound);
	tree.insert (a5);
	assert (tree.verify(a5));
	assert (tree.verify(a2));
	assert (tree.remove(a2)==tsbtrrFound);
	assert (!tree.verify(a2));
	assert (tree.remove(a2)==tsbtrrNotFound);
	assert (tree.verify(a5));
	assert (tree.remove(a5)==tsbtrrFound);
	assert (tree.remove(a5)==tsbtrrNotFound);
	assert (tree.remove(a0)==tsbtrrNotFound);
	assert (tree.remove(a4)==tsbtrrFound);
	assert (tree.remove(a3)==tsbtrrFound);
	assert (tree.remove(a4)==tsbtrrNotFound);
	assert (tree.remove(a1)==tsbtrrFound);

	tree.traverse (&A::show);

	return 0;
}


