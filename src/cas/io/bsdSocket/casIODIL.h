
#ifndef casIODILh
#define casIODILh

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

#endif // casIODILh

