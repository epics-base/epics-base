
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


