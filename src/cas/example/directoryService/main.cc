
#include <stdio.h>

#include "directoryServer.h"
#include "fdManager.h"

#define LOCAL static

LOCAL int parseDirectoryFile (const char *pFileName);
LOCAL int parseDirectoryFP (FILE *pf, const char *pFileName);

#ifndef TRUE
#define TRUE 1
#endif

#ifndef INADDR_NONE
#define INADDR_NONE (~0ul)
#endif

//
// main()
// (example single threaded ca server tool main loop)
//
extern int main (int argc, const char **argv)
{
	osiTime		begin(osiTime::getCurrent());
	directoryServer	*pCAS;
	unsigned 	debugLevel = 0u;
	float		executionTime;
	char		pvPrefix[128] = "";
	char		fileName[128] = "pvDirectory.txt";
	unsigned	aliasCount = 0u;
	aitBool		forever = aitTrue;
	int		nPV;
	int		i;

	for (i=1; i<argc; i++) {
		if (sscanf(argv[i], "-d %u", &debugLevel)==1) {
			continue;
		}
		if (sscanf(argv[i],"-t %f", &executionTime)==1) {
			forever = aitFalse;
			continue;
		}
		if (sscanf(argv[i],"-p %127s", pvPrefix)==1) {
			continue;
		}
		if (sscanf(argv[i],"-c %u", &aliasCount)==1) {
			continue;
		}
		if (sscanf(argv[i], "-f %127s", fileName)==1) {
			continue;
		}
		fprintf (stderr,
"usage: %s [-d<debug level> -t<execution time> -p<PV name prefix> -c<numbered alias count> -f<PV directory file>]\n", 
			argv[0]);

		return (1);
	}

	nPV = parseDirectoryFile (fileName);
	if (nPV<=0) {
		fprintf(stderr, 
"No PVs in directory file=%s. Exiting because there is no useful work to do.\n",
			fileName);
		return (-1);
	}

	pCAS = new directoryServer(pvPrefix, (unsigned) nPV, aliasCount);
	if (!pCAS) {
		return (-1);
	}

	pCAS->setDebugLevel(debugLevel);

	if (forever) {
		osiTime	delay(1000u,0u);
		//
		// loop here forever
		//
		while (aitTrue) {
			fileDescriptorManager.process(delay);
		}
	}
	else {
		osiTime total(executionTime);
		osiTime delay(osiTime::getCurrent() - begin);
		//
		// loop here untime the specified execution time
		// expires
		//
		while (delay < total) {
			fileDescriptorManager.process(delay);
			delay = osiTime::getCurrent() - begin;
		}
	}
	pCAS->show(2u);
	delete pCAS;
	return (0);
}

//
// parseDirectoryFile()
//
// PV directory file is expected to have entries of the form:
// <PV name> <host name or dotted ip address> [<optional IP port number>]
//
//
LOCAL int parseDirectoryFile (const char *pFileName)
{

	FILE	*pf;
	int	nPV;

	//
	// open a file that contains the PV directory
	//
	pf = fopen(pFileName, "r");
	if (!pf) {
		fprintf(stderr, "Directory file access probems with file=\"%s\" because \"%s\"\n",
			pFileName, strerror(errno));
		return (-1);
	}

	nPV =  parseDirectoryFP (pf, pFileName);
	
	fclose (pf);

	return nPV;
}

//
// parseDirectoryFP()
//
// PV directory file is expected to have entries of the form:
// <PV name> <host name or dotted ip address> [<optional IP port number>]
//
//
LOCAL int parseDirectoryFP (FILE *pf, const char *pFileName)
{
	pvInfo *pPVI;
	char 	pvNameStr[128];
	struct sockaddr_in ipa;
	char hostNameStr[128];
	int nPV=0;
	
	ipa.sin_family = AF_INET;
	while (TRUE) {
	
		//
		// parse the PV name entry from the file
		//
		if (fscanf(pf, " %127s ", pvNameStr) != 1) {
			return nPV; // success
		}

		//	
		// parse out server address
		//
		if (fscanf(pf, " %s ", hostNameStr) == 1) {
			
			ipa.sin_addr.s_addr = inet_addr(hostNameStr);
			if (ipa.sin_addr.s_addr==INADDR_NONE) {
				struct hostent *pent;
				
				pent = gethostbyname(hostNameStr);
				if (!pent) {
					fprintf (pf, "Unknown host name=\"%s\" (or bad dotted ip addr) in \"%s\" with PV=\"%s\"?\n", 
						hostNameStr, pFileName, pvNameStr);
					return -1;
				}
				if (pent->h_addrtype != AF_INET) {
					fprintf (pf, "Unknown addr type for host=\"%s\" in \"%s\" with PV=\"%s\" addr type=%u?\n", 
						hostNameStr, pFileName, pvNameStr, pent->h_addrtype);
					return -1;
				}
				assert (pent->h_addr_list[0]);
				assert (pent->h_length==sizeof(ipa.sin_addr));

				memcpy ((char *)&ipa.sin_addr, pent->h_addr_list[0], pent->h_length);
			
			}
		}
		else {
			fprintf (stderr,"No host name (or dotted ip addr) after PV name in \"%s\" with PV=\"%s\"?\n", 
					pFileName, pvNameStr);
			return -1;
		}
		
		//
		// parse out optional IP port number
		//
		unsigned portNumber;
		if (fscanf(pf, " %u ", &portNumber) == 1) {
			if (portNumber>0xffff) {
				fprintf (stderr,"Port number supplied is to large in \"%s\" with PV=\"%s\" host=\"%s\" port=%u?\n", 
					pFileName, pvNameStr, hostNameStr, portNumber);
				return -1;
			}
			
			ipa.sin_port = (aitUint16) portNumber;
		}
		else {
			ipa.sin_port = 0u; // will default to this server's port
		}

		pPVI = new pvInfo (pvNameStr, ipa);
		if (!pPVI) {
			fprintf (stderr, "Unable to allocate space for a new PV in \"%s\" with PV=\"%s\" host=\"%s\"\n",
					pFileName, pvNameStr, hostNameStr);
			return -1;
		}
		nPV++;
	}
}
