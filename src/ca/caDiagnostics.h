
enum appendNumberFlag {appendNumber, dontAppendNumber};

int catime (char *channelName, unsigned channelCount, enum appendNumberFlag appNF);

int acctst (char *pname);

#define CATIME_OK 0
#define CATIME_ERROR -1

