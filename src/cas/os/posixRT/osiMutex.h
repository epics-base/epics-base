//
// osiMutex - OS independent mutex 
// (posix RT version)
//

#include <semaphore.h>

#ifndef assert // allow use of epicsAssert.h
#include <assert.h>
#endif

class osiMutex {
public:
	//
	// constructor that returns status
	// (since g++ does not have exceptions)
	//
	caStatus osiMutexInit()
	{
		int status;
		status = sem_init(&this->sem,0,0);		
		if (status) {
			return -1;
		}
		return 0;
	}
	~osiMutex()
	{
		int status;
		status = sem_destroy(&this->sem);		
		assert(status==0);
	}
        void lock() 
	{
		int status;
		status = sem_wait(&this->sem);		
		assert(status==0);
	}
        void unlock() 
	{
		int status;
		status = sem_post(&this->sem);		
		assert(status==0);
	}
private:
	sem_t	sem;
};

