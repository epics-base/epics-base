program autoControl

option -c;	/* don't wait for all channels to connect */
option +r;	/* reentrant */

short   mode;
assign  mode to "{user}:autoC";
monitor mode;

short   htrMon;
assign  htrMon to "{user}:cathodeTempM";
monitor htrMon;

string  msg;
assign  msg to "{user}:OP:autoMsg";

float   htr;
assign  htr to "{user}:cathodeCurrentC";

short   beamOn;
assign  beamOn to "{user}:gunOnC";

short   gv1;
assign  gv1 to "{user}:GV1:positionC";

short   h1Scan;
assign  h1Scan to "{user}A:H1:setCurrentC.SCAN";

short   v1Scan;
assign  v1Scan to "{user}A:V1:setCurrentC.SCAN";

short   h2Scan;
assign  h2Scan to "{user}A:H2:setCurrentC.SCAN";

short   v2Scan;
assign  v2Scan to "{user}A:V2:setCurrentC.SCAN";

short   h3Scan;
assign  h3Scan to "{user}A:H3:setCurrentC.SCAN";

short   v3Scan;
assign  v3Scan to "{user}A:V3:setCurrentC.SCAN";

short   h4Scan;
assign  h4Scan to "{user}A:H4:setCurrentC.SCAN";

short   v4Scan;
assign  v4Scan to "{user}A:V4:setCurrentC.SCAN";

short   h5Scan;
assign  h5Scan to "{user}A:H5:setCurrentC.SCAN";

short   v5Scan;
assign  v5Scan to "{user}A:V5:setCurrentC.SCAN";

float   h1A;
assign  h1A to "{user}A:H1:setCurrentC";

float   v1A;
assign  v1A to "{user}A:V1:setCurrentC";

float   h2A;
assign  h2A to "{user}A:H2:setCurrentC";

float   v2A;
assign  v2A to "{user}A:V2:setCurrentC";

float   h3A;
assign  h3A to "{user}A:H3:setCurrentC";

float   v3A;
assign  v3A to "{user}A:V3:setCurrentC";

float   h4A;
assign  h4A to "{user}A:H4:setCurrentC";

float   v4A;
assign  v4A to "{user}A:V4:setCurrentC";

float   h5A;
assign  h5A to "{user}A:H5:setCurrentC";

float   v5A;
assign  v5A to "{user}A:V5:setCurrentC";

float   h1AR;
assign  h1AR to "{user}A:H1:setCurrentC.OROC";

float   v1AR;
assign  v1AR to "{user}A:V1:setCurrentC.OROC";

float   h2AR;
assign  h2AR to "{user}A:H2:setCurrentC.OROC";

float   v2AR;
assign  v2AR to "{user}A:V2:setCurrentC.OROC";

float   h3AR;
assign  h3AR to "{user}A:H3:setCurrentC.OROC";

float   v3AR;
assign  v3AR to "{user}A:V3:setCurrentC.OROC";

float   h4AR;
assign  h4AR to "{user}A:H4:setCurrentC.OROC";

float   v4AR;
assign  v4AR to "{user}A:V4:setCurrentC.OROC";

float   h5AR;
assign  h5AR to "{user}A:H5:setCurrentC.OROC";

float   v5AR;
assign  v5AR to "{user}A:V5:setCurrentC.OROC";


%%#include <epicsThread.h>

