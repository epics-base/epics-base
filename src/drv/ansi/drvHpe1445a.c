/* base/src/drv $Id$ */
/*
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

static char	*sccsId = "@(#)drvHpe1445a.c	1.5\t8/27/93";

#include <vxWorks.h>
#include <stdioLib.h>
#include <taskLib.h>
#include <string.h>

#include  "dbDefs.h"
#include  "errlog.h"
#include <module_types.h>
#include <fast_lock.h>
#include <devLib.h>
#include <drvEpvxi.h>

/*
 * comment out this line if you prefer
 * to use backplane ttl trigger 0
 */
#define FRONT_PANEL_TRIGGER

#define VXI_MODEL_HPE1445A 	(418)

LOCAL int hpe1445aDriverId;

#define HPE1445A_MAX_POINTS	0x40000	
#define HPE1445A_MIN_POINTS	4	

#define HPE1445_DATA_PORT_OFFSET	(0x26)

struct hpe1445aConfig {
	FAST_LOCK	lck;
	char		device_active;
	double		dacPeakAmplitude;
	double		dacOffset;
	char		buf[256];
};

#define HPE1445A_PCONFIG(LA, PC) \
epvxiFetchPConfig((LA), hpe1445aDriverId, (PC))

typedef long hpe1445aStat;

/*
 *
 * For External Use
 *
 */
hpe1445aStat hpe1445aInit(void);
hpe1445aStat hpe1445aSetupDAC(unsigned la, double dacPeakAmplitude, double dacOffset);
hpe1445aStat hpe1445aSetupFreq(unsigned la, char *pFreqString);
void         hpe1445aIoReport(unsigned la, int level);
hpe1445aStat hpe1445aLoadWaveform(unsigned la, char	*pWaveformName, 
		double	*pdata, unsigned long	npoints);
hpe1445aStat hpe1445aUnloadWaveform(unsigned la, char *pWaveformName);
hpe1445aStat hpe1445aActivateWaveform(unsigned la, char *pWaveformName);
hpe1445aStat hpe1445aTest(unsigned la);
hpe1445aStat hpe1445aWriteWithLineno(unsigned la, char *pmsg, unsigned lineno);
void hpe1445aLogErrorsWithLineno(unsigned la, int lineno);


/*
 *
 * For Driver Internal Use
 *
 */
LOCAL void 	hpe1445aInitCard(unsigned la, void *pArg);
LOCAL hpe1445aStat	hpe1445aReset(unsigned la);
LOCAL hpe1445aStat	logEntireError(unsigned la, int lineno);
LOCAL hpe1445aStat	hpe1445aActivateWaveformLocked(unsigned la, char *pWaveformName,
		 struct hpe1445aConfig	*pc);
LOCAL hpe1445aStat	hpe1445aSetupFunction(unsigned la);
LOCAL hpe1445aStat	hpe1445aSetupOutput(unsigned la);
LOCAL hpe1445aStat	hpe1445aArm(unsigned la);
LOCAL hpe1445aStat	hpe1445aUnloadWaveformLocked(unsigned la, char *pWaveformName);
LOCAL hpe1445aStat 	hpe1445aLoadWaveformLocked(unsigned la, 
				struct hpe1445aConfig *pc, 
				char *pWaveformName, double *pdata, 
				unsigned long npoints);


#define logErrors(LA) hpe1445aLogErrorsWithLineno(LA, __LINE__)
#define hpe1445aWrite(LA, PMSG) hpe1445aWriteWithLineno(LA, PMSG, __LINE__)

#define HPE1445A_MAX_NAME_LENGTH 12
#define SEGMENT_NAME_PREFIX	"e_"
#define SEQUENCE_NAME_PREFIX	"e__"
#define PREFIX_MAX_NAME_LENGTH	3
#define MAX_NAME_LENGTH		(HPE1445A_MAX_NAME_LENGTH-PREFIX_MAX_NAME_LENGTH) 

