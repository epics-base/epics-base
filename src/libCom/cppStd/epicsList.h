// epicsList.h
//	Author:	Andrew Johnson
//	Date:	October 2000

#ifndef __EPICS_LIST_H__
#define __EPICS_LIST_H__

#include "epicsListBase.h"
#include "epicsExcept.h"
#include "epicsCppStd.h"
#include <stdexcept>

// epicsList
// ===========

// This implementation omits allocators, and only supports lists of pointers to
// objects.  Class templates are not provided, so functions overloaded on input
// iterators are not available, nor are reverse iterators.  Some functionality
// makes no sense in this version, for example splice cannot be implemented
// quickly because list nodes must never be transferred to a different list.

// Reference numbers in comments are to sections in ISO/IEC 14882:1998(E)

template <class T>
class epicsList {
public:
    // Types
    typedef size_t size_type;
    class iterator;
    class const_iterator;
    
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
    iterator erase(iterator position, iterator last);
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

friend class iterator;
friend class const_iterator;
};

// Specialized algorithms:
template <class T>
    inline void swap(epicsList<T>& x, epicsList<T>& y) { x.swap(y); }


// Mutable iterator

template <class T>
class epicsList<T>::iterator {
public:
    iterator();
// The following are created automatically by C++:
    // iterator(const iterator& rhs);
    // iterator& operator=(const iterator& rhs);
    
    T operator*() const;
    T operator->() const;
    
    iterator& operator++();
    const iterator operator++(int);
    iterator& operator--();
    const iterator operator--(int);
    
    bool operator==(const iterator& rhs) const;
    bool operator==(const const_iterator& rhs) const;
    bool operator!=(const iterator& rhs) const;
    bool operator!=(const const_iterator& rhs) const;

private: // Constructor for List use
    iterator(epicsListNode* node);

private:
    epicsListNode* _node;

friend class epicsList<T>;
friend class const_iterator;
};


// Constant iterator

template <class T>
class epicsList<T>::const_iterator {
public:
    const_iterator();
    const_iterator(const iterator& rhs);
    const_iterator&  operator=(const iterator& rhs);
// The following are created automatically by C++:
    // const_iterator(const const_iterator& rhs);
    // const_iterator&  operator=(const const_iterator& rhs);
    
    const T operator*() const;
    const T operator->() const;
    
    const_iterator& operator++();
    const const_iterator operator++(int);
    const_iterator& operator--();
    const const_iterator operator--(int);
    
