/*
 *      /share/src/drv $Id$
 *
 *      driver for hpe1445a arbitrary function generator VXI modules
 *
 *      Author:     Jeff Hill
 *      Date:       082492 
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *      Modification Log:
 *      -----------------
 *	.01 021192 joh	Fixed hpe1445aUnloadWaveformLocked() ANSI C 
 *			function prototype missmatch. Changed
 *			unsigned short to unsigned. 
 *		
 *
 */

static char	*sccsId = "$Id$\t$Date$";

#include <vxWorks.h>
#include <stdioLib.h>
#include <taskLib.h>
#include <module_types.h>
#include <fast_lock.h>
#include <drvEpvxi.h>

/*
 * comment out this line if you prefer
 * to use backplane ttl trigger 0
 */
#define FRONT_PANEL_TRIGGER

#define VXI_MODEL_HPE1445A 	(418)

LOCAL
int hpe1445aDriverId;

#define HPE1445A_MAX_POINTS	0x40000	
#define HPE1445A_MIN_POINTS	4	

#define HPE1445_DATA_PORT_OFFSET	(0x26)

struct hpe1445aConfig {
	FAST_LOCK	lck;
	char		device_active;
	double		dacPeakAmplitude;
	double		dacOffset;
};

#define HPE1445A_PCONFIG(LA) \
epvxiPConfig((LA), hpe1445aDriverId, struct hpe1445aConfig *)


/*
 *
 * For External Use
 *
 */
long	hpe1445aInit(void);
long 	hpe1445aSetupDAC(unsigned la, double dacPeakAmplitude, double dacOffset);
long	hpe1445aSetupFreq(unsigned la, char *pFreqString);
void	hpe1445aIoReport(unsigned la, int level);
long 	hpe1445aLoadWaveform(unsigned la, char	*pWaveformName, 
		double	*pdata, unsigned long	npoints);
long 	hpe1445aUnloadWaveform(unsigned la, char *pWaveformName);
long	hpe1445aActivateWaveform(unsigned la, char *pWaveformName);
long	hpe1445aTest(unsigned la);
long 	hpe1445aWriteWithLineno(unsigned la, char *pmsg, unsigned lineno);
void 	hpe1445aLogErrorsWithLineno(unsigned la, int lineno);


/*
 *
 * For Driver Internal Use
 *
 */
static void 	hpe1445aInitCard(unsigned la);
static long	hpe1445aReset(unsigned la);
static long	logEntireError(unsigned la, int lineno);
static long	hpe1445aActivateWaveformLocked(unsigned la, char *pWaveformName,
		 struct hpe1445aConfig	*pc);
static long	hpe1445aSetupFunction(unsigned la);
static long	hpe1445aSetupOutput(unsigned la);
static long	hpe1445aArm(unsigned la);
static long	hpe1445aUnloadWaveformLocked(unsigned la, char *pWaveformName);
static long 	hpe1445aLoadWaveformLocked(unsigned la, struct hpe1445aConfig *pc, 
		char *pWaveformName, double *pdata, unsigned long npoints);


#define logErrors(LA) hpe1445aLogErrorsWithLineno(LA, __LINE__)
#define hpe1445aWrite(LA, PMSG) hpe1445aWriteWithLineno(LA, PMSG, __LINE__)

#define HPE1445A_MAX_NAME_LENGTH 12
#define SEGMENT_NAME_PREFIX	"e_"
#define SEQUENCE_NAME_PREFIX	"e__"
#define PREFIX_MAX_NAME_LENGTH	3
#define MAX_NAME_LENGTH		(HPE1445A_MAX_NAME_LENGTH-PREFIX_MAX_NAME_LENGTH) 

#define	TEST_FILE_NAME	"~hill/hpe1445a.dat"


/*
 *
 *	hpe1445aTest()
 *
 *
 */
