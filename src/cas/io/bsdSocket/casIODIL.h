
#ifndef casIODILh
#define casIODILh


//
// casDGIntfIO::getFD()
//
inline int casDGIntfIO::getFD() const
{
	return this->sock;
}

//
// casDGIntfIO::getBCastFD()
//
inline int casDGIntfIO::getBCastFD() const
{
	return this->bcastRecvSock;
}

//
// casDGIntfIO::getBCastFD()
//
inline bool casDGIntfIO::validBCastFD() const
{
	return this->bcastRecvSock!=INVALID_SOCKET;
}

//
// casIntfIO::serverAddress ()
//
inline caNetAddr casIntfIO::serverAddress () const
{
    return caNetAddr (this->addr);
}

#endif // casIODILh

