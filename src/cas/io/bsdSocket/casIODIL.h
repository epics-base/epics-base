
#ifndef casIODILh
#define casIODILh

//
// casDGIntfIO::processDG()
//
inline void casDGIntfIO::processDG()
{
	assert(this->pAltOutIO);

	//
	// process the request DG and send a response DG 
	// if it is required
	//
	this->client.processDG(*this,*this->pAltOutIO);
}

#endif // casIODILh

