
enum appendNumberFlag {appendNumber, dontAppendNumber};

int catime ( char *channelName, unsigned channelCount, enum appendNumberFlag appNF );

int acctst ( char *pname, unsigned channelCount, unsigned repititionCount );

void caConnTest ( const char *pNameIn, unsigned channelCountIn, double delayIn );

#define CATIME_OK 0
#define CATIME_ERROR -1

