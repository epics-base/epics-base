
#include "shareLib.h"

/*
 * truncate to specified size (we dont use truncate()
 * because it is not portable)
 *
 * pFileName - name (and optionally path) of file
 * size - the new file size (if file is curretly larger)
 * 
 * returns TF_OK if the file is less than size bytes
 * or if it was successfully truncated. Returns
 * TF_ERROR if the file could not be truncated.
 */
enum TF_RETURN {TF_OK=0, TF_ERROR=1};
epicsShareFunc enum TF_RETURN epicsShareAPI truncateFile (const char *pFileName, unsigned size);

