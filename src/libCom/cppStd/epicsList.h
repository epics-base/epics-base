// epicsList.h
//	Author:	Andrew Johnson
//	Date:	October 2000

#ifndef __EPICS_LIST_H__
#define __EPICS_LIST_H__

#include "epicsListBase.h"
#include <stdexcept>

// epicsList
// ===========

// This implementation omits allocators, and only supports lists of pointers to
// objects.  Class templates are not provided, so functions overloaded on input
// iterators are not available, nor are reverse iterators.  Some functionality
// makes no sense in this version, for example splice cannot be implemented
// quickly because list nodes must never be transferred to a different list.

// Reference numbers in comments are to sections in ISO/IEC 14882:1998(E)

template <class T> class epicsListIterator;
template <class T> class epicsListConstIterator;

template <class T>
class epicsList {
public:
    // Types
    typedef size_t size_type;
    typedef epicsListIterator<T> iterator;
    typedef epicsListConstIterator<T> const_iterator;
    
    // Construct/copy/destroy (23.2.2.1)
    epicsList();
    ~epicsList();
    
    // Iterator support
    iterator begin();
    const_iterator begin() const;
    iterator end();
    const_iterator end() const;
    
    // Capacity
    bool empty() const;
    size_type size() const;
    
    // Element access
    T front();
    const T front() const;
    T back();
    const T back() const;
    
    // Modifiers (23.2.2.3)
    void push_front(const T x);
    void pop_front();
    void push_back(const T x);
    void pop_back();
    
    iterator insert(iterator position, const T x);
    iterator erase(iterator position);
    iterator erase(iterator position, iterator leave);
    void swap(epicsList<T>& x);
    void clear();
    
    // List operations (23.2.2.4)
//  void remove(const T value);
//  void unique();
//  void reverse();

private: // Prevent compiler-generated functions
    epicsList(const epicsList<T>&);
    epicsList<T>& operator=(const epicsList<T>&);

private: // Data
    size_type _count;
    epicsListNode _head;
    epicsListNodePool _pool;

friend class epicsListIterator<T>;
friend class epicsListConstIterator<T>;
};

// Specialized algorithms:
template <class T>
    inline void epicsListSwap(epicsList<T>& x, epicsList<T>& y) { x.swap(y); }


// Mutable iterator
// 	These used to be inner classes of epicsList<T>, but MSVC6
//	didn't like that so they had to be typedefs...

template <class T>
class epicsListIterator {
public:
    epicsListIterator();
// The following are created automatically by C++:
    // epicsListIterator(const epicsListIterator& rhs);
    // epicsListIterator& operator=(const epicsListIterator& rhs);
    
    T operator*() const;
    T operator->() const;
    
    epicsListIterator& operator++();
    const epicsListIterator operator++(int);
    epicsListIterator& operator--();
    const epicsListIterator operator--(int);
    
    bool operator==(const epicsListIterator& rhs) const;
    bool operator==(const epicsListConstIterator<T>& rhs) const;
    bool operator!=(const epicsListIterator& rhs) const;
    bool operator!=(const epicsListConstIterator<T>& rhs) const;

private: // Constructor for List use
    epicsListIterator(epicsListNode* node);

private:
    epicsListNode* _node;

friend class epicsList<T>;
friend class epicsListConstIterator<T>;
};


// Constant iterator

template <class T>
class epicsListConstIterator {
public:
    epicsListConstIterator();
    epicsListConstIterator(const epicsListIterator<T>& rhs);
    epicsListConstIterator&  operator=(const epicsListIterator<T>& rhs);
// The following are created automatically by C++:
    // epicsListConstIterator(const epicsListConstIterator& rhs);
    // epicsListConstIterator&  operator=(const epicsListConstIterator& rhs);
    
    const T operator*() const;
    const T operator->() const;
    
    epicsListConstIterator& operator++();
    const epicsListConstIterator operator++(int);
    epicsListConstIterator& operator--();
    const epicsListConstIterator operator--(int);
    
