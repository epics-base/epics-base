# Release Notes

This document describes the changes that were included in the release of EPICS
noted below. Release entries are now provided in a separate document for each
version in the EPICS 7 series, but all are combined into a single page for
publishing on the EPICS website. Separate release documents are also included
from the older Base 3.15 and 3.16 series.

## EPICS Release 7.0.3

### `epicsTimeGetCurrent()` optimization

Add a fast path to epicsTimeGetCurrent() and related calls in the common case
where only the default OS current time provider is registered. This path does
not take the global mutex guarding the time providers list, potentially
reducing lock contention.

### dbEvent tweak Queue size

The size of the queue used by dbEvent to push monitor updates has been
slightly increased based on `DBR_TIME_DOUBLE` to better fill an ethernet frame.
This may result in slightly fewer, but larger frames being sent.

### mbbo/mbbiDirect number of bits as precision

Report NOBT as "precision" through the dbAccess API. This is not accessible
through CA, but is planned to be used through QSRV.