long 
hpe1445aTest(unsigned la)
{
	int	s;
	FILE	*pf;
	char	*pfn = TEST_FILE_NAME;
	double	*pwaveform;
	int	nsamples;
	int	i;

	{
		int	options;
		s = taskOptionsGet(taskIdSelf(), &options);
		if(s < 0){
			return ERROR;
		}
		if(!(options&VX_STDIO)){
			logMsg("%s: task needs SDIO option set\n",__FILE__);
			return ERROR;
		}
	}
	
	pf = fopen(pfn, "r");
	if(!pf){
		logMsg("%s: file access problems %s\n", __FILE__, pfn);
		fclose(pf);
		return ERROR;
	}
	s = fscanf(pf, "%d", &nsamples);
	if(s!=1){
		logMsg("%s: no element count in the file %s\n", __FILE__, pfn);
		fclose(pf);
		return ERROR;
	}

	pwaveform = (double *) calloc(nsamples, sizeof(double));
	if(!pwaveform){
		logMsg("%s: unable to allocate enough memory  s\n", __FILE__, pfn);
		fclose(pf);
		return ERROR;
	}

	for(i=0; i<nsamples; i++){
		s = fscanf(pf, "%lf", &pwaveform[i]);
		if(s != 1){
			logMsg("%s: bad file format %s\n", __FILE__, pfn);
			fclose(pf);
			return ERROR;
		}
	}

	s = hpe1445aLoadWaveform(la, "fred", pwaveform, nsamples);
	if(s<0){
		logMsg("bomb 1\n");
		return ERROR;
	}
	s = hpe1445aUnloadWaveform(la, "fred");
	if(s<0){
		logMsg("bomb 2\n");
		return ERROR;
	}
	s = hpe1445aLoadWaveform(la, "fred", pwaveform, nsamples);
	if(s<0){
		logMsg("bomb 3\n");
		return ERROR;
	}

	s = hpe1445aLoadWaveform(la, "willma", pwaveform, nsamples);
	if(s<0){
		logMsg("bomb 4\n");
		return ERROR;
	}

	s = hpe1445aActivateWaveform(la, "fred");
	if(s<0){
		logMsg("bomb 5\n");
		return ERROR;
	}
	s = hpe1445aActivateWaveform(la, "willma");
	if(s<0){
		logMsg("bomb 6\n");
		return ERROR;
	}

	return OK;
}


/*
 * hpe1445aInit
 *
 * initialize all hpe1445a cards
 *
 */
long hpe1445aInit(void)
{
        int     s;

        /*
         * do nothing on crates without VXI
         */
        if(!epvxiResourceMangerOK){
                return OK;
        }

	hpe1445aDriverId = epvxiUniqueDriverID();

        s = epvxiRegisterModelName(
			VXI_MAKE_HP,
                        VXI_MODEL_HPE1445A,
                        "Arbitrary Function Generator");
        if(s<0){
        	logMsg(	"%s: failed to register model at init\n",
			__FILE__);
        }
	s = epvxiRegisterMakeName(VXI_MAKE_HP, "Hewlett-Packard");
	if(s<0){
		logMsg( "%s: failed to register make at init\n", 
			__FILE__);
	}

        {
                epvxiDeviceSearchPattern  dsp;

                dsp.flags = VXI_DSP_make | VXI_DSP_model;
                dsp.make = VXI_MAKE_HP;
                dsp.model = VXI_MODEL_HPE1445A;
                s = epvxiLookupLA(&dsp, hpe1445aInitCard, (void *)NULL);
                if(s<0){
                        return ERROR;
                }
        }

        return OK;
}



/*
 *	hpe1445aInitCard() 
 *
 * initialize single at5vxi card
 *
 *
 */
LOCAL
void hpe1445aInitCard(
unsigned 	la
)
{
        int                     s;
	struct hpe1445aConfig	*pc;
	
	s = epvxiOpen(
		la,
		hpe1445aDriverId,
		(unsigned long) sizeof(*pc),
		hpe1445aIoReport);	
	if(s<0){
		logMsg(	"%s:, Device open failed la=%d status=%d\n",
			__FILE__,
			la,
			s);
		return;
	}

	pc = HPE1445A_PCONFIG(la);
	if(!pc){
		return;
	}
	
	/*
	 *
 	 * set configuration to a known state
	 *
	 */
	s = hpe1445aReset(la);
	if(s<0){
		epvxiClose(la, hpe1445aDriverId);
		return;
	}

	/*
	 * set the download format to signed
	 */
	s = hpe1445aWrite(
			la, 
			"source:arbitrary:dac:format signed");
	if(s<0){
		return;
	}

	s = hpe1445aSetupFreq(la, "10 KHz");
	if(s<0){
		epvxiClose(la, hpe1445aDriverId);
		return;
	}
	
	/*
	 *
 	 * set the waveform amplitude and offset
 	 * LSB is 1/4095 of this value
	 *
 	 * ie the lsb is 1.25 mv
	 */
	s = hpe1445aSetupDAC(la, 5.11875, 0.0);
	if(s<0){
		return;
	}

	s = hpe1445aSetupFunction(la);
	if(s<0){
		epvxiClose(la, hpe1445aDriverId);
		return;
	}

	s = hpe1445aSetupOutput(la);
	if(s<0){
		epvxiClose(la, hpe1445aDriverId);
		return;
	}

	s = hpe1445aArm(la);
	if(s<0){
		epvxiClose(la, hpe1445aDriverId);
		return;
	}

	/*
	 * check for errors one last time 
	 */
	logErrors(la);

}


