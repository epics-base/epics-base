
#include <assert.h>

//
// tsBTreeRMRet
//
template <class T>
class tsBTreeRMRet {
public:
	tsBTreeRMRet (unsigned foundItIn, T *pNewSegIn) :
		foundIt(foundItIn), pNewSeg(pNewSegIn) {}

	const unsigned	foundIt;
	T * const 	pNewSeg;
};

//
// tsBTreeNode
//
template <class T> 
class tsBTreeNode
{
friend class tsBTree<T>;
public:
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

	static void traverse(T &self, void (T::*pCB)()) 
	{
		if (self.tsBTreeNode<T>::pLeft) {
			tsBTreeNode<T>::traverse
				(*self.tsBTreeNode<T>::pLeft, pCB);
		}
		(self.*pCB)();
		if (self.tsBTreeNode<T>::pRight) {
			tsBTreeNode<T>::traverse
				(*self.tsBTreeNode<T>::pRight, pCB);
		}
	}

	//
	// NOTE: 
	// no change to item.pLeft and item.pRight here
	// so that an segment of a tree can be inserted
	//
	static void insert(T &self, T &item)
	{
		btCmp result = item.tsBTreeCompare(self);
		if (result==btLessOrEqual) {
			if (self.tsBTreeNode<T>::pLeft) {
				tsBTreeNode<T>::insert
					(*self.tsBTreeNode<T>::pLeft, item);
			}
			else {
				self.tsBTreeNode<T>::pLeft = &item;
			}
		}
		else if(result==btGreater) {
			if (self.tsBTreeNode<T>::pRight) {
				tsBTreeNode<T>::insert
					(*self.tsBTreeNode<T>::pRight, item);
			}
			else {
				self.tsBTreeNode<T>::pRight = &item;
			}
		}
		else {
			assert(0);
		}
	}

	//
	// remove()
	// returns pointer to modified tree and found/not found
	// (NULL if this portion of the tree is empty)
	//
	static tsBTreeRMRet<T> remove(T &self, T &item)
	{
		if (&self == &item) {
			if (self.tsBTreeNode<T>::pLeft) {
				if (self.tsBTreeNode<T>::pRight) {
					T *pR = self.tsBTreeNode<T>::pLeft->tsBTreeNode<T>::pRight;
					if (pR) {
						tsBTreeNode<T>::insert
							(*pR, *self.tsBTreeNode<T>::pRight);
					}
					else {
						self.tsBTreeNode<T>::pLeft->tsBTreeNode<T>::pRight =
							self.tsBTreeNode<T>::pRight;
					}
				}
				return tsBTreeRMRet<T>(1u, self.tsBTreeNode<T>::pLeft); // found it
			}
			else {
				return tsBTreeRMRet<T>(1u, self.tsBTreeNode<T>::pRight); // found it
			}
		}

		btCmp result = item.tsBTreeCompare(self);
		if (result==btLessOrEqual) {
			if (self.tsBTreeNode<T>::pLeft) {
				tsBTreeRMRet<T> ret = tsBTreeNode<T>::
					remove(*self.tsBTreeNode<T>::pLeft, item);
				if (ret.foundIt) {
					self.tsBTreeNode<T>::pLeft= ret.pNewSeg;
					return tsBTreeRMRet<T>(1u,&self); // TRUE - found it
				}
			}
			return tsBTreeRMRet<T>(0u, 0u); // not found
		}
		else if(result==btGreater) {
			if (self.tsBTreeNode<T>::pRight) {
				tsBTreeRMRet<T> ret = tsBTreeNode<T>::
					remove(*self.tsBTreeNode<T>::pRight, item);
				if (ret.foundIt) {
					self.tsBTreeNode<T>::pRight = ret.pNewSeg;
					return tsBTreeRMRet<T>(1u,&self); // TRUE - found it
				}
			}
			return tsBTreeRMRet<T>(0u, 0u); // not found
		}
		else {
			assert(0);
		}
	}
	//
	// verify
	//
	static unsigned verify(const T &self, const T &item) 
	{
		if (&self == &item) {
			return 1u; // TRUE -item is present
		}
		btCmp result = item.tsBTreeCompare(self);
		if (result==btLessOrEqual) {
			if (self.tsBTreeNode<T>::pLeft) {
				return tsBTreeNode<T>::verify
					(*self.tsBTreeNode<T>::pLeft, item);
			}
			else {
				return 0u; // FALSE - not found
			}
		}
		else if(result==btGreater) {
			if (self.tsBTreeNode<T>::pRight) {
				return tsBTreeNode<T>::verify
					(*self.tsBTreeNode<T>::pRight, item);
			}
			else {
				return 0u; // FALSE - not found
			}
		}
		else {
			assert(0);
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
		item.tsBTreeNode<T>::pLeft = 0;
		item.tsBTreeNode<T>::pRight = 0;
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

