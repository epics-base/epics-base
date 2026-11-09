### Synchronous link groups in sequence records

GitHub [issue #784](https://github.com/epics-base/epics-base/issues/784)

The sequence record can now process link groups synchronously.
Setting a `DLYn` field to a negative value processes that link group immediately,
without releasing the record's lockset after the preceding group.
Consecutive link groups with negative delays therefore cannot be interleaved
with processing by another record in the same lockset.

Zero and positive `DLYn` values retain their previous callback behavior.
A value of zero schedules an immediate callback,
while a positive value schedules a callback after the specified delay.

Negative delays should be used carefully,
since processing several link groups while retaining the lockset can delay
other records in that lockset.


