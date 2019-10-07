
#include <string.h>
#include <stdlib.h>

#define epicsExportSharedSymbols
#include <osiFileName.h>

char *epicsGetExecName(void)
{
    const char *raw = getexecname();
    char *ret = NULL;
    /* manpage says getexecname() might return a relative path.  we treat this as an error */
    if(raw[0]=='/') {
        ret = strdup(raw);
    }
    return ret;
}

char *epicsGetExecDir(void)
{
    char *ret = epicsGetExecName();
    if(ret) {
        char *sep = strrchr(ret, '/');
        if(sep) {
            /* nil the charactor after the / */
            sep[1] = '\0';
        }
    }
    return ret;
}
