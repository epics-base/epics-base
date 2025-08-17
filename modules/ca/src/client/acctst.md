# acctst

    acctst <PV name> [progress logging level] [channel duplication count]
                     [test repetition count] [enable preemptive callback]

## Description

Channel Access Client Library regression test.

The PV used with the test must be native type `DBR_DOUBLE` or `DBR_FLOAT`,
and modified only by acctst while the test is running. Therefore,
periodically scanned hardware attached analog input records do not work
well. Test failure is indicated if the program stops prior to printing
"test complete". If unspecified the progress logging level is zero,
and no messages are printed while the test is progressing. If
unspecified, the channel duplication count is 20000. If unspecified, the
test repetition count is once only. If unspecified, preemptive callback
is disabled.
