# catime

    catime <PV name> [channel count] [append number to pv name if true]

## Description

Channel Access Client Library performance test.

If unspecified, the channel count is 10000. If the "append number to pv
name if true" argument is specified and it is greater than zero then
the channel names in the test are numbered as follows.

    <PV name>000000, <PV name>000001, ... <PV name>nnnnnn


