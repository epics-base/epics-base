program beamTrajectory 

option -c;	/* don't wait for all channels to connect */
option +r;	/* reentrant */

short   seqRunning;
assign  seqRunning to "{user}:trajSeqOnC";

double   CM1_I;
assign  CM1_I to "{user}:CM1:intensityM";
monitor CM1_I;

double   PM1_X;
assign  PM1_X to "{user}:PM1:X:positionM";
monitor PM1_X;

double   PM1_Y;
assign  PM1_Y to "{user}:PM1:Y:positionM";
monitor PM1_Y;

double   PM1_I;
assign  PM1_I to "{user}:PM1:intensityM";
monitor PM1_I;

double   PM2_X;
assign  PM2_X to "{user}:PM2:X:positionM";
monitor PM2_X;

double   PM2_Y;
assign  PM2_Y to "{user}:PM2:Y:positionM";
monitor PM2_Y;

double   PM2_I;
assign  PM2_I to "{user}:PM2:intensityM";
monitor PM2_I;

double   PM3_X;
assign  PM3_X to "{user}:PM3:X:positionM";
monitor PM3_X;

double   PM3_Y;
assign  PM3_Y to "{user}:PM3:Y:positionM";
monitor PM3_Y;

double   PM3_I;
assign  PM3_I to "{user}:PM3:intensityM";
monitor PM3_I;

double   PM4_X;
assign  PM4_X to "{user}:PM4:X:positionM";
monitor PM4_X;

double   PM4_Y;
assign  PM4_Y to "{user}:PM4:Y:positionM";
monitor PM4_Y;

double   PM4_I;
assign  PM4_I to "{user}:PM4:intensityM";
monitor PM4_I;

double   PM5_X;
assign  PM5_X to "{user}:PM5:X:positionM";
monitor PM5_X;

double   PM5_Y;
assign  PM5_Y to "{user}:PM5:Y:positionM";
monitor PM5_Y;

double   PM5_I;
assign  PM5_I to "{user}:PM5:intensityM";
monitor PM5_I;

double   FC1_I;
assign  FC1_I to "{user}:FC1:intensityM";
monitor FC1_I;

double   FC2_I;
assign  FC2_I to "{user}:FC2:intensityM";
monitor FC2_I;

double   xPosWf[8];
assign  xPosWf to "{user}:PM:xTrajectoryWF";

double   yPosWf[8];
assign  yPosWf to "{user}:PM:yTrajectoryWF";

double   iWf[8];
assign  iWf to "{user}:PM:iTrajectoryWF";

double   bpmLocations[8];
assign  bpmLocations to "{user}:PM:distancesWF";

double   refLocations[2];
assign  refLocations to "{user}:PM:refLocationsWF";

double   refHi[2];
assign  refHi to "{user}:PM:referenceHiWF";

double   refLo[2];
assign  refLo to "{user}:PM:referenceLoWF";

double   bpmFixedLocations[13];

short   i;	/* index into LI waveforms */
short   cBpm;		/* bpm under consideration */

ss bpmTrajectorySS
{
state init
  {
        when()
            {
            /* Indicate sequence is running */
            seqRunning = 1;
            pvPut(seqRunning);

            /* BPM Position from EGUN */
            bpmFixedLocations[0] =  0.0; 
            bpmFixedLocations[1] =  2.4; 
            bpmFixedLocations[2] =  9.0; 
            bpmFixedLocations[3] =  20.0; 
            bpmFixedLocations[4] =  33.0;
            bpmFixedLocations[5] =  44.0; 
            bpmFixedLocations[6] =  54,5; 
            bpmFixedLocations[7] =  58.0; 

            refLocations[0] = 0; 
            refLocations[1] = 60;
            pvPut(refLocations);

            refHi[0] = 1.0;
            refHi[1] = 1.0;
            refLo[0] = -1.0;
            refLo[1] = -1.0;
            
            pvPut(refHi);
            pvPut(refLo);

            } state waitToPlot 
  } 


state waitToPlot 
  {
	when(delay(1.0))
	    {
	    } state plot
  }

state plot 
  {
        when()
            {
               i=7;
               cBpm=7;
            /* include reading if > threshold */ 
            if (FC1_I > 1.0) {
               xPosWf[i] = PM5_X;
               yPosWf[i] = PM5_Y;
               iWf[i]    = FC1_I;
               bpmLocations[i] = bpmFixedLocations[cBpm];
               i--;
            }
            else if(FC2_I > 1.0) {
               xPosWf[i] = PM5_X;
               yPosWf[i] = PM5_Y;
               iWf[i]    = FC2_I;
               bpmLocations[i] = bpmFixedLocations[cBpm];
               i--;
            }
            else if(PM5_X != 0.0) {
               xPosWf[i] = PM5_X;
               yPosWf[i] = PM5_Y;
               iWf[i]    = 0;
               bpmLocations[i] = bpmFixedLocations[cBpm];
               i--;
            }

            cBpm--;

            if (PM5_X != 0.0) {
               xPosWf[i] = PM5_X;
               yPosWf[i] = PM5_Y;
               iWf[i]    = PM5_I;
               bpmLocations[i] = bpmFixedLocations[cBpm];
               i--;
            }
            cBpm--;

            if (PM4_X != 0.0) {
               xPosWf[i] = PM4_X;
               yPosWf[i] = PM4_Y;
               iWf[i]    = PM4_I;
               bpmLocations[i] = bpmFixedLocations[cBpm];
               i--;
            }
            cBpm--;

            if (PM3_X != 0.0) {
               xPosWf[i] = PM3_X;
               yPosWf[i] = PM3_Y;
               iWf[i]    = PM3_I;
               bpmLocations[i] = bpmFixedLocations[cBpm];
               i--;
            }
            cBpm--;

            if (PM2_X != 0.0) {
               xPosWf[i] = PM2_X;
               yPosWf[i] = PM2_Y;
               iWf[i]    = PM2_I;
               bpmLocations[i] = bpmFixedLocations[cBpm];
               i--;
            }
            cBpm--;

            if (PM1_X != 0.0) {
               xPosWf[i] = PM1_X;
               yPosWf[i] = PM1_Y;
               iWf[i]    = PM1_I;
               bpmLocations[i] = bpmFixedLocations[cBpm];
               i--;
            }

            cBpm--;
            if (CM1_I > 1.0) {
               xPosWf[i] = 0; 
               yPosWf[i] = 0; 
               iWf[i]    = CM1_I; 
               bpmLocations[i] = bpmFixedLocations[cBpm];
               i--;
               xPosWf[i] = 0; 
               yPosWf[i] = 0; 
               iWf[i]    = CM1_I; 
               bpmLocations[i] = bpmFixedLocations[cBpm-1];
               i--;
            }
            
            /* fill in unplotted points on top of last point */
            while (i>=0)
               {
               xPosWf[i] = 0;
               yPosWf[i] = 0;
               iWf[i]    = CM1_I;
               bpmLocations[i] = 0;
               i--;
               }

            pvPut(xPosWf);
            pvPut(yPosWf);
            pvPut(iWf);
            pvPut(bpmLocations);
  
            } state waitToPlot
  } 

}

