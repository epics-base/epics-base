
//
// osiMutex - OS independent mutex 
// (vxWorks version)
//

#include <semLib.h>
#include <assert.h>

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
		this->mutex = semMCreate(SEM_Q_PRIORITY|SEM_INVERSION_SAFE);
		if (this->mutex==NULL) 
		{
			return -1;
		}
		return 0;
	}
	~osiMutex()
	{
		STATUS s;
		s = semDelete (this->mutex);
		assert (s==OK);
	}
        void lock() 
	{
		STATUS s;
		assert(this->mutex);
		s = semTake (this->mutex, WAIT_FOREVER);
		assert (s==OK);
	}
        void unlock() 
	{ 
		STATUS s;
		s = semGive (this->mutex);
		assert (s==OK);
	}
private:
	SEM_ID	mutex;
};