/*
 *	hpe1445aLogErrorsWithLineno()
 */
void hpe1445aLogErrorsWithLineno(
unsigned	la,
int		lineno
)
{
	int		s;


	while(TRUE){
		s = hpe1445aWrite(la, "SYST:ERR?");
		if(s<0){
			break;
		}
		s = logEntireError(la, lineno);
		if(s<0){
			break;
		}
	}	
}


/*
 *
 * 	logEntireError()
 *
 */
LOCAL
long	logEntireError(
unsigned	la,
int		lineno
)
{
	int		s;
	char		pbuf[64];
	unsigned long	read_count;
	int		nreads = 0;

	while(TRUE){
		s = epvxiRead(
			la, 
			pbuf, 
			sizeof(pbuf), 
			&read_count,
			0);
		if(s!=VXI_BUFFER_FULL && s!=VXI_SUCCESS){
			logMsg(	"%s line=%d LA=0X%X: error fetch problem %d\n",
				__FILE__,
				lineno,
				la,
				s);
			return ERROR;
		}
		nreads++;
		/*
		 * return of zero indicates no errors
		 * this will always be a very short message
		 */
		if(nreads==1){
			int val;
			int n;

			n = sscanf(pbuf,"%d",&val);
			if(n==1){
				if(val==0){
					return ERROR;
				}
			}
		}
		logMsg(	"%s line=%d LA=0X%X: Device Error => %s\n",
			__FILE__,
			lineno,
			la,
			pbuf);

		if(s==VXI_SUCCESS){
			break;
		}
	}

	return OK;
}


/*
 *
 *	hpe1445aReset()
 *
 *
 */
LOCAL
long hpe1445aReset(unsigned la)
{
	int 	s;

	s = hpe1445aWrite(la, "*RST");
	if(s<0){
		return ERROR;
	}

	s = hpe1445aWrite(la, "source:list1:ssequence:delete:all");
	if(s<0){
		return ERROR;
	}

	s = hpe1445aWrite(la, "source:list1:segment:delete:all");
	if(s<0){
		return ERROR;
	}

	return OK;
}


/*
 *
 *	hpe1445aSetupFreq()
 *
 *
 */
long	hpe1445aSetupFreq(unsigned la, char *pFreqString)
{
	char	pbuf[64];
	int 	s;

	/*
	 *
	 * Set the sample rate
	 *
	 */
	sprintf(pbuf, "source:frequency:fixed %s", pFreqString);
	s = hpe1445aWrite(la, pbuf);
	if(s<0){
		return ERROR;
	}

	return OK;
}


/*
 *
 *	hpe1445aSetupFunction()
 *
 *
 */
LOCAL
long	hpe1445aSetupFunction(unsigned la)
{
	int 	s;

	/*
	 *
 	 * set the function to arbitrary waveform
	 *
	 */
	s = hpe1445aWrite(la, "source:function:shape user");
	if(s<0){
		return ERROR;
	}

	return OK;
}


/*
 *
 *	hpe1445aSetupOutput()
 *
 *
 */
LOCAL
long	hpe1445aSetupOutput(unsigned la)
{
	int	s;


	/*
 	 * set the output impedance
	 * (50 Ohm coax assumed)
	 *
	 */
	s = hpe1445aWrite(la, "output:impedance 50 Ohm");
	if(s<0){
		return ERROR;
	}

	/*
	 *
 	 * disable output low pass filter
	 * (freq would need to be set if it were turned on)
	 *
	 */
	s = hpe1445aWrite(la, "output:filter:lpass:state off");
	if(s<0){
		return ERROR;
	}

	return OK;
}


