#ifndef osiRingh
#define osiRingh

#include <vxWorks.h>
#include <rngLib.h>

#define ringId RING_ID

#define ringCreate rngCreate
#define ringDelete rngDelete
#define ringGet rngBufGet
#define ringPut rngBufPut
#define ringFlush rngFlush
#define ringFreeBytes rngFreeBytes
#define ringUsedBytes rngNBytes
#define ringSize(ID) (rngFreeBytes((ID)) + rngNBytes((ID)))
#define ringIsEmpty rngIsEmpty
#define ringIsFull rngIsFull

#endif /* osiRingh */
