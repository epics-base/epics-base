
//
// tsDLIter<T>
//
// doubly linked list iterator
//
// Notes:
// 1) 
//  This iterator does not allow for removal
//	of an item in order to avoid problems 
//	resulting when we remove an item (and 
//	then dont know whether to make pCurrent 
//	point at the item before or after the 
//	item removed
//
template <class T>
class tsDLIter {
public:
	tsDLIter (tsDLList<T> & listIn);
	void reset ();
	void reset (tsDLList<T> &listIn);
	void operator = (tsDLList<T> &listIn);
	T * next ();
	T * prev ();
	T * first();
	T * last();
	T * operator () ();

protected:
	T       *pCurrent;
	tsDLList<T> *pList;

    T * current (); // certain compilers require this
};

//
// class tsDLFwdIter<T>
//
// Notes:
// 1) No direct access to pCurrent is provided since
// 	this might allow for confusion when an item
//	is removed (and pCurrent ends up pointing at
//	an item that has been seen before)
//
// 2) This iterator only moves forward in order to 
//	avoid problems resulting when we remove an
//	item (and then dont know whether to make 
//	pCurrent point at the item before or after
// 	the item removed
//
template <class T>
class tsDLFwdIter: private tsDLIter<T> {
public:
	tsDLFwdIter (tsDLList<T> &listIn);
	void reset ();
	void reset (tsDLList<T> &listIn);
	void operator = (tsDLList<T> &listIn);
	T * operator () ();
	T * next ();
	T * first();

	//
	// remove ()
	// (and move current to be the item
	// pointed to by pPrev - the item seen
	// by the iterator before the current one -
	// this guarantee that the list will be
	// accessed sequentially even if an item
	// is removed)
	//
	void remove ();
protected:
    T *current ();
};

//
// tsDLBwdIter<T>
//
// Notes:
// 1) No direct access to pCurrent is provided since
// 	this might allow for confusion when an item
//	is removed (and pCurrent ends up pointing at
//	an item that has been seen before)
//
// 2) This iterator only moves backward in order to 
//	avoid problems resulting when we remove an
//	item (and then dont know whether to make 
//	pCurrent point at the item before or after
// 	the item removed
//
template <class T>
class tsDLBwdIter : private tsDLIter<T> {
public:
	tsDLBwdIter (tsDLList<T> &listIn);
	void reset ();
	void reset (tsDLList<T> &listIn);
	void operator = (tsDLList<T> &listIn);
	T * operator () () { return this->tsDLIter<T>::prev(); }
	T * prev ();
	T * last();
	//
	// remove ()
	// remove current item 
	// (and move current to be the item
	// pointed to by pNext - the item seen
	// by the iterator before the current one -
	// this guarantee that the list will be
	// accessed sequentially even if an item
	// is removed)
	//
	void remove ();
protected:
    T * current ();
};

//
// tsDLFwdIter<T>::remove ()
// (and move current to be the item
// pointed to by pPrev - the item seen
// by the iterator before the current one -
// this guarantee that the list will be
// accessed sequentially even if an item
// is removed)
//
template <class T>
void tsDLFwdIter<T>::remove ()
{
	T *pCur = this->pCurrent;

	if (pCur) {
		tsDLNode<T> *pCurNode = pCur;

		//
		// Move this->pCurrent to the previous item
		//
		this->pCurrent = pCurNode->pPrev;

		//
		// delete current item
		//
		this->pList->remove(*pCur);
	}
}

//
// tsDLFwdIter<T>::next ()
// 
template <class T>
inline T * tsDLFwdIter<T>::next () 
{
	tsDLIter<T> &iterBase = *this;
	return iterBase.next();
}

//
// tsDLFwdIter<T>::operator () ()
// 
template <class T>
inline T * tsDLFwdIter<T>::operator () () 
{
	return this->next();
}

//////////////////////////////////////////
// tsDLIter<T> member functions
//////////////////////////////////////////

template <class T> 
inline tsDLIter<T>::tsDLIter (tsDLList<T> & listIn) : 
	pCurrent(0), pList(&listIn) {}

template <class T> 
inline void tsDLIter<T>::reset ()
{
	this->pCurrent = 0;
}

