

#ifdef __cplusplus
extern "C" {
#endif

enum appendNumberFlag {appendNumber, dontAppendNumber};
int catime ( char *channelName, unsigned channelCount, enum appendNumberFlag appNF );

int acctst ( char *pname, unsigned channelCount, unsigned repititionCount );

#define CATIME_OK 0
#define CATIME_ERROR -1

#ifdef __cplusplus
}
#endif

void caConnTest ( const char *pNameIn, unsigned channelCountIn, double delayIn );


