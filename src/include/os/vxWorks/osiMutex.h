
//
// osiMutex - OS independent mutex 
// (vxWorks version)
//

#include <semLib.h>
#include <epicsAssert.h>

class osiMutex {
public:
    class osiMutexNoMemory {}

	osiMutex()
	{
		this->mutex = semMCreate(SEM_Q_PRIORITY|SEM_INVERSION_SAFE|SEM_DELETE_SAFE);
		if (this->mutex==NULL) 
		{
			throw osiMutexNoMemory();
		}
	}

	~osiMutex()
	{
		STATUS s;
		s = semDelete (this->mutex);
		assert (s==OK);
	}

	void lock () const
	{
		STATUS s;
		s = semTake (this->mutex, WAIT_FOREVER);
		assert (s==OK);
	}

    void unlock() const
	{ 
		STATUS s;
		s = semGive (this->mutex);
		assert (s==OK);
	}

	void show (unsigned level) const
	{
		semShow (this->mutex, (int) level);
	}
private:
	mutable SEM_ID	mutex;
};

