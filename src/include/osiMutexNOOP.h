/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

//
// osiMutex - OS independent mutex 
// (NOOP on single threaded OS)
//
// NOTE:
// I have made lock/unlock const because this allows
// a list to be run in a const member function on a 
// multi-threaded os (since paired lock/unlock
// requests really do not modify the internal 
// state of the object and neither does
// running the list if we dont modify the list).
//
class osiMutex {
public:
        //
        // constructor that returns status
        // (since g++ does not have exceptions)
        //
	int init() {return 0;}
        void osiLock() const {}
        void osiUnlock() const {}
	void show (unsigned) const {}
private:
};

