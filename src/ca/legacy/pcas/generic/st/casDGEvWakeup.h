/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef casDGEvWakeuph 
#define casDGEvWakeuph 

class casDGEvWakeup : public epicsTimerNotify {
public:
	casDGEvWakeup ();
	virtual ~casDGEvWakeup();
	void show ( unsigned level ) const;
    void start ( class casDGIntfOS &osIn );
private:
    epicsTimer &timer;
	class casDGIntfOS *pOS;
	expireStatus expire( const epicsTime & currentTime );
	casDGEvWakeup ( const casDGEvWakeup & );
	casDGEvWakeup & operator = ( const casDGEvWakeup & );
};

#endif // casDGEvWakeuph
