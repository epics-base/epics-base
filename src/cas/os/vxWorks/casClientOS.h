/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

class casStreamEvWakeup;

//
// casStreamOS
//
class casStreamOS : public casStrmClient {
	friend int casStrmServer (casStreamOS *);
	friend int casStrmEvent (casStreamOS *);
public:
	casStreamOS(caServerI &, casMsgIO &);
	caStatus init();
	~casStreamOS();

	//
	// process any incomming messages
	//
	casProcCond processInput();
	caStatus start();

	void eventSignal();
	void eventFlush();

	void show(unsigned level);
private:
	SEM_ID		eventSignalSem;
	int		clientTId;
	int		eventTId;
};

//
// vxWorks task entry 
//
int casStrmServer (casStreamOS *);
int casStrmEvent (casStreamOS *);

class casDGEvWakeup;

//
// casDGOS
//
class casDGOS : public casDGClient {
	friend int casDGServer (casDGOS *);
	friend int casDGEvent (casDGOS *);
public:
	casDGOS(caServerI &cas);
	caStatus init();
	~casDGOS();

	//
	// process any incomming messages
	//
	casProcCond processInput();
	caStatus start();

	void eventSignal();
	void eventFlush();

	void show(unsigned level);
private:
	SEM_ID		eventSignalSem;
	int		clientTId;
	int		eventTId;
};

//
// vxWorks task entry 
//
int casDGServer (casDGOS *);
int casDGEvent (casDGOS *);


