
//
// osiMutex - OS independent mutex 
// (NOOP on single threaded OS)
//
class osiMutex {
public:
        //
        // constructor that returns status
        // (since g++ does not have exceptions)
        //
	int init() {return 0;}
        void osiLock() {}
        void osiUnlock() {}
	void show (unsigned) {}
private:
};