template <class T> 
inline void tsDLIter<T>::reset (tsDLList<T> &listIn)
{
	this->reset();
	this->pList = &listIn;
}

template <class T> 
inline void tsDLIter<T>::operator = (tsDLList<T> &listIn)
{
	this->reset(listIn);
}

template <class T> 
inline T * tsDLIter<T>::first()
{
	this->pCurrent = this->pList->pFirst;
	return this->pCurrent;
}

template <class T> 
inline T * tsDLIter<T>::last()
{
	this->pCurrent = this->pList->pLast;
	return this->pCurrent;
}

//
// tsDLIter<T>::next ()
//
template <class T> 
inline T * tsDLIter<T>::next () 
{
	T *pCur = this->pCurrent;
	if (pCur==0) {
		pCur = this->pList->pFirst;
	}
	else {
		tsDLNode<T> *pCurNode = pCur;
		pCur = pCurNode->pNext;
	}
	this->pCurrent = pCur;
	return pCur;
}

//
// tsDLIter<T>::prev ()
//
template <class T>
inline T * tsDLIter<T>::prev ()
{
	T *pCur = this->pCurrent;
	if (pCur==0) {
		pCur = this->pList->pLast;
	}
	else {
		tsDLNode<T> *pCurNode = pCur;
		pCur = pCurNode->pPrev;
	}
	this->pCurrent = pCur;
	return pCur;
}

//
// tsDLIter<T>::operator () ()
//
template <class T>
inline T * tsDLIter<T>::operator () () 
{
	return this->next();
}

//
// tsDLIter<T>::current()
//
template <class T>
inline T * tsDLIter<T>::current()
{
	return this->pCurrent;
}

///////////////////////////////////////////
// tsDLBwdIter<T> member functions
///////////////////////////////////////////

template <class T>
inline tsDLBwdIter<T>::tsDLBwdIter(tsDLList<T> &listIn) : 
	tsDLIter<T>(listIn) {}

template <class T>
inline void tsDLBwdIter<T>::reset ()
{
	this->tsDLIter<T>::reset();
}

template <class T>
inline void tsDLBwdIter<T>::reset (tsDLList<T> &listIn)
{
	this->tsDLIter<T>::reset(listIn);
}

template <class T>
inline void tsDLBwdIter<T>::operator = (tsDLList<T> &listIn)
{
	this->tsDLIter<T>::reset(listIn);
}

template <class T>
inline T * tsDLBwdIter<T>::last()
{
	return this->tsDLIter<T>::last();
}

//
// tsDLBwdIter<T>::remove ()
// 
// remove current item 
// (and move current to be the item
// pointed to by pNext - the item seen
// by the iterator before the current one -
// this guarantee that the list will be
// accessed sequentially even if an item
// is removed)
//
template <class T>
void tsDLBwdIter<T>::remove ()
{
	T *pCur = this->pCurrent;

	if (pCur) {
		tsDLNode<T> *pCurNode = pCur;

		//
		// strip const (we didnt declare the
		// list const in the constructor)
		//
		tsDLList<T> * pMutableList = 
			(tsDLList<T> *) this->pList;

		//
		// Move this->pCurrent to the item after the
		// item being deleted 
		//
		this->pCurrent = pCurNode->pNext;

		//
		// delete current item
		//
		pMutableList->remove(*pCur);
	}
}

//////////////////////////////////////////
// tsDLFwdIter<T> member functions
//////////////////////////////////////////

template <class T>
inline tsDLFwdIter<T>::tsDLFwdIter (tsDLList<T> &listIn) : 
	tsDLIter<T>(listIn) {}

template <class T>
inline void tsDLFwdIter<T>::reset ()
{
	this->tsDLIter<T>::reset();
}

template <class T>
inline void tsDLFwdIter<T>::reset (tsDLList<T> &listIn)
{
	this->tsDLIter<T>::reset(listIn);
}

template <class T>
inline void tsDLFwdIter<T>::operator = (tsDLList<T> &listIn)
{
	this->tsDLIter<T>::reset(listIn);
}

template <class T>
inline T * tsDLFwdIter<T>::first()
{
	tsDLIter<T> &iterBase = *this;
	return iterBase.first();
}

template <class T>
inline T * tsDLFwdIter<T>::current()
{
	return this->tsDLIter<T>::current ();
}
