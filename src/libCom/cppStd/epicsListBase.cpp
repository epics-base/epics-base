// epicsListBase.cpp
//	Author:	Andrew Johnson
//	Date:	October 2000

#ifdef __GNUG__
  #pragma implementation
#endif

#define epicsExportSharedSymbols
#include "epicsListBase.h"
#include "cantProceed.h"

// epicsListNodePool
epicsMutex epicsListNodePool::_mutex;
epicsListLink epicsListNodePool::_store;

epicsListNodePool::~epicsListNodePool() {
    while (_blocks.hasNext()) {
	epicsListNodeBlock* block = 
		static_cast<epicsListNodeBlock*>(_blocks.extract());
	block->reset();
	_mutex.lock();
	_store.append(block);
	_mutex.unlock();
    }
}

void epicsListNodePool::extend() {
    assert(!_free.hasNext());
    epicsListNodeBlock* block = 0;
    if (_store.hasNext()) {
	_mutex.lock();
	if (_store.hasNext())
	    block = static_cast<epicsListNodeBlock*>(_store.extract());
	_mutex.unlock();
    }
    if (block == 0)
	block = new epicsListNodeBlock;
    if (block == 0) // in case new didn't throw...
	cantProceed("epicsList: out of memory");
    _blocks.append(block);
    _free.set(block->first());
}

