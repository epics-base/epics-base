/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2002 Berliner Elektronenspeicherringgesellschaft fuer
*     Synchrotronstrahlung.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* 
 *  $Id$
 *
 *  Author: Ralph Lange (BESSY)
 *
 */

#ifndef INCLparseOptionsh
#define INCLparseOptionsh

/* IMPORTANT NOTE:
 * ---------------
 * This library is a workaround for achieving getopt() functionality in
 * EPICS Base 3.14.5 - it will be replaced as soon as possible without
 * further notice.
 *
 * DO NOT WRITE ANY CODE BASED ON THIS LIBRARY - IT WILL BREAK.
 *
 */

/* This is a minimal version of a parser whose function is somewhat getopt()
 * compatible. Subsequent calls return the command line options one by one.
 * The special option "--" forces the end of option scanning.
 *
 * parseOptions   returns the option character found on the command line
 *                returns '?' if an option is invalid / misses an argument (see below)
 *                returns -1 if there are no more options (argv[optIndex] is
 *                the first non-option argument)
 *
 * options      -  valid options string (format is similar to getopt):
 *     <letter>    for a valid option
 *     <letter>:   for an option that takes an argument
 *     :           as first character: parseOptions will return ':' 
 *                 instead of '?' for a missing argument
 *
 * optArgument  -  Points to an options argument (NULL otherwise)
 * optChar      -  Contains the last tested character
 *                 (i.e. the failing one for an unrecognized option)
 * optIndex     -  contains the index to the next (first non-option) argument
 * 
 * Differences to getopt():
 *  o No error messages printed
 *  o No argv[] shuffling
 *  o No special characters '+' or '-' as first options character
 *
 */

extern int optIndex;
extern char *optArgument;
extern char optChar;

extern int parseOptions (int argc, char * const argv[], const char *options);

/*
 * no additions below this endif
 */
#endif /* ifndef INCLparseOptionsh */