/*
 *	hpe1445aArm() 
 *
 */
long	hpe1445aArm(unsigned la)
{
	int	s;

	/*
	 *
 	 * initiate waveform output off the external trigger input 
	 *
	 */
#ifdef FRONT_PANEL_TRIGGER
	s = hpe1445aWrite(la, "arm:start:layer2:source external");
#else
	s = hpe1445aWrite(la, "arm:start:layer2:source ttltrg0");
#endif
	if(s<0){
		return ERROR;
	}

	/*
	 *
 	 * initiate waveform output on the rising edge 
	 *
	 */
	s = hpe1445aWrite(la, "arm:start:layer2:slope positive");
	if(s<0){
		return ERROR;
	}

	/*
	 *
 	 * output the waveform once only after receiving
	 * a trigger 
	 *
	 */
	s = hpe1445aWrite(la, "arm:start:layer1:count 1");
	if(s<0){
		return ERROR;
	}

	/*
	 *
	 * output the waveform after each trigger edge
	 * forever
	 *
	 */
	s = hpe1445aWrite(la, "arm:start:layer2:count infinity");
	if(s<0){
		return ERROR;
	}

	return OK;
}


/*
 *
 *
 * 	hpe1445aSetupDAC
 *
 *
 *
 */
long
hpe1445aSetupDAC(
unsigned 	la,
double		dacPeakAmplitude,
double		dacOffset)
{
	char			pbuf[64];
	int			s;
        struct hpe1445aConfig	*pc;

	pc = HPE1445A_PCONFIG(la);
	if(!pc){
		logMsg("%s: device offline\n", __FILE__);
		return ERROR;
	}

	sprintf(	pbuf, 
			"source:voltage:level:immediate:amplitude %lf V", 
			dacPeakAmplitude);
	s = hpe1445aWrite(la, pbuf);
	if(s<0){
		return ERROR;
	}
	pc->dacPeakAmplitude = dacPeakAmplitude;

	sprintf(	pbuf, 
			"source:voltage:level:immediate:offset %lf V", 
			dacOffset);
	s = hpe1445aWrite(la, pbuf);
	if(s<0){
		return ERROR;
	}
	pc->dacOffset = dacOffset;

	logErrors(la);

	return OK;
}



/*
 *
 *	hpe1445aActivateWaveform() 
 *
 *
 *
 */
long
hpe1445aActivateWaveform(
unsigned 	la,
char		*pWaveformName
)
{
	int			s;
        struct hpe1445aConfig	*pc;

	pc = HPE1445A_PCONFIG(la);
	if(!pc){
		logMsg("%s: device offline\n", __FILE__);
		return ERROR;
	}

	FASTLOCK(&pc->lck);

	s = hpe1445aActivateWaveformLocked(la, pWaveformName, pc);

	FASTUNLOCK(&pc->lck);

	return s;
}


/*
 *
 *	hpe1445aActivateWaveformLocked() 
 *
 *
 *
 */
LOCAL long 
hpe1445aActivateWaveformLocked(
unsigned 		la,
char			*pWaveformName,
struct hpe1445aConfig	*pc
)
{
	char		pbuf[64];
	int		s;

	if(pc->device_active){
		s = hpe1445aWrite(la, "abort");
		if(s<0){
			return ERROR;
		}
		pc->device_active = FALSE;
	}

	/*
	 *
	 * select active sequence
	 *
	 */ 
	sprintf(	pbuf, 
			"source:function:user %s%s", 
			SEQUENCE_NAME_PREFIX,
			pWaveformName);
	s = hpe1445aWrite(la, pbuf);
	if(s<0){
		return ERROR;
	}
	
	/*
	 *
 	 * initiate the trigger system 
	 *
	 */
	s = hpe1445aWrite(la, "initiate:immediate");
	if(s<0){
		return ERROR;
	}
	pc->device_active = TRUE;

	logErrors(la);

	return OK;
}



/*
 *
 *	hpe1445aUnloadWaveform() 
 *
 *
 *
 */