#define	TEST_FILE_NAME	"hpe1445a.dat"


/*
 *
 *	hpe1445aTest()
 *
 *
 */
hpe1445aStat hpe1445aTest(unsigned la)
{
	hpe1445aStat	s;
	FILE		*pf;
	char		*pfn = TEST_FILE_NAME;
	double		*pwaveform;
	int		nsamples;
	int		i;

	pf = fopen(pfn, "r");
	if(!pf){
		s = S_dev_internal;
		errPrintf(
			s, 
			__FILE__,
			__LINE__,
			"file access problems %s", 
			pfn);
		fclose(pf);
		return s;
	}
	s = fscanf(pf, "%d", &nsamples);
	if(s!=1){
		s = S_dev_internal;
		errPrintf(
			s, 
			__FILE__,
			__LINE__,
			"no element count in the file %s", 
			pfn);
		fclose(pf);
		return s;
	}

	pwaveform = (double *) calloc(nsamples, sizeof(double));
	if(!pwaveform){
		s = S_dev_noMemory;
		errPrintf(
			s, 
			__FILE__,
			__LINE__,
			"specified sample count to large %s", 
			pfn);
		fclose(pf);
		return s;
	}

	for(i=0; i<nsamples; i++){
		s = fscanf(pf, "%lf", &pwaveform[i]);
		if(s != 1){
			s = S_dev_internal;
			errPrintf(
				s, 
				__FILE__,
				__LINE__,
				"waveform element has bad file format %s", 
				pfn);
			fclose(pf);
			return s;
		}
	}

	s = hpe1445aLoadWaveform(la, "fred", pwaveform, nsamples);
	if(s){
		errMessage(s,NULL);
		return s;
	}
	s = hpe1445aUnloadWaveform(la, "fred");
	if(s){
		errMessage(s,NULL);
		return s;
	}
	s = hpe1445aLoadWaveform(la, "fred", pwaveform, nsamples);
	if(s){
		errMessage(s,NULL);
		return s;
	}

	s = hpe1445aLoadWaveform(la, "willma", pwaveform, nsamples);
	if(s){
		errMessage(s,NULL);
		return s;
	}

	s = hpe1445aActivateWaveform(la, "fred");
	if(s){
		errMessage(s,NULL);
		return s;
	}
	s = hpe1445aActivateWaveform(la, "willma");
	if(s){
		errMessage(s,NULL);
		return s;
	}

	return VXI_SUCCESS;
}


/*
 * hpe1445aInit
 *
 * initialize all hpe1445a cards
 *
 */
