/*
 * caRepeater.c
 * share/src/ca/caRepeater.c
 *
 * CA broadcast repeater executable
 *
 * Author:      Jeff Hill
 * Date:        3-27-90
 *
 *      PURPOSE:
 *      Broadcasts fan out over the LAN, but UDP does not allow
 *      two processes on the same machine to get the same broadcast.
 *      This code takes extends the broadcast model from the net to within
 *      the OS.
 *
 *      NOTES:
 *
 *	see repeater.c
 *
 */

#include <iocinf.h>

main()
{
	ca_repeater ();
	assert (0);
	return (0);
}
