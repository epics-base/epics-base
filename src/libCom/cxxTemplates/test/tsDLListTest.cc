/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/



#include "tsDLList.h"
#include <assert.h>
#include <stdio.h>

class fred : public tsDLNode<fred> {
public:
    fred(const char * const pNameIn) : pName(pNameIn){}
    void show () {printf("%s\n", pName);}
private:
    const char * const pName;
};

class jane : public fred, public tsDLNode<jane> {
public:
    jane(const char * const pNameIn) : fred(pNameIn){}
private:
};

int main ()
{
    unsigned                i;
    tsDLList<fred>          list;
    fred                    *pFred;
    fred                    *pFredII;
    fred                    *pFredBack;
    tsDLList<jane>          janeList;
    tsDLIter<jane>          janeFwdIter = janeList.firstIter();
    tsDLIter<jane>          janeBwdIter = janeList.lastIter();
    jane                    *pJane;

    pFred = new fred ("A");
    pFredII = new fred ("B");

    list.add (*pFred);
    list.add (*pFredII);
    tsDLIter <fred> iter = list.firstIter();
    assert (iter.pointer() == pFred);
    iter++;
    assert (iter.pointer() == pFredII);
    list.remove(*pFred);
    list.add(*pFred);
    pFredBack = list.get();
    assert (pFredBack == pFredII);
    pFredBack = list.get();
    assert (pFredBack == pFred);
    assert (list.count() == 0u);
    list.add(*pFred);
    list.add(*pFredII);
    list.add(* new fred("C"));
    list.add(* new fred("D"));

    iter = list.firstIter();
    while ( iter.valid() ) {
        iter->show();
        iter++;
    }

    pJane = new jane("JA");
    janeList.add(*pJane);   
    pJane = new jane("JB");
    assert ( janeList.find ( *pJane ) == -1 );
    janeList.add(*pJane);   
    assert ( janeList.find ( *pJane ) == 1 );

    while ( janeFwdIter.valid() ) {
        janeFwdIter->show();
        janeFwdIter++;
    }

    while ( janeBwdIter.valid() ) {
        janeBwdIter->show();
        janeBwdIter--;
    }

    iter = list.firstIter();
    while ( iter.valid() ) {
        iter->show();
        iter++;
    }

    tsDLIter < jane > bdIter = janeList.firstIter ();
    i = 0;
    while ( bdIter.valid () ) {
        i++;
        bdIter++;
    }
    assert ( i == janeList.count () );

    iter = list.firstIter();
    while ( iter.pointer() ) {
        list.remove( * iter.pointer() );
        iter++;
    }
    assert(list.count()==0);

    janeFwdIter = janeList.firstIter();
    while ( janeFwdIter.valid() ) {
        janeList.remove( * janeFwdIter.pointer() );
        janeFwdIter++;
    }
    assert(janeList.count()==0);

    pJane = new jane("JA");
    janeList.add(*pJane);   
    pJane = new jane("JB");
    janeList.add(*pJane);   
    janeBwdIter = janeList.lastIter();
    while ( janeBwdIter.valid() ) {
        janeList.remove( * janeBwdIter.pointer() );
        janeBwdIter--;
    }
    assert(janeList.count()==0);

    return 0;
}