    bool operator==(const epicsListConstIterator& rhs) const;
    bool operator==(const epicsListIterator<T>& rhs) const;
    bool operator!=(const epicsListConstIterator& rhs) const;
    bool operator!=(const epicsListIterator<T>& rhs) const;

private: // Constructor for List use
    epicsListConstIterator(const epicsListNode* node);

private:
    const epicsListNode* _node;

friend class epicsList<T>;
};

// END OF DECLARATIONS

// INLINE METHODS

// epicsList<T>
template <class T>
inline epicsList<T>::epicsList() :
    _count(0), _head(0), _pool() {}

template <class T>
inline epicsList<T>::~epicsList() {}

template <class T>
inline typename epicsList<T>::iterator epicsList<T>::begin() {
    return _head.next();
}

template <class T>
inline typename epicsList<T>::const_iterator epicsList<T>::begin() const {
    return _head.next();
}

template <class T>
inline typename epicsList<T>::iterator epicsList<T>::end() {
    return &_head;
}

template <class T>
inline typename epicsList<T>::const_iterator epicsList<T>::end() const {
    return &_head;
}

template <class T>
inline bool epicsList<T>::empty() const {
    return (_count == 0);
}

template <class T>
inline typename epicsList<T>::size_type epicsList<T>::size() const {
    return _count;
}

template <class T>
inline T epicsList<T>::front() {
    if (empty())
	throw std::logic_error("epicsList::front: list empty");
    return static_cast<T>(_head.next()->payload);
}

template <class T>
inline const T epicsList<T>::front() const {
    if (empty())
	throw std::logic_error("epicsList::front: list empty");
    return static_cast<const T>(_head.next()->payload);
}

template <class T>
inline T epicsList<T>::back() {
    if (empty())
	throw std::logic_error("epicsList::back: list empty");
    return static_cast<T>(_head.prev()->payload);
}

template <class T>
inline const T epicsList<T>::back() const {
    if (empty())
	throw std::logic_error("epicsList::back: list empty");
    return static_cast<const T>(_head.prev()->payload);
}

template <class T>
inline void epicsList<T>::push_front(const T x) {
    epicsListNode* node = _pool.allocate();
    node->payload = x;
    _head.append(*node);
    _count++;
}

template <class T>
inline void epicsList<T>::pop_front() {
    if (empty())
	throw std::logic_error("epicsList::pop_front: list empty");
    epicsListNode* node = _head.next();
    node->unlink();
    _count--;
    _pool.free(node);
}

template <class T>
inline void epicsList<T>::push_back(const T x) {
    epicsListNode* node = _pool.allocate();
    node->payload = x;
    _head.insert(*node);
    _count++;
}

template <class T>
inline void epicsList<T>::pop_back() {
    if (empty())
	throw std::logic_error("epicsList::pop_back: list empty");
    epicsListNode* node = _head.prev();
    node->unlink();
    _count--;
    _pool.free(node);
}

template <class T>
inline typename epicsList<T>::iterator epicsList<T>::insert(iterator pos, const T x) {
    epicsListNode* node = _pool.allocate();
    node->payload = x;
    pos._node->insert(*node);
    _count++;
    return node;
}

template <class T>
inline typename epicsList<T>::iterator epicsList<T>::erase(iterator pos) {
    if ((pos._node == 0) || (pos._node == &_head))
	return pos;
    iterator next = pos._node->next();
    pos._node->unlink();
    _count--;
    _pool.free(pos._node);
    return next;
}

template <class T>
inline typename epicsList<T>::iterator epicsList<T>::erase(iterator pos, iterator leave) {
    while (pos != leave) {
	pos = erase(pos);
    }
    return pos;
}

template <class T>
inline void epicsList<T>::swap(epicsList<T>& x) {
    _head.swap(x._head);
    _pool.swap(x._pool);
    epicsSwap(x._count, _count);
}

template <class T>
inline void epicsList<T>::clear() {
    if (!empty())
	erase(begin(), end());
}