ss autoCtlSS
{
state init
  {
        when()
            {
            h1Scan = 0;
            v1Scan = 0;
            h2Scan = 0;
            v2Scan = 0;
            h3Scan = 0;
            v3Scan = 0;
            h4Scan = 0;
            v4Scan = 0;
            h5Scan = 0;
            v5Scan = 0;
            pvPut(h1Scan);
            pvPut(v1Scan);
            pvPut(h2Scan);
            pvPut(v2Scan);
            pvPut(h3Scan);
            pvPut(v3Scan);
            pvPut(h4Scan);
            pvPut(v4Scan);
            pvPut(h5Scan);
            pvPut(v5Scan);
%%          epicsThreadSleep(.5);
            mode = 0;
            pvPut(mode);
%%          epicsThreadSleep(.1);
            } state waitForAuto
  } 


state waitForAuto
  {
	when(mode)
	    {
            beamOn = 0;
            pvPut(beamOn);
            gv1 = 0;
            pvPut(gv1);

            h1AR = 0;
            v1AR = 0;
            h2AR = 0;
            v2AR = 0;
            h3AR = 0;
            v3AR = 0;
            h4AR = 0;
            v4AR = 0;
            h5AR = 0;
            v5AR = 0;
            pvPut(h1AR);
            pvPut(v1AR);
            pvPut(h2AR);
            pvPut(v2AR);
            pvPut(h3AR);
            pvPut(v3AR);
            pvPut(h4AR);
            pvPut(v4AR);
            pvPut(h5AR);
            pvPut(v5AR);
%%          epicsThreadSleep(.1);

            h1A = 0;
            v1A = 0;
            h2A = 0;
            v2A = 0;
            h3A = 0;
            v3A = 0;
            h4A = 0;
            v4A = 0;
            h5A = 0;
            v5A = 0;
            pvPut(h1A);
            pvPut(v1A);
            pvPut(h2A);
            pvPut(v2A);
            pvPut(h3A);
            pvPut(v3A);
            pvPut(h4A);
            pvPut(v4A);
            pvPut(h5A);
            pvPut(v5A);
%%          epicsThreadSleep(.2);

            h1AR = 0.1;
            v1AR = 0.1;
            h2AR = 0.1;
            v2AR = 0.1;
            h3AR = 0.1;
            v3AR = 0.1;
            h4AR = 0.1;
            v4AR = 0.1;
            h5AR = 0.1;
            v5AR = 0.1;
            pvPut(h1AR);
            pvPut(v1AR);
            pvPut(h2AR);
            pvPut(v2AR);
            pvPut(h3AR);
            pvPut(v3AR);
            pvPut(h4AR);
            pvPut(v4AR);
            pvPut(h5AR);
            pvPut(v5AR);
%%          epicsThreadSleep(.2);

            sprintf(msg,"Optimizing Cathode Temperature");
            pvPut(msg);
            htr = 12.3;
            pvPut(htr);
%%          epicsThreadSleep(3);
            } state waitForHtr
  }

state waitForHtr
  {
        when(htrMon > 153)
            {
            sprintf(msg,"Turning Beam On");
            pvPut(msg);
%%          epicsThreadSleep(2);
            beamOn = 1;
            pvPut(beamOn);

            h1Scan = 9;
            v1Scan = 9;
            h2Scan = 9;
            v2Scan = 9;
            h3Scan = 9;
            v3Scan = 9;
            h4Scan = 9;
            v4Scan = 9;
            h5Scan = 9;
            v5Scan = 9;
            pvPut(h1Scan);
            pvPut(v1Scan);
            pvPut(h2Scan);
            pvPut(v2Scan);
            pvPut(h3Scan);
            pvPut(v3Scan);
            pvPut(h4Scan);
            pvPut(v4Scan);
            pvPut(h5Scan);
            pvPut(v5Scan);
%%          epicsThreadSleep(3);

            h1A = -1.31;
            v1A = 1.79;
            h2A = 2.67;
            v2A = -1.39;
            h3A = -1.68;
            v3A = -.8;
            h4A = 1.63;
            v4A = .1;
            h5A = -1.83;
            v5A = 2.21;
            sprintf(msg,"Optimizing H1/V1 Steering");
            pvPut(msg);
            pvPut(h1A);
            pvPut(v1A);
%%          epicsThreadSleep(3);
            sprintf(msg,"Optimizing H2/V2 Steering");
            pvPut(msg);
            pvPut(h2A);
            pvPut(v2A);
%%          epicsThreadSleep(3);
            sprintf(msg,"Opening Gate Valve");
            pvPut(msg);
%%          epicsThreadSleep(2);
            gv1 = 1;
            pvPut(gv1);
%%          epicsThreadSleep(3);
            sprintf(msg,"Optimizing H3/V3 Steering");
            pvPut(msg);
            pvPut(h3A);
            pvPut(v3A);
%%          epicsThreadSleep(3);
            sprintf(msg,"Optimizing H4/V4 Steering");
            pvPut(msg);
            pvPut(h4A);
            pvPut(v4A);
%%          epicsThreadSleep(3);
            sprintf(msg,"Optimizing H5/V5 Steering");
            pvPut(msg);
            pvPut(h5A);
            pvPut(v5A);
%%          epicsThreadSleep(3);
            sprintf(msg,"Optimization Complete");
            pvPut(msg);
%%          epicsThreadSleep(4);
	    } state init
  }

}

