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
// On older version of ms visual c++ operator new 
// does not throw a bad_alloc exception
// when it fails. It simply returns a null pointer.
// This behavior is not in conformance with the 
// ANSI / ISO C++.
// 
#if _MSC_VER > 1000 && _MSC_VER < 1400

#include <new>
#include <new.h>

// instruct loader to call this gllobal object 
// constructor before user global object constructors
#pragma warning (disable: 4073)
#pragma init_seg(lib)
#pragma warning (default: 4073)

static int my_new_handler (size_t size)
{
    throw std::bad_alloc();
    return 0;
}

class my_new_handler_obj
{
public:
    my_new_handler_obj()
    {
        //_old_new_mode = _set_new_mode(1); // cause malloc to throw like new
        _old_new_handler = _set_new_handler(my_new_handler);
    }

    ~my_new_handler_obj()
    {
        _set_new_handler(_old_new_handler);
        //_set_new_mode(_old_new_mode);
    }

private:
    _PNH _old_new_handler;
    //int _old_new_mode;
};

static my_new_handler_obj _g_new_handler_obj;

#endif