long
hpe1445aUnloadWaveform(
unsigned	la,
char		*pWaveformName
)
{
	int			s;
        struct hpe1445aConfig	*pc;

	pc = HPE1445A_PCONFIG(la);
	if(!pc){
		logMsg("%s: device offline\n", __FILE__);
		return ERROR;
	}

	FASTLOCK(&pc->lck);

	s = hpe1445aUnloadWaveformLocked(la, pWaveformName);

	FASTUNLOCK(&pc->lck);

	logErrors(la);

	return s;
}


/*
 *
 *	hpe1445aUnloadWaveformLocked() 
 *
 *
 *
 */
LOCAL long	 
hpe1445aUnloadWaveformLocked(
unsigned 	la,
char		*pWaveformName
)
{
	char			pbuf[64];
	int			s;

	sprintf(
		pbuf, 
		"source:list:ssequence:select %s%s", 
		SEQUENCE_NAME_PREFIX, 
		pWaveformName);
	s = hpe1445aWrite(la, pbuf); 
	if(s<0){
		return ERROR;
	}

	s = hpe1445aWrite(la, "source:list:ssequence:delete:selected"); 
	if(s<0){
		return ERROR;
	}

	sprintf(
		pbuf, 
		"source:list:segment:select %s%s", 
		SEGMENT_NAME_PREFIX, 
		pWaveformName);
	s = hpe1445aWrite(la, pbuf); 
	if(s<0){
		return ERROR;
	}

	s = hpe1445aWrite(la, "source:list:segment:delete:selected"); 
	if(s<0){
		return ERROR;
	}

	return OK;
}



/*
 *
 *	hpe1445aLoadWaveform() 
 *
 *
 *
 */
long
hpe1445aLoadWaveform(
unsigned 	la,
char		*pWaveformName,
double		*pdata,
unsigned long	npoints
)
{
	int			s;
        struct hpe1445aConfig	*pc;

	pc = HPE1445A_PCONFIG(la);
	if(!pc){
		logMsg("%s: device offline\n", __FILE__);
		return ERROR;
	}
	
	if(strlen(pWaveformName)>MAX_NAME_LENGTH){
		logMsg("%s: waveform element name to long\n", __FILE__);
		return ERROR;
	}

	if(npoints<HPE1445A_MIN_POINTS){
		logMsg("%s: waveform element count to small\n", __FILE__);
		return ERROR;
	}

	FASTLOCK(&pc->lck);

	s = hpe1445aLoadWaveformLocked(la, pc, pWaveformName, pdata, npoints);

	FASTUNLOCK(&pc->lck);

	return s;
}

	
/*
 *
 * hpe1445aLoadWaveformLocked()
 *
 *
 */
