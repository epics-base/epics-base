



/*
 * caExtraEventLabor ()
 */
LOCAL void caExtraEventLabor (void *pArg)
{
    cac                         *pcac = (cac *) pArg;
	dbcaPutNotify               *ppnb;
	struct event_handler_args   args;
	
	while (TRUE) {
        semTakeStatus semStatus;

		/*
		 * independent lock used here in order to
		 * avoid any possibility of blocking
		 * the database (or indirectly blocking
		 * one client on another client).
		 */
		semStatus = semMutexTakeTimeout (pcac->localIIU.putNotifyLock, 60.0 /* sec */);
        if (semStatus!=semTakeOK) {
            ca_printf ("cac: put notify deadlock condition detected\n");
            (*pcac->localIIU.pdbAdapter->p_db_post_extra_labor) (pcac->localIIU.evctx);
            break;
        }
		ppnb = (dbcaPutNotify *) ellGet (&pcac->localIIU.putNotifyQue);
		semMutexGive (pcac->localIIU.putNotifyLock);
		
		/*
		 * break to loop exit
		 */
		if (!ppnb) {
			break;
		}
		
		/*
		 * setup arguments and call user's function
		 */
		args.usr = ppnb->caUserArg;
		args.chid = ppnb->dbPutNotify.usrPvt;
#error type must be an external type
		args.type = ppnb->dbPutNotify.dbrType;
		args.count = ppnb->dbPutNotify.nRequest;
		args.dbr = NULL;
		if (ppnb->dbPutNotify.status) {
			if (ppnb->dbPutNotify.status == S_db_Blocked) {
				args.status = ECA_PUTCBINPROG;
			}
			else {
				args.status = ECA_PUTFAIL;
			}
		}
		else {
			args.status = ECA_NORMAL;
		}
		
		pcac->lock ();
		(*ppnb->caUserCallback) (args);
		pcac->unlock ();
		
		ppnb->busy = FALSE;
	}
	
	/*
	 * wakeup the TCP thread if it is waiting for a cb to complete
	 */
	semBinaryGive (pcac->ca_blockSem);
}