// epicsListIterator<T>
template <class T>
inline epicsListIterator<T>::epicsListIterator() :
    _node(0) {}

template <class T>
inline epicsListIterator<T>::epicsListIterator(epicsListNode* node) :
    _node(node) {}

template <class T>
inline T epicsListIterator<T>::operator*() const {
    return static_cast<T>(_node->payload);
}

template <class T>
inline T epicsListIterator<T>::operator->() const {
    return static_cast<T>(_node->payload);
}

template <class T>
inline epicsListIterator<T>& epicsListIterator<T>::operator++() {
    _node = _node->next();
    return *this;
}

template <class T>
inline const epicsListIterator<T> epicsListIterator<T>::operator++(int) {
    epicsListIterator<T> temp = *this;
    ++(*this);
    return temp;
}

template <class T>
inline epicsListIterator<T>& epicsListIterator<T>::operator--() {
    _node = _node->prev();
    return *this;
}

template <class T>
inline const epicsListIterator<T> epicsListIterator<T>::operator--(int) {
    epicsListIterator<T> temp = *this;
    --(*this);
    return temp;
}

template <class T>
inline bool epicsListIterator<T>::operator==(const epicsListIterator<T>& rhs) const {
    return (_node == rhs._node);
}

template <class T>
inline bool epicsListIterator<T>::operator==(const epicsListConstIterator<T>& rhs) const {
    return (rhs == *this);
}

template <class T>
inline bool epicsListIterator<T>::operator!=(const epicsListIterator<T>& rhs) const {
    return !(_node == rhs._node);
}

template <class T>
inline bool epicsListIterator<T>::operator!=(const epicsListConstIterator<T>& rhs) const {
    return !(rhs == *this);
}

// epicsListConstIterator<T>
template <class T>
inline epicsListConstIterator<T>::epicsListConstIterator() :
    _node(0) {}

template <class T>
inline epicsListConstIterator<T>::epicsListConstIterator(const epicsListIterator<T>& rhs) :
    _node(rhs._node) {}

template <class T>
inline epicsListConstIterator<T>::epicsListConstIterator(const epicsListNode* node) :
    _node(node) {}

template <class T>
inline epicsListConstIterator<T>&
epicsListConstIterator<T>::operator=(const epicsListIterator<T>& rhs){
    _node = rhs._node;
    return *this;
}

template <class T>
inline const T epicsListConstIterator<T>::operator*() const {
    return static_cast<T>(_node->payload);
}

template <class T>
inline const T epicsListConstIterator<T>::operator->() const {
    return static_cast<T>(_node->payload);
}

template <class T>
inline epicsListConstIterator<T>& epicsListConstIterator<T>::operator++() {
    _node = _node->next();
    return *this;
}

template <class T>
inline const epicsListConstIterator<T> epicsListConstIterator<T>::operator++(int) {
    epicsListConstIterator<T> temp = *this;
    ++(*this);
    return temp;
}

template <class T>
inline epicsListConstIterator<T>& epicsListConstIterator<T>::operator--() {
    _node = _node->prev();
    return *this;
}

template <class T>
inline const epicsListConstIterator<T> epicsListConstIterator<T>::operator--(int) {
    epicsListConstIterator<T> temp = *this;
    --(*this);
    return temp;
}

template <class T>
inline bool 
epicsListConstIterator<T>::operator==(const epicsListConstIterator<T>& rhs) const {
    return (_node == rhs._node);
}

template <class T>
inline bool epicsListConstIterator<T>::operator==(const epicsListIterator<T>& rhs) const {
    return (_node == rhs._node);
}

template <class T>
inline bool
epicsListConstIterator<T>::operator!=(const epicsListConstIterator<T>& rhs) const {
    return !(_node == rhs._node);
}

template <class T>
inline bool epicsListConstIterator<T>::operator!=(const epicsListIterator<T>& rhs) const {
    return !(_node == rhs._node);
}

#endif