LOCAL long 
hpe1445aLoadWaveformLocked(
unsigned 		la,
struct hpe1445aConfig	*pc,
char			*pWaveformName,
double			*pdata,
unsigned long		npoints
)
{
	unsigned long		read_count;
	char			pbuf[64];
        struct vxi_csr		*pcsr;
	int			s;

	s = hpe1445aWrite(la, "source:list:segment:free?"); 
	if(s<0){
		return ERROR;
	}
	s = epvxiRead(
		la, 
		pbuf, 
		sizeof(pbuf), 
		&read_count,
		0);
	if(s!=VXI_SUCCESS){
		logMsg("\"source:list:segment:free?\" query failed\n");
		return ERROR;
	}	

	{
		int	nfree;
		int	nused;

		s = sscanf(pbuf, "%d,%d", &nfree, &nused);
		if(s!=2){
			return ERROR;
		}
		if(nfree < npoints){
			logMsg("%s: %d waveform elements available\n", nfree);
			logMsg("%s: %d element waveform rejected\n", npoints);
			return ERROR;
		}
	}
	

	/*
	 *
 	 * select the segment for subsequent
	 * commands
	 *
	 */
	sprintf(
		pbuf, 
		"source:list:segment:select %s%s", 
		SEGMENT_NAME_PREFIX, 
		pWaveformName);
	s = hpe1445aWrite(la, pbuf); 
	if(s<0){
		return ERROR;
	}


	sprintf(pbuf, "source:list:segment:define %u", npoints);
	s = hpe1445aWrite(la, pbuf); 
	if(s<0){
		return ERROR;
	}
	sprintf(	pbuf, 
			"source:arbitrary:download VXI,%s%s,%d", 
			SEGMENT_NAME_PREFIX,
			pWaveformName,
			npoints);
	s = hpe1445aWrite(la, pbuf); 
	if(s<0){
		return ERROR;
	}

	/*
	 * wait for the device to finish with the download
	 * command prior to the backplane download
	 */
	s = hpe1445aWrite(la, "*OPC?"); 
	if(s<0){
		return ERROR;
	}
	s = epvxiRead(
		la, 
		pbuf, 
		sizeof(pbuf), 
		&read_count,
		0);
	if(s!=VXI_SUCCESS){
		logMsg("\"*OPC?\" query failed\n");
		return ERROR;
	}	

	{
		double		truncationOffset;
		double		dacPeakAmplitude;
		double		dacOffset;
		double		*pwf;
		unsigned short	*pdata_port;

		pdata_port = (unsigned short *) epvxiA24Base(la);
		pdata_port += (HPE1445_DATA_PORT_OFFSET/sizeof(*pdata_port));
		dacPeakAmplitude = pc->dacPeakAmplitude;
		dacOffset = pc->dacOffset;
		truncationOffset = dacPeakAmplitude/(4095*2);
		for(pwf=pdata; pwf < &pdata[npoints]; pwf++){
			short	idata;
			double	fdata;
		
			/*
			 * extra step here preserves precision 
			 */
			fdata = (*pwf-dacOffset)*4095;
			fdata = fdata/dacPeakAmplitude;
			/*
			 * This offset causes round up to occur and
			 * not truncation
			 */
			fdata += truncationOffset;
			idata = (short) fdata; 
			idata = idata << 3;

			if(pwf == &pdata[npoints-4]){
#				define LAST_POINT 1
				idata |= LAST_POINT;
			}

			/*
			 * load the waveform element into the 1445
			 */
			*pdata_port = idata;
		}
	}

	s = hpe1445aWrite(la, "source:arbitrary:download:complete"); 
	if(s<0){
		return ERROR;
	}

	/*
	 *
	 * install this segment into the sequence
	 *
	 */ 
	sprintf(	pbuf, 
			"source:list:ssequence:select %s%s", 
			SEQUENCE_NAME_PREFIX,
			pWaveformName);
	s = hpe1445aWrite(la, pbuf);
	if(s<0){
		return ERROR;
	}
	/*
	 *
	 * set the number of segments in this sequence
	 *
	 */ 
	s = hpe1445aWrite(la, "source:list:ssequence:define 1");
	if(s<0){
		return ERROR;
	}
	sprintf(pbuf, 
		"source:list:ssequence:sequence %s%s", 
		SEGMENT_NAME_PREFIX,
		pWaveformName);
	s = hpe1445aWrite(la, pbuf);
	if(s<0){
		return ERROR;
	}
	s = hpe1445aWrite(la, "source:list:ssequence:dwell:count 1");
	if(s<0){
		return ERROR;
	}

	return OK;
}


/*
 *
 *	hpe1445aWriteWithLineno() 
 *
 */
long hpe1445aWriteWithLineno(
	unsigned	la,
	char		*pmsg,
	unsigned	lineno 
)
{
	unsigned long	nactual;
	int		s;

	s = epvxiWrite(
			la,
			pmsg,
			strlen(pmsg),
			&nactual,
			0);
	if(s<0){
		logMsg(	"%s: VXI message write failed\n",
			__FILE__);
		logMsg("%s: LA=0X%02X LINE=%d MSG=%s\n",
			__FILE__,
			la,
			lineno,
			pmsg);
		return ERROR;
	}

	return OK;
}


/*
 *
 *	hpe1445aIoReport()
 *
 *
 */
void	hpe1445aIoReport(unsigned la, int level)	
{
	struct hpe1445aConfig	*pc;
	char			*pStateName[] = {"in",""};

	pc = HPE1445A_PCONFIG(la);
	if(!pc){
		return;
	}

	if(level>0){
		printf("\tdevice %sactive, DAC peak = %lf V, DAC offset = %lf V\n",
			pStateName[pc->device_active],
			pc->dacPeakAmplitude,
			pc->dacOffset);
	}
}
