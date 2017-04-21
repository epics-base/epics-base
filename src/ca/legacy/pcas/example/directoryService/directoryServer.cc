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
// Example EPICS CA directory server
//

#include "directoryServer.h"
#include "epicsAlgorithm.h"

const pvInfo *pvInfo::pFirst;

//
// directoryServer::directoryServer()
//
directoryServer::directoryServer(const char * const pvPrefix, unsigned aliasCount) 
{
	unsigned i;
	const pvInfo *pPVI;
	char pvAlias[256];
	const char * const pNameFmtStr = "%.100s%.20s";
	const char * const pAliasFmtStr = "%.100s%.20s%u";

	//
	// pre-create all of the simple PVs that this server will export
	//
	for (pPVI = pvInfo::getFirst(); pPVI; pPVI=pPVI->getNext()) {

		//
		// Install canonical (root) name
		//
		sprintf(pvAlias, pNameFmtStr, pvPrefix, pPVI->getName());
		this->installAliasName(*pPVI, pvAlias);

		//
		// Install numbered alias names
		//
		for (i=0u; i<aliasCount; i++) {
			sprintf(pvAlias, pAliasFmtStr, pvPrefix,  
					pPVI->getName(), i);
			this->installAliasName(*pPVI, pvAlias);
		}
	}
}


//
// directoryServer::~directoryServer()
//
directoryServer::~directoryServer()
{
}

//
// directoryServer::installAliasName()
//
void directoryServer::installAliasName(const pvInfo &info, const char *pAliasName)
{
	pvEntry	*pEntry;

	pEntry = new pvEntry(info, *this, pAliasName);
	if (pEntry) {
		int resLibStatus;
		resLibStatus = this->stringResTbl.add(*pEntry);
		if (resLibStatus==0) {
			return;
		}
		else {
			delete pEntry;
		}
	}
	fprintf(stderr, 
"Unable to enter PV=\"%s\" Alias=\"%s\" in PV name alias hash table\n",
		info.getName(), pAliasName);
}

//
// More advanced pvExistTest() isnt needed so we forward to
// original version. This avoids sun pro warnings and speeds 
// up execution.
//
pvExistReturn directoryServer::pvExistTest
	( const casCtx & ctx, const caNetAddr &, const char * pPVName )
{
	return this->pvExistTest ( ctx, pPVName );
}

//
// directoryServer::pvExistTest()
//
pvExistReturn directoryServer::pvExistTest
	( const casCtx & /* ctx */, const char * pPVName )
{
	//
	// lifetime of id is shorter than lifetime of pName
	//
	char pvNameBuf[512];
	stringId id(pPVName, stringId::refString);
	stringId idBuf(pvNameBuf, stringId::refString);
	pvEntry *pPVE;
	const char *pStr, *pLastStr;

	//
	// strip trailing occurrence of ".field" 
	// (for compatibility with EPICS
	// function block database).
	//
	pLastStr = pPVName;
	pStr = strstr (pPVName, ".");
	while (pStr) {
		pLastStr = pStr;
		pStr += 1;
		pStr = strstr (pStr, ".");
	}

	if (pLastStr==pPVName) {
		pPVE = this->stringResTbl.lookup(id);
		if (!pPVE) {
			return pverDoesNotExistHere;
		}
	}
	else {
		size_t diff = pLastStr-pPVName;
		diff = epicsMin (diff, sizeof(pvNameBuf)-1);
		memcpy (pvNameBuf, pPVName, diff);
		pvNameBuf[diff] = '\0';
		pLastStr = pvNameBuf;
		pPVE = this->stringResTbl.lookup(idBuf);
		if (!pPVE) {
			//
			// look for entire PV name (in case this PV isnt
			// associated a function block database)
			//
			// lifetime of id2 is shorter than lifetime of pName
			//
			pPVE = this->stringResTbl.lookup(id);
			if (!pPVE) {
				return pverDoesNotExistHere;
			}
		}
	}

	return pvExistReturn (caNetAddr(pPVE->getInfo().getAddr()));
}

//
// directoryServer::show() 
//
void directoryServer::show (unsigned level) const
{
	//
	// server tool specific show code goes here
	//
	this->stringResTbl.show(level);

	//
	// print information about ca server libarary
	// internals
	//
	this->caServer::show(level);
}

