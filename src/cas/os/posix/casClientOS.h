
class casStreamWriteReg;
class casStreamReadReg;
class casStreamEvWakeup;

//
// casStreamOS
//
class casStreamOS : public casStrmClient {
	friend class casStreamReadReg;
	friend class casStreamWriteReg;
	friend class casStreamEvWakeup;
public:
        casStreamOS(caServerI &, casMsgIO &);
	caStatus init();
	~casStreamOS();

        //
        // process any incomming messages
        //
        casProcCond processInput();
        caStatus start();

        void recvCB();
        void sendCB();

	void sendBlockSignal();
	
	void ioBlockedSignal();

	void eventSignal();
	void eventFlush();

	void show(unsigned level);
private:
	casStreamWriteReg	*pWtReg;
	casStreamReadReg	*pRdReg;
	casStreamEvWakeup	*pEvWk;
	unsigned		sendBlocked:1;
        //
        //
        //
        void armSend ();
        void armRecv ();
        void disarmSend();
        void disarmRecv();
};

//
// casStreamReadReg
//
class casStreamReadReg : public fdReg {
public:
	casStreamReadReg (casStreamOS &osIn) : 
		os (osIn),
		fdReg (osIn.getFD(), fdrRead) {}
	~casStreamReadReg ();

	void show (unsigned level);
private:
	casStreamOS	&os;

	void callBack ();
};

//
// casStreamWriteReg
//
class casStreamWriteReg : public fdReg {
public:
	casStreamWriteReg (casStreamOS &osIn) : 
		os (osIn), fdReg (osIn.getFD(), fdrWrite, TRUE) {}
	~casStreamWriteReg ();

	void show (unsigned level);
private:
	casStreamOS	&os;

	void callBack ();
};

class casDGReadReg;
class casDGEvWakeup;

//
// casDGOS
//
class casDGOS : public casDGClient {
	friend class casDGReadReg;
	friend class casDGEvWakeup;
public:
        casDGOS(caServerI &cas);
	caStatus init();
	~casDGOS();

        //
        // process any incomming messages
        //
        casProcCond processInput();
        caStatus start();

        void recvCB();
        void sendCB();

	void sendBlockSignal() {}

	void eventSignal();
	void eventFlush();

	void show(unsigned level);
private:
	casDGReadReg	*pRdReg;
	casDGEvWakeup	*pEvWk;
};

//
// casDGReadReg
//
class casDGReadReg : public fdReg {
public:
	casDGReadReg (casDGOS &osIn) : 
		os (osIn), fdReg (osIn.getFD(), fdrRead) {}
	~casDGReadReg ();

	void show (unsigned level);
private:
	casDGOS	&os;

	void callBack ();
};

