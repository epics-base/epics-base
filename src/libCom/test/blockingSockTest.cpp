
#include <assert.h>

#include "osiSock.h"
#include "osiWireFormat.h"
#include "epicsThread.h"
#include "epicsSignal.h"

static SOCKET s;
static bool blockingSockWakeup = false;

void socketJoltTest ( void * )
{
    epicsSignalInstallSigAlarmIgnore ();
    char buf [1];
    recv ( s, buf, (int) sizeof ( buf ), 0 );
    blockingSockWakeup = true;
}

void blockingSockTest ()
{
    s = epicsSocketCreate ( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    assert ( s != INVALID_SOCKET );

    epicsUInt16 port = 5071;
    union {
        struct sockaddr_in ia; 
        struct sockaddr sa;
    } bd;
    memset ( (char *) &bd, 0, sizeof ( bd ) );
    bd.ia.sin_family = AF_INET;
    bd.ia.sin_addr.s_addr = epicsHTON32 ( INADDR_ANY ); 
    bd.ia.sin_port = epicsHTON16 ( port );   
    int status = bind ( s, &bd.sa, sizeof ( bd ) );
    assert ( status == 0 );

    blockingSockWakeup = false;
    epicsThreadId id = epicsThreadCreate ( 
        "Socket Jolt Test", epicsThreadPriorityMedium, 
        epicsThreadGetStackSize(epicsThreadStackMedium), 
        socketJoltTest, 0 );

    epicsThreadSleep ( 1.0 );
    assert ( ! blockingSockWakeup );

    epicsSignalRaiseSigAlarm ( id );
    epicsThreadSleep ( 1.0 );
    char * pStr = "esscimqi_?????";
    if ( blockingSockWakeup ) {
        pStr = "esscimqi_socketSigAlarmRequired";
    }
    else {
        status = shutdown ( s, SHUT_RDWR );
        assert ( status == 0 );
        epicsThreadSleep ( 1.0 );
        if ( blockingSockWakeup ) {
            pStr = "esscimqi_socketBothShutdownRequired";
        }
        else {
            epicsSocketDestroy ( s );
            epicsThreadSleep ( 1.0 );
            if ( blockingSockWakeup ) {
                pStr = "esscimqi_socketCloseRequired";
            }
            else {
                pStr = "esscimqi_?????";
            }
        }
    }
    printf ( "Local os behaves like \"%s\"\n", pStr );
    pStr = "esscimqi_?????";
    switch ( epicsSocketSystemCallInterruptMechanismQuery() ) {
    case esscimqi_socketCloseRequired:
        pStr = "esscimqi_socketCloseRequired";
        break;
    case esscimqi_socketBothShutdownRequired:
        pStr = "esscimqi_socketBothShutdownRequired";
        break;
    case esscimqi_socketSigAlarmRequired:
        pStr = "esscimqi_socketSigAlarmRequired";
        break;
    }
    printf ( "epicsSocketSystemCallInterruptMechanismQuery() returns\n\"%s\"\n",
        pStr );
}
