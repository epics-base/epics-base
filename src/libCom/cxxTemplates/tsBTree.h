/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef assert // allow use of epicsAssert.h
#include <assert.h>
#endif
#include "locationException.h"

//
// tsBTreeRMRet
//
enum tsbtRR {tsbtrrNotFound, tsbtrrFound};
template <class T>
class tsBTreeRMRet {
public:
	tsBTreeRMRet (tsbtRR foundItIn, T *pNewSegIn) :
		foundIt(foundItIn), pNewSeg(pNewSegIn) {}

	operator tsbtRR ()
	{
		return this->foundIt;
	}

	const tsbtRR	foundIt;
	T * const 	pNewSeg;
};

template <class T> class tsBTree;

//
// tsBTreeNode
//
template <class T> 
class tsBTreeNode
{
friend class tsBTree<T>;
public:
	//
	// exceptions
	//
	class invalid_btCmp {};


	//
	// when someone copies into a class deriving from this
	// do _not_ change the node pointers
	//
	void operator = (tsBTreeNode<T> &) {}

	enum btCmp {btGreater, btLessOrEqual};

	//
	// class T must supply this member function which 
	// comapres *this with item 
	//
	// returns:
	// btGreater		*this is greater than item 
	// btLessOrEqual	*this is less than or equal to item
	//
	// btCmp tsBTreeCompare(const T &item) const;
	//

private:
	T *pLeft;
	T *pRight;

	//
	// run callback for evey item in the B-Treee in sort order
	//
	static void traverse (T &item, void (T::*pCallBack)()) 
	{
		tsBTreeNode<T> &node = item;
		if (node.pLeft) {
			tsBTreeNode<T>::traverse (*node.pLeft, pCallBack);
		}
		(item.*pCallBack)();
		if (node.pRight) {
			tsBTreeNode<T>::traverse (*node.pRight, pCallBack);
		}
	}

	//
	// NOTE: 
	// no change to item.pLeft and item.pRight here
	// so that an segment of a tree can be inserted
	//
	static void insert(T &self, T &item)
	{
		tsBTreeNode<T> &node = self;
		btCmp result = item.tsBTreeCompare(self);
		if (result==btLessOrEqual) {
			if (node.pLeft) {
				tsBTreeNode<T>::insert (*node.pLeft, item);
			}
			else {
				node.pLeft = &item;
			}
		}
		else if(result==btGreater) {
			if (node.pRight) {
				tsBTreeNode<T>::insert (*node.pRight, item);
			}
			else {
				node.pRight = &item;
			}
		}
		else {
            throwWithLocation ( invalid_btCmp () );
		}
	}

	//
	// remove()
	// returns pointer to modified tree and found/not found
	// (NULL if this portion of the tree is empty)
	//
	static tsBTreeRMRet<T> remove(T &self, T &item)
	{
		tsBTreeNode<T> &node = self;
		if (&self == &item) {
			if (node.pLeft) {
				if (node.pRight) {
					tsBTreeNode<T> *pLeftNode = node.pLeft;
					T *pR = pLeftNode->pRight;
					if (pR) {
						tsBTreeNode<T>::insert (*pR, *node.pRight);
					}
					else {
						pLeftNode->pRight = node.pRight;
					}
				}
				return tsBTreeRMRet<T>(tsbtrrFound, node.pLeft); // found it
			}
			else {
				return tsBTreeRMRet<T>(tsbtrrFound, node.pRight); // found it
			}
		}

		btCmp result = item.tsBTreeCompare(self);
		if (result==btLessOrEqual) {
			if (node.pLeft) {
				tsBTreeRMRet<T> ret = tsBTreeNode<T>::remove(*node.pLeft, item);
				if (ret.foundIt==tsbtrrFound) {
					node.pLeft= ret.pNewSeg;
					return tsBTreeRMRet<T> (tsbtrrFound, &self); // TRUE - found it
				}
			}
			return tsBTreeRMRet<T>(tsbtrrNotFound, 0u); // not found
		}
		else if(result==btGreater) {
			if (node.pRight) {
				tsBTreeRMRet<T> ret = tsBTreeNode<T>::remove(*node.pRight, item);
				if (ret.foundIt==tsbtrrFound) {
					node.pRight = ret.pNewSeg;
					return tsBTreeRMRet<T>(tsbtrrFound,&self); // TRUE - found it
				}
			}
			return tsBTreeRMRet<T>(tsbtrrNotFound, 0u); // not found
		}
		else {
			return tsBTreeRMRet<T>(tsbtrrNotFound, 0u); // not found
		}
	}
	//
	// verify
	//
	static unsigned verify(const T &self, const T &item) 
	{
		const tsBTreeNode<T> &node = self;

		if (&self == &item) {
			return 1u; // TRUE -item is present
		}
		btCmp result = item.tsBTreeCompare(self);
		if (result==btLessOrEqual) {
			if (node.pLeft) {
				return tsBTreeNode<T>::verify (*node.pLeft, item);
			}
			else {
				return 0u; // FALSE - not found
			}
		}
		else if(result==btGreater) {
			if (node.pRight) {
				return tsBTreeNode<T>::verify (*node.pRight, item);
			}
			else {
				return 0u; // FALSE - not found
			}
		}
		else {
			return 0u; // FALSE - not found
		}
	}
};

//
// tsBTree
//
template <class T>
class tsBTree
{
public:
	tsBTree() : pRoot(0u) {}

//	~tsBTree()  
//	{
//		this->traverse(T::~T);
//	}

	void insert(T &item)
	{
		tsBTreeNode<T> &node = item;
		node.pLeft = 0;
		node.pRight = 0;
		if (this->pRoot) {
			tsBTreeNode<T>::insert(*this->pRoot, item);
		}
		else {
			this->pRoot = &item;
		}
	}
	//
	// remove item from the tree
	//
	// returns true if item was in the tree
	// (otherwise FALSE)
	//
	unsigned remove(T &item)
	{
		if (this->pRoot) {
			tsBTreeRMRet<T> ret = 
				tsBTreeNode<T>::remove(*this->pRoot, item);
			if (ret.foundIt) {
				this->pRoot = ret.pNewSeg;
				return 1u; // TRUE - found it
			}
		}
		return 0u; // FALSE - not found
	}
	//
	// verify that item is in the tree
	//
	// returns true if item is in the tree
	// (otherwise FALSE)
	//
	unsigned verify(T &item) const
	{
		if (this->pRoot) {
			return tsBTreeNode<T>::verify(*this->pRoot, item);
		}
		else {
			return 0u; // FALSE - not found
		}
	}
    //
    // Call (pT->*pCB) () for each item in the table
    //
    // where pT is a pointer to type T and pCB is
    // a pointer to a memmber function of T with
    // no parameters and returning void
    //
    void traverse(void (T::*pCB)()) 
	{
		if (this->pRoot) {
			tsBTreeNode<T>::traverse(*this->pRoot, pCB);
		}
	}
private:
	T *pRoot;
};

