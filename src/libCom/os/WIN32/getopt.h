/* getopt.h */

#ifndef INC_getopt_H
#define INC_getopt_H

#include <shareLib.h>

epicsShareExtern int opterr; 
epicsShareExtern int optind;
epicsShareExtern int optopt;
epicsShareExtern int optopt;
epicsShareExtern int optreset;
epicsShareExtern char *optarg;

epicsShareFunc int epicsShareAPI getopt ( int argc, 
                        char * const *argv, const char *ostr );

#endif
