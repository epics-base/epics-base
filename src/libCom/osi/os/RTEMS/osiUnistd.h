/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#include <unistd.h>

/*
 * Some systems fail to provide prototypes of these functions.
 * Others provide different prototypes.
 * There seems to be no way to handle this automatically, so
 * if you get compile errors, just make the appropriate changes here.
 */
int putenv (char *);
char *strdup (const char *);
