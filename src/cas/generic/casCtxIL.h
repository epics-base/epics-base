
#ifndef casCtxILh
#define casCtxILh

//
// casCtx::casCtx()
//
inline casCtx::casCtx() :
	pData(NULL), pCAS(NULL), pClient(NULL),
	pChannel(NULL), pPV(NULL), nAsyncIO(0u)
{
	memset(&this->msg, 0, sizeof(this->msg));
}

//
// casCtx::getMsg()
//
inline const caHdr *casCtx::getMsg() const 
{	
	return (const caHdr *) &this->msg;
}

//
// casCtx::getData()
//
inline void *casCtx::getData() const 
{
	return this->pData;
}

//
// casCtx::getServer()
//
inline caServerI * casCtx::getServer() const 
{
	return this->pCAS;
}

//
// casCtx::getClient()
//
inline casCoreClient * casCtx::getClient() const 
{
	return this->pClient;
}

//
// casCtx::getPV()
//
inline casPVI * casCtx::getPV() const 
{
	return this->pPV;
}

//
// casCtx::getChannel()
//
inline casChannelI * casCtx::getChannel() const 
{	
	return this->pChannel;
}

//
// casCtx::setMsg() 
// (assumes incoming message is in network byte order)
//
inline void casCtx::setMsg(const char *pBuf)
{
	//
	// copy as raw bytes in order to avoid
	// alignment problems
	//
	memcpy (&this->msg, pBuf, sizeof(this->msg));
	this->msg.m_cmmd = ntohs (this->msg.m_cmmd);
	this->msg.m_postsize = ntohs (this->msg.m_postsize);
	this->msg.m_type = ntohs (this->msg.m_type);
	this->msg.m_count = ntohs (this->msg.m_count);
	this->msg.m_cid = ntohl (this->msg.m_cid);
	this->msg.m_available = ntohl (this->msg.m_available);
}

//
// casCtx::setData()
//
inline void casCtx::setData(void *p) 
{
	this->pData = p;
}

//
// casCtx::setServer()
//
inline void casCtx::setServer(caServerI *p)
{
	this->pCAS = p;
}

//
// casCtx::setClient()
//
inline void casCtx::setClient(casCoreClient *p) 
{
	this->pClient = p;
}

//
// casCtx::setPV()
//
inline void casCtx::setPV(casPVI *p) 
{
	this->pPV = p;
}

//
// casCtx::setChannel()
//
inline void casCtx::setChannel(casChannelI *p) 
{
	this->pChannel = p;
}

#endif // casCtxILh

