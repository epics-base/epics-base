/*
	$Id$
	caeventmask.h

	Modification History
	joh	04-16-90	Created

*/

#ifndef INCLcaeventmaskh
#define INCLcaeventmaskh

/*
	event selections
	(If any more than 8 of these are needed then update the 
	select field in the event_block struct in db_event.c from 
	unsigned char to unsigned short)


	DBE_VALUE 
	Trigger an event when a significant change in the channel's value
	occurs. Relies on the monitor deadband field under DCT.

	DBE_LOG
	Trigger an event when an archive significant change in the channel's
	valuue occurs. Relies on the archiver monitor deadband field under DCT.

	DBE_ALARM
	Trigger an event when the alarm state changes

*/
#define	DBE_VALUE	(1<<0)	
#define	DBE_LOG		(1<<1)
#define	DBE_ALARM	(1<<2)	

#endif
