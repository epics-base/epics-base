

//
// osiMutex - OS independent mutex 
// (NOOP on single threaded OS)
//
class osiMutex {
public:
        void lock() {};
        void unlock() {};
private:
};