    bool operator==(const const_iterator& rhs) const;
    bool operator==(const iterator& rhs) const;
    bool operator!=(const const_iterator& rhs) const;
    bool operator!=(const iterator& rhs) const;

private: // Constructor for List use
    const_iterator(const epicsListNode* node);

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
inline epicsList<T>::iterator epicsList<T>::begin() {
    return _head.next();
}

template <class T>
inline epicsList<T>::const_iterator epicsList<T>::begin() const {
    return _head.next();
}

template <class T>
inline epicsList<T>::iterator epicsList<T>::end() {
    return &_head;
}

template <class T>
inline epicsList<T>::const_iterator epicsList<T>::end() const {
    return &_head;
}

template <class T>
inline bool epicsList<T>::empty() const {
    return (_count == 0);
}

template <class T>
inline epicsList<T>::size_type epicsList<T>::size() const {
    return _count;
}

template <class T>
inline T epicsList<T>::front() {
    if (empty())
	throw STD_ logic_error("list::front: list empty");
    return static_cast<T>(_head.next()->payload);
}

template <class T>
inline const T epicsList<T>::front() const {
    if (empty())
	throw STD_ logic_error("list::front: list empty");
    return static_cast<const T>(_head.next()->payload);
}

template <class T>
inline T epicsList<T>::back() {
    if (empty())
	throw STD_ logic_error("list::back: list empty");
    return static_cast<T>(_head.prev()->payload);
}

template <class T>
inline const T epicsList<T>::back() const {
    if (empty())
	throw STD_ logic_error("list::back: list empty");
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
	throw STD_ logic_error("list::pop_front: list empty");
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
	throw STD_ logic_error("list::pop_back: list empty");
    epicsListNode* node = _head.prev();
    node->unlink();
    _count--;
    _pool.free(node);
}

template <class T>
inline epicsList<T>::iterator epicsList<T>::insert(iterator pos, const T x) {
    epicsListNode* node = _pool.allocate();
    node->payload = x;
    pos._node->insert(*node);
    _count++;
    return node;
}

template <class T>
inline epicsList<T>::iterator epicsList<T>::erase(iterator pos) {
    if ((pos._node == 0) || (pos._node == &_head))
	return pos;
    iterator next = pos._node->next();
    pos._node->unlink();
    _count--;
    _pool.free(pos._node);
    return next;
}

template <class T>
inline epicsList<T>::iterator epicsList<T>::erase(iterator pos, iterator last) {
    while (pos != last) {
	pos = erase(pos);
    }
    return pos;
}

template <class T>
inline void epicsList<T>::swap(epicsList<T>& x) {
    _head.swap(x._head);
    _pool.swap(x._pool);
    size_type temp = x._count;
    x._count = _count;
    _count = temp;
}

template <class T>
inline void epicsList<T>::clear() {
    if (!empty())
	erase(begin(), end());
}


// epicsList<T>::iterator
template <class T>
inline epicsList<T>::iterator::iterator() :
    _node(0) {}

template <class T>
inline epicsList<T>::iterator::iterator(epicsListNode* node) :
    _node(node) {}

template <class T>
inline T epicsList<T>::iterator::operator*() const {
    return static_cast<T>(_node->payload);
}

template <class T>
inline T epicsList<T>::iterator::operator->() const {
    return static_cast<T>(_node->payload);
}

template <class T>
inline epicsList<T>::iterator& epicsList<T>::iterator::operator++() {
    _node = _node->next();
    return *this;
}

template <class T>
inline const epicsList<T>::iterator epicsList<T>::iterator::operator++(int) {
    iterator temp = *this;
    ++(*this);
    return temp;
}

template <class T>
inline epicsList<T>::iterator& epicsList<T>::iterator::operator--() {
    _node = _node->prev();
    return *this;
}

template <class T>
inline const epicsList<T>::iterator epicsList<T>::iterator::operator--(int) {
    iterator temp = *this;
    --(*this);
    return temp;
}

template <class T>
inline bool epicsList<T>::iterator::operator==(const iterator& rhs) const {
    return (_node == rhs._node);
}

template <class T>
inline bool epicsList<T>::iterator::operator==(const const_iterator& rhs) const {
    return (rhs == *this);
}

template <class T>
inline bool epicsList<T>::iterator::operator!=(const iterator& rhs) const {
    return !(_node == rhs._node);
}

template <class T>
inline bool epicsList<T>::iterator::operator!=(const const_iterator& rhs) const {
    return !(rhs == *this);
}

// epicsList<T>::const_iterator
template <class T>
inline epicsList<T>::const_iterator::const_iterator() :
    _node(0) {}

template <class T>
inline epicsList<T>::const_iterator::const_iterator(const iterator& rhs) :
    _node(rhs._node) {}

template <class T>
inline epicsList<T>::const_iterator::const_iterator(const epicsListNode* node) :
    _node(node) {}

template <class T>
inline epicsList<T>::const_iterator&
epicsList<T>::const_iterator::operator=(const iterator& rhs){
    _node = rhs._node;
    return *this;
}

template <class T>
inline const T epicsList<T>::const_iterator::operator*() const {
    return static_cast<T>(_node->payload);
}

template <class T>
inline const T epicsList<T>::const_iterator::operator->() const {
    return static_cast<T>(_node->payload);
}

template <class T>
inline epicsList<T>::const_iterator& epicsList<T>::const_iterator::operator++() {
    _node = _node->next();
    return *this;
}

template <class T>
inline const epicsList<T>::const_iterator epicsList<T>::const_iterator::operator++(int) {
    const_iterator temp = *this;
    ++(*this);
    return temp;
}

template <class T>
inline epicsList<T>::const_iterator& epicsList<T>::const_iterator::operator--() {
    _node = _node->prev();
    return *this;
}

template <class T>
inline const epicsList<T>::const_iterator epicsList<T>::const_iterator::operator--(int) {
    const_iterator temp = *this;
    --(*this);
    return temp;
}

template <class T>
inline bool 
epicsList<T>::const_iterator::operator==(const const_iterator& rhs) const {
    return (_node == rhs._node);
}

template <class T>
inline bool epicsList<T>::const_iterator::operator==(const iterator& rhs) const {
    return (_node == rhs._node);
}

template <class T>
inline bool
epicsList<T>::const_iterator::operator!=(const const_iterator& rhs) const {
    return !(_node == rhs._node);
}

template <class T>
inline bool epicsList<T>::const_iterator::operator!=(const iterator& rhs) const {
    return !(_node == rhs._node);
}

#endif
