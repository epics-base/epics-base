// epicsListBase.h
//	Author:	Andrew Johnson
//	Date:	October 2000

#ifndef __EPICS_LIST_BASE_H__
#define __EPICS_LIST_BASE_H__

#ifdef __GNUG__
  #pragma interface
#endif

#include "epicsMutex.h"
#include "epicsAlgorithm.h"

// epicsListNode
class epicsListNode {
public:
    epicsListNode(int /*dummy*/); // constructor for epicsList<T>._head
    
    // Status queries
    bool hasNext() const;
    epicsListNode* next() const;
    epicsListNode* prev() const;
    
    // Node operations
    void insert(epicsListNode& node);
    void append(epicsListNode& node);
    void unlink();
    void swap(epicsListNode& node);
    
private: // Constructor for epicsListNodeBlock._node[]
    epicsListNode();
    
private: // Data - must start with the _next pointer
    epicsListNode* _next;
    epicsListNode* _prev;
public: // Data
    void* payload;

friend class epicsListNodeBlock;
};


// epicsListLink
class epicsListLink {
public:
    epicsListLink();
    
    // Status queries
    bool hasNext() const;
    
    // list operations
    void set(epicsListLink* node);
    void append(epicsListLink* node);
    epicsListLink* extract();
    void swap(epicsListLink& node);
    
private:
    epicsListLink* _next;
};


// epicsListNodeBlock
class epicsListNodeBlock : public epicsListLink {
public:
    epicsListNodeBlock();
    
    // operations
    epicsListLink* first();
    void reset();
    
private: // Data
    enum {blocksize = 256};	// Poor man's const int, for MSVC++
    
    epicsListNode _node[blocksize];
};


// epicsListNodePool
class epicsListNodePool {
public:
    epicsListNodePool();
    epicsShareFunc epicsShareAPI ~epicsListNodePool();
    
    // allocate & free nodes
    epicsListNode* allocate();
    void free(epicsListNode* node);
    
    void swap(epicsListNodePool& pool);

private: // Non-inline routines
    epicsShareFunc void epicsShareAPI extend();

private: // Data
    epicsListLink _free;
    epicsListLink _blocks;
    
    static epicsMutex  _mutex;
    static epicsListLink _store;
};

// END OF DECLARATIONS

// INLINE METHODS

// epicsListLink
inline epicsListLink::epicsListLink() :
    _next(0) {}

inline bool epicsListLink::hasNext() const {
    return (_next != 0);
}

inline void epicsListLink::set(epicsListLink* node) {
    _next = node;
}

inline void epicsListLink::append(epicsListLink* node) {
    node->_next = _next;
    _next = node;
}

inline epicsListLink* epicsListLink::extract() {
    epicsListLink* result = _next;
    _next = result->_next;
    result->_next = 0;
    return result;
}

inline void epicsListLink::swap(epicsListLink& node) {
    epicsSwap(node._next, _next);
}


// epicsListNode

// Disable spurious warnings from MSVC
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4355)
#endif

inline epicsListNode::epicsListNode() :
    _next(this + 1), _prev(0), payload(0) {}

inline epicsListNode::epicsListNode(int) :
    _next(this), _prev(this), payload(0) {}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

inline bool epicsListNode::hasNext() const {
    return (_next != this);
}

inline epicsListNode* epicsListNode::next() const {
    return _next;
}

inline epicsListNode* epicsListNode::prev() const {
    return _prev;
}

inline void epicsListNode::insert(epicsListNode& node) {
    node._next = this;
    node._prev = _prev;
    _prev->_next = &node;
    _prev = &node;
}

inline void epicsListNode::append(epicsListNode& node) {
    node._prev = this;
    node._next = _next;
    _next->_prev = &node;
    _next = &node;
}

inline void epicsListNode::unlink() {
    _prev->_next = _next;
    _next->_prev = _prev;
    _next = this;
    _prev = this;
}

inline void epicsListNode::swap(epicsListNode& node) {
    epicsSwap(node._next, _next);
    _next->_prev = this;
    node._next->_prev = &node;
    
    epicsSwap(node._prev, _prev);
    _prev->_next = this;
    node._prev->_next = &node;
}

// epicsListNodeBlock
inline epicsListNodeBlock::epicsListNodeBlock() {
    // The epicsListNode constructor links all the _next pointers in the block
    _node[blocksize-1]._next = 0;
}

inline void epicsListNodeBlock::reset() {
    for (int i=0; i<blocksize-1; i++)
	_node[i]._next = &(_node[i+1]);
    _node[blocksize-1]._next = 0;
}

inline epicsListLink* epicsListNodeBlock::first() {
    return reinterpret_cast<epicsListLink*>(&(_node[0]));
}

// epicsListNodePool
inline epicsListNodePool::epicsListNodePool() :
    _free(), _blocks() {}

inline epicsListNode* epicsListNodePool::allocate() {
    if (!_free.hasNext())
	extend();	// Complex non-inline code
    return reinterpret_cast<epicsListNode*>(_free.extract());
}

inline void epicsListNodePool::free(epicsListNode* node) {
    _free.append(reinterpret_cast<epicsListLink*>(node));
}

inline void epicsListNodePool::swap(epicsListNodePool& pool) {
    _free.swap(pool._free);
    _blocks.swap(pool._blocks);
}

#endif
