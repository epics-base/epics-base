#include <vxWorks.h>
#include <lstLib.h>
#include <taskLib.h>
#include <types.h>
#include <socket.h>
#include <in.h>
#include <db_access.h>
#include <task_params.h>
#include <server.h>

#define DELETE_TASK(TID)\
if(errnoOfTaskGet(TID)!=ERROR)td(TID);

rsrv_init()
{
  FAST struct client		*client;
  void				req_server();
  void				cast_server();

  FASTLOCKINIT(&rsrv_free_addrq_lck);
  FASTLOCKINIT(&rsrv_free_eventq_lck);
  FASTLOCKINIT(&clientQlock);

  /* 
	the following is based on the assumtion that external variables
	are not reloaded when debugging.
	NOTE: NULL below specifies all clients
  */
  free_client(NULL);


  DELETE_TASK(taskNameToId(CAST_SRVR_NAME));
  DELETE_TASK(taskNameToId(REQ_SRVR_NAME));
  taskSpawn(	REQ_SRVR_NAME,
		REQ_SRVR_PRI,
		REQ_SRVR_OPT,
		REQ_SRVR_STACK,
		req_server);

  taskSpawn(	CAST_SRVR_NAME,
		CAST_SRVR_PRI,
		CAST_SRVR_OPT,
		CAST_SRVR_STACK,
		cast_server);
}
