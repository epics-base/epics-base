
#include <osiTime.h>

#include <sys/types.h>
#include <sys/time.h>

extern "C" {
int gettimeofday (struct timeval *tp, struct timezone *tzp);
}

//
// osiTime::getCurrent ()
//
osiTime osiTime::getCurrent ()
{
        int             status;
        struct timeval  tv;
        struct timezone tzp;
 
        status = gettimeofday (&tv, &tzp);
        assert (status==0);
 
        return osiTime(tv.tv_sec, tv.tv_usec * nSecPerUSec);
}