hpe1445aStat hpe1445aInit(void)
{
	hpe1445aStat s;

        /*
         * do nothing on crates without VXI
         */
        if(!epvxiResourceMangerOK){
                return VXI_SUCCESS;
        }

	hpe1445aDriverId = epvxiUniqueDriverID();

        s = epvxiRegisterModelName(
			VXI_MAKE_HP,
                        VXI_MODEL_HPE1445A,
                        "Arbitrary Function Generator");
        if(s){
		errMessage(s, NULL);
        }
	s = epvxiRegisterMakeName(VXI_MAKE_HP, "Hewlett-Packard");
        if(s){
		errMessage(s, NULL);
        }

        {
                epvxiDeviceSearchPattern  dsp;

                dsp.flags = VXI_DSP_make | VXI_DSP_model;
                dsp.make = VXI_MAKE_HP;
                dsp.model = VXI_MODEL_HPE1445A;
                s = epvxiLookupLA(&dsp, hpe1445aInitCard, (void *)NULL);
                if(s){
			errMessage(s, NULL);
                        return s;
                }
        }

        return VXI_SUCCESS;
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
unsigned 	la,
void		*pArg
)
{
        hpe1445aStat		s;
	struct hpe1445aConfig	*pc;
	
	s = epvxiOpen(
		la,
		hpe1445aDriverId,
		(unsigned long) sizeof(*pc),
		hpe1445aIoReport);	
	if(s){
		errPrintf(
			s,
			__FILE__,
			__LINE__,
			"la=0X%X",
			la);
		return;
	}

	s = HPE1445A_PCONFIG(la, pc);
	if(s){
		errMessage(s,NULL);
		return;
	}
	
	/*
	 *
 	 * set configuration to a known state
	 *
	 */
	s = hpe1445aReset(la);
	if(s){
		errMessage(s,NULL);
		epvxiClose(la, hpe1445aDriverId);
		return;
	}

	/*
	 * set the download format to signed
	 */
	s = hpe1445aWrite(
			la, 
			"source:arbitrary:dac:format signed");
	if(s){
		errMessage(s,NULL);
		return;
	}

	s = hpe1445aSetupFreq(la, "10 KHz");
	if(s){
		errMessage(s,NULL);
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
	if(s){
		errMessage(s,NULL);
		return;
	}

	s = hpe1445aSetupFunction(la);
	if(s){
		errMessage(s,NULL);
		epvxiClose(la, hpe1445aDriverId);
		return;
	}

	s = hpe1445aSetupOutput(la);
	if(s){
		errMessage(s,NULL);
		epvxiClose(la, hpe1445aDriverId);
		return;
	}

	s = hpe1445aArm(la);
	if(s){
		errMessage(s,NULL);
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
	hpe1445aStat	s;

	while(TRUE){
		s = hpe1445aWrite(la, "SYST:ERR?");
		if(s){
			break;
		}
		s = logEntireError(la, lineno);
		if(s){
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
hpe1445aStat	logEntireError(
unsigned	la,
int		lineno
)
{
	hpe1445aStat		s;
	struct hpe1445aConfig	*pc;
	unsigned long		read_count;
	int			nreads = 0;

	s = HPE1445A_PCONFIG(la, pc);
	if(s){
		errMessage(s,NULL);
		return s;
	}

	while(TRUE){
		s = epvxiRead(
			la, 
			pc->buf, 
			sizeof(pc->buf), 
			&read_count,
			0);
		if(s!=S_epvxi_bufferFull && s!=VXI_SUCCESS){
			errPrintf(
				s,
				__FILE__,
				lineno,
				"error fetch problem at LA=0X%X",
				la);
			return s;
		}
		nreads++;
		/*
		 * return of zero indicates no errors
		 * this will always be a very short message
		 */
		if(nreads==1){
			int val;
			int n;

			n = sscanf(pc->buf,"%d",&val);
			if(n==1){
				if(val==0){
					return S_epvxi_internal;
				}
			}
		}
		errPrintf(
			S_epvxi_msgDeviceStatus,
			__FILE__,
			lineno,
			"LA=0X%X: Error => %s",
			la,
			pc->buf);

		if(s==VXI_SUCCESS){
			break;
		}
	}

	return VXI_SUCCESS;
}


/*
 *
 *	hpe1445aReset()
 *
 *
 */
LOCAL
hpe1445aStat hpe1445aReset(unsigned la)
{
	hpe1445aStat	s;

	s = hpe1445aWrite(la, "*RST");
	if(s){
		return s;
	}

	s = hpe1445aWrite(la, "source:list1:ssequence:delete:all");
	if(s){
		return s;
	}

	s = hpe1445aWrite(la, "source:list1:segment:delete:all");
	if(s){
		return s;
	}

	return VXI_SUCCESS;
}


/*
 *
 *	hpe1445aSetupFreq()
 *
 *
 */
hpe1445aStat	hpe1445aSetupFreq(unsigned la, char *pFreqString)
{
	hpe1445aStat		s;
	struct hpe1445aConfig	*pc;

	s = HPE1445A_PCONFIG(la, pc);
	if(s){
		errMessage(s,NULL);
		return s;
	}

	/*
	 *
	 * Set the sample rate
	 *
	 */
	sprintf(pc->buf, "source:frequency:fixed %s", pFreqString);
	s = hpe1445aWrite(la, pc->buf);
	if(s){
		return s;
	}

	return VXI_SUCCESS;
}


/*
 *
 *	hpe1445aSetupFunction()
 *
 *
 */
LOCAL
hpe1445aStat	hpe1445aSetupFunction(unsigned la)
{
	hpe1445aStat	s;

	/*
	 *
 	 * set the function to arbitrary waveform
	 *
	 */
	s = hpe1445aWrite(la, "source:function:shape user");
	if(s){
		return s;
	}

	return VXI_SUCCESS;
}


/*
 *
 *	hpe1445aSetupOutput()
 *
 *
 */
LOCAL
hpe1445aStat	hpe1445aSetupOutput(unsigned la)
{
	hpe1445aStat	s;


	/*
 	 * set the output impedance
	 * (50 Ohm coax assumed)
	 *
	 */
	s = hpe1445aWrite(la, "output:impedance 50 Ohm");
	if(s){
		return s;
	}

	/*
	 *
 	 * disable output low pass filter
	 * (freq would need to be set if it were turned on)
	 *
	 */
	s = hpe1445aWrite(la, "output:filter:lpass:state off");
	if(s){
		return s;
	}

	return VXI_SUCCESS;
}


/*
 *	hpe1445aArm() 
 *
 */
hpe1445aStat	hpe1445aArm(unsigned la)
{
	hpe1445aStat	s;

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
	if(s){
		return s;
	}

	/*
	 *
 	 * initiate waveform output on the rising edge 
	 *
	 */
	s = hpe1445aWrite(la, "arm:start:layer2:slope positive");
	if(s){
		return s;
	}

	/*
	 *
 	 * output the waveform once only after receiving
	 * a trigger 
	 *
	 */
	s = hpe1445aWrite(la, "arm:start:layer1:count 1");
	if(s){
		return s;
	}

	/*
	 *
	 * output the waveform after each trigger edge
	 * forever
	 *
	 */
	s = hpe1445aWrite(la, "arm:start:layer2:count infinity");
	if(s){
		return s;
	}

	return VXI_SUCCESS;
}


/*
 *
 *
 * 	hpe1445aSetupDAC
 *
 *
 *
 */
hpe1445aStat hpe1445aSetupDAC(
unsigned 	la,
double		dacPeakAmplitude,
double		dacOffset)
{
	hpe1445aStat		s;
        struct hpe1445aConfig	*pc;

	s = HPE1445A_PCONFIG(la,pc);
	if(s){
		errMessage(s,NULL);
		return s;
	}

	sprintf(pc->buf, 
		"source:voltage:level:immediate:amplitude %f V", 
		dacPeakAmplitude);
	s = hpe1445aWrite(la, pc->buf);
	if(s){
		return s;
	}
	pc->dacPeakAmplitude = dacPeakAmplitude;

	sprintf(pc->buf, 
		"source:voltage:level:immediate:offset %f V", 
		dacOffset);
	s = hpe1445aWrite(la, pc->buf);
	if(s){
		return s;
	}
	pc->dacOffset = dacOffset;

	logErrors(la);

	return VXI_SUCCESS;
}



/*
 *
 *	hpe1445aActivateWaveform() 
 *
 *
 *
 */
hpe1445aStat hpe1445aActivateWaveform(
unsigned 	la,
char		*pWaveformName
)
{
	hpe1445aStat		s;
        struct hpe1445aConfig	*pc;

	s = HPE1445A_PCONFIG(la, pc);
	if(s){
		errMessage(s,NULL);
		return s;
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
LOCAL hpe1445aStat 
hpe1445aActivateWaveformLocked(
unsigned 		la,
char			*pWaveformName,
struct hpe1445aConfig	*pc
)
{
	int		s;

	if(pc->device_active){
		s = hpe1445aWrite(la, "abort");
		if(s){
			return s;
		}
		pc->device_active = FALSE;
	}

	/*
	 *
	 * select active sequence
	 *
	 */ 
	sprintf(	pc->buf, 
			"source:function:user %s%s", 
			SEQUENCE_NAME_PREFIX,
			pWaveformName);
	s = hpe1445aWrite(la, pc->buf);
	if(s){
		return s;
	}
	
	/*
	 *
 	 * initiate the trigger system 
	 *
	 */
	s = hpe1445aWrite(la, "initiate:immediate");
	if(s){
		return s;
	}
	pc->device_active = TRUE;

	logErrors(la);

	return VXI_SUCCESS;
}



/*
 *
 *	hpe1445aUnloadWaveform() 
 *
 *
 *
 */
hpe1445aStat hpe1445aUnloadWaveform(
unsigned	la,
char		*pWaveformName
)
{
	hpe1445aStat		s;
        struct hpe1445aConfig	*pc;

	s = HPE1445A_PCONFIG(la, pc);
	if(s){
		errMessage(s, NULL);
		return s;
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
LOCAL hpe1445aStat hpe1445aUnloadWaveformLocked(
unsigned 	la,
char		*pWaveformName
)
{
	hpe1445aStat		s;
	struct hpe1445aConfig   *pc;

	s = HPE1445A_PCONFIG(la, pc);
	if(s){
		errMessage(s, NULL);
		return s;
	}

	sprintf(
		pc->buf, 
		"source:list:ssequence:select %s%s", 
		SEQUENCE_NAME_PREFIX, 
		pWaveformName);
	s = hpe1445aWrite(la, pc->buf); 
	if(s){
		return s;
	}

	s = hpe1445aWrite(la, "source:list:ssequence:delete:selected"); 
	if(s){
		return s;
	}

	sprintf(
		pc->buf, 
		"source:list:segment:select %s%s", 
		SEGMENT_NAME_PREFIX, 
		pWaveformName);
	s = hpe1445aWrite(la, pc->buf); 
	if(s){
		return s;
	}

	s = hpe1445aWrite(la, "source:list:segment:delete:selected"); 
	if(s){
		return s;
	}

	return VXI_SUCCESS;
}



/*
 *	hpe1445aLoadWaveform() 
 */
hpe1445aStat
hpe1445aLoadWaveform(
unsigned 	la,
char		*pWaveformName,
double		*pdata,
unsigned long	npoints
)
{
	hpe1445aStat		s;
        struct hpe1445aConfig	*pc;

	s = HPE1445A_PCONFIG(la, pc);
	if(s){
		errMessage(s, NULL);
		return s;
	}
	
	if(strlen(pWaveformName)>MAX_NAME_LENGTH){
		s = S_dev_highValue;
		errMessage(s, "waveform element name to long");
		return s;
	}

	if(npoints<HPE1445A_MIN_POINTS){
		s = S_dev_lowValue;
		errMessage(s, "waveform element count to small");
		return s;
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
LOCAL hpe1445aStat hpe1445aLoadWaveformLocked(
unsigned 		la,
struct hpe1445aConfig	*pc,
char			*pWaveformName,
double			*pdata,
unsigned long		npoints
)
{
	unsigned long	read_count;
	hpe1445aStat		s;

	s = hpe1445aWrite(la, "source:list:segment:free?"); 
	if(s){
		return s;
	}
	s = epvxiRead(
		la, 
		pc->buf, 
		sizeof(pc->buf), 
		&read_count,
		0);
	if(s){
		errMessage(s, "\"source:list:segment:free?\" query failed");
		return s;
	}	

	{
		int	nfree;
		int	nused;

		s = sscanf(pc->buf, "%d,%d", &nfree, &nused);
		if(s!=2){
			s = S_dev_internal;
			errMessage(s, "bad \"source:list:segment:free?\" resp");
			return s;
		}
		if(nfree < npoints){
			s = S_dev_internal;
			errPrintf(
				s,
				__FILE__,
				__LINE__,
				"%d waveform elements available", 
				nfree);
			errPrintf(
				s,
				__FILE__,
				__LINE__,
				"%d element waveform rejected",
				npoints);
			return s;
		}
	}
	

	/*
	 *
 	 * select the segment for subsequent
	 * commands
	 *
	 */
	sprintf(
		pc->buf, 
		"source:list:segment:select %s%s", 
		SEGMENT_NAME_PREFIX, 
		pWaveformName);
	s = hpe1445aWrite(la, pc->buf); 
	if(s){
		return s;
	}

	sprintf(pc->buf, "source:list:segment:define %lu", npoints);
	s = hpe1445aWrite(la, pc->buf); 
	if(s){
		return s;
	}
	sprintf(	pc->buf, 
			"source:arbitrary:download VXI,%s%s,%lu", 
			SEGMENT_NAME_PREFIX,
			pWaveformName,
			npoints);
	s = hpe1445aWrite(la, pc->buf); 
	if(s){
		return s;
	}

	/*
	 * wait for the device to finish with the download
	 * command prior to the backplane download
	 */
	s = hpe1445aWrite(la, "*OPC?"); 
	if(s){
		return s;
	}
	s = epvxiRead(
		la, 
		pc->buf, 
		sizeof(pc->buf), 
		&read_count,
		0);
	if(s){
		errMessage(s,"\"*OPC?\" query failed");
		return s;
	}	

	{
		double		truncationOffset;
		double		dacPeakAmplitude;
		double		dacOffset;
		double		*pwf;
		uint16_t	*pdata_port;

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
	if(s){
		return s;
	}

	/*
	 *
	 * install this segment into the sequence
	 *
	 */ 
	sprintf(	pc->buf, 
			"source:list:ssequence:select %s%s", 
			SEQUENCE_NAME_PREFIX,
			pWaveformName);
	s = hpe1445aWrite(la, pc->buf);
	if(s){
		return s;
	}

	/*
	 * set the number of segments in this sequence
	 */ 
	s = hpe1445aWrite(la, "source:list:ssequence:define 1");
	if(s){
		return s;
	}
	sprintf(pc->buf, 
		"source:list:ssequence:sequence %s%s", 
		SEGMENT_NAME_PREFIX,
		pWaveformName);
	s = hpe1445aWrite(la, pc->buf);
	if(s){
		return s;
	}
	s = hpe1445aWrite(la, "source:list:ssequence:dwell:count 1");
	if(s){
		return s;
	}

	return VXI_SUCCESS;
}


/*
 *
 *	hpe1445aWriteWithLineno() 
 *
 */
hpe1445aStat hpe1445aWriteWithLineno(
	unsigned	la,
	char		*pmsg,
	unsigned	lineno 
)
{
	unsigned long	nactual;
	hpe1445aStat	s;

	s = epvxiWrite(
			la,
			pmsg,
			strlen(pmsg),
			&nactual,
			0);
	if(s){
		errPrintf(
			s,
			__FILE__,
			lineno,
			"LA=0X%02X MSG=%s",
			la,
			pmsg);
		return s;
	}

	return VXI_SUCCESS;
}


/*
 *
 *	hpe1445aIoReport()
 *
 *
 */
void	hpe1445aIoReport(unsigned la, int level)	
{
	hpe1445aStat		s;
	struct hpe1445aConfig	*pc;
	char			*pStateName[] = {"in",""};

	s = HPE1445A_PCONFIG(la, pc);
	if(s){
		errMessage(s, NULL);
		return;
	}

	if(level>0){
		printf("\tdevice %sactive, DAC peak = %f V, DAC offset = %f V\n",
			pStateName[(unsigned)pc->device_active],
			pc->dacPeakAmplitude,
			pc->dacOffset);
	}
}
