
//
// osiMutex - OS independent mutex 
// (vxWorks version)
//
//
// NOTES:
// 1) epicsPrintf() is used in this file because we cant stand
// the logMsg() 8 arg API amd we dont want the messages from different
// tasks to co-mingle
//

#include <semLib.h>
#include <epicsAssert.h>
#include <epicsPrint.h>

#define DEBUG_OSIMUTEX

#ifdef DEBUG_OSIMUTEX
#include <stdio.h>
#endif

#ifdef DEBUG_OSIMUTEX
#define osiLock() osiLockI (__FILE__, __LINE__)
#define osiUnlock() osiUnlockI (__FILE__, __LINE__)
#endif

class osiMutex {
public:
	osiMutex()
	{
		mutex = NULL;
	}
	//
	// constructor that returns status
	// (since g++ does not have exceptions)
	//
	int init ()
	{
		this->mutex = semMCreate(SEM_Q_PRIORITY|SEM_INVERSION_SAFE|SEM_DELETE_SAFE);
		if (this->mutex==NULL) 
		{
			return -1;
		}
#		ifdef DEBUG_OSIMUTEX
			epicsPrintf("created mutex at %lx\n", 
				(unsigned long) this->mutex);
#		endif
		return 0;
	}

	~osiMutex()
	{
		STATUS s;
		s = semDelete (this->mutex);
		assert (s==OK);
#		ifdef DEBUG_OSIMUTEX
			epicsPrintf("destroyed mutex at %lx\n", 
				(unsigned long) this->mutex);
#		endif
	}

#ifdef DEBUG_OSIMUTEX
	void osiLockI(const char *pFN, unsigned ln) 
#else
	void osiLock() 
#endif
	{
		STATUS s;
		if (!this->mutex) {
#			ifdef DEBUG_OSIMUTEX
				epicsPrintf(
	"osiMutex: lock request before init was ignored from %s at %u\n",
				pFN, ln);
#			else
				epicsPrintf(
	"osiMutex: lock request before init was ignored\n");
#			endif
			return;
		}
		assert(this->mutex);
		s = semTake (this->mutex, WAIT_FOREVER);
		assert (s==OK);
#		ifdef DEBUG_OSIMUTEX
			epicsPrintf("L%lx in %s at %u\n", 
				(unsigned long) this->mutex,
				pFN, ln);
#		endif
	}

#ifdef DEBUG_OSIMUTEX
	void osiUnlockI(const char *pFN, unsigned ln) 
#else
        void osiUnlock() 
#endif
	{ 
		STATUS s;

		if (!this->mutex) {
#			ifdef DEBUG_OSIMUTEX
				epicsPrintf(
	"osiMutex: unlock request before init was ignored from %s at %u\n",
				pFN, ln);
#			else
				epicsPrintf( 
	"osiMutex: unlock request before init was ignored\n");
#			endif
			return;
		}
		s = semGive (this->mutex);
		assert (s==OK);
#		ifdef DEBUG_OSIMUTEX
			epicsPrintf("U%lx in %s at %d\n", 
				(unsigned long) this->mutex,
				pFN, ln);
#		endif
	}

	void show(unsigned level) const
	{
		semShow(this->mutex, (int) level);
	}
private:
	SEM_ID	mutex;
};

