#ifndef GDD_H
#define GDD_H

/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 * Revision 1.21  1997/03/17 17:14:48  jbk
 * fixed a problem with gddDestructor and reference counting
 *
 * Revision 1.20  1997/01/12 20:32:46  jbk
 * many errors fixed
 *
 * Revision 1.18  1996/11/04 17:12:50  jbk
 * fix setFirst
 *
 * Revision 1.17  1996/11/02 01:24:49  jhill
 * strcpy => styrcpy (shuts up purify)
 *
 * Revision 1.16  1996/10/17 12:39:14  jbk
 * removed strdup definition, fixed up the local/network byte order functions
 *
 * Revision 1.15  1996/09/10 15:06:29  jbk
 * Adjusted dbMapper.cc so gdd to string function work correctly
 * Added checks in gdd.h so that get(pointer) functions work with scalars
 *
 * Revision 1.14  1996/09/07 13:03:05  jbk
 * fixes to destroyData function
 *
 * Revision 1.13  1996/09/04 22:47:09  jhill
 * allow vxWorks 5.1 and gnu win 32
 *
 * Revision 1.12  1996/08/27 13:05:06  jbk
 * final repairs to string functions, put() functions, and error code printing
 *
 * Revision 1.11  1996/08/22 21:05:41  jbk
 * More fixes to make strings and fixed string work better.
 *
 * Revision 1.10  1996/08/14 16:29:38  jbk
 * fixed a put() function that did not return anything
 *
 * Revision 1.9  1996/08/14 12:30:15  jbk
 * fixes for converting aitString to aitInt8* and back
 * fixes for managing the units field for the dbr types
 *
 * Revision 1.8  1996/08/13 15:07:48  jbk
 * changes for better string manipulation and fixes for the units field
 *
 * Revision 1.7  1996/08/09 02:29:16  jbk
 * fix getRef(aitString*&) to return the correct value if gdd is scalar
 *
 * Revision 1.6  1996/08/06 19:14:12  jbk
 * Fixes to the string class.
 * Changes units field to a aitString instead of aitInt8.
 *
 * Revision 1.5  1996/07/26 02:23:17  jbk
 * Fixed the spelling error with Scalar.
 *
 * Revision 1.4  1996/07/24 22:17:17  jhill
 * removed gdd:: from func proto
 *
 * Revision 1.3  1996/07/23 17:13:33  jbk
 * various fixes - dbmapper incorrectly worked with enum types
 *
 * Revision 1.2  1996/06/26 00:19:40  jhill
 * added CHAR_BIT to max bound calc for the "ref_cnt" member of class gdd
 *
 * Revision 1.1  1996/06/25 19:11:38  jbk
 * new in EPICS base
 *
 *
 * *Revision 1.4  1996/06/24 03:15:33  jbk
 * *name changes and fixes for aitString and fixed string functions
 * *Revision 1.3  1996/06/17 15:24:09  jbk
 * *many mods, string class corrections.
 * *gdd operator= protection.
 * *dbmapper uses aitString array for menus now
 * *Revision 1.2  1996/06/13 21:31:56  jbk
 * *Various fixes and correction - including ref_cnt change to unsigned short
 * *Revision 1.1  1996/05/31 13:15:25  jbk
 * *add new stuff
 *
 */

// Still need to handle to network byte order stuff.
// Should always be marked net byte order on CPUs with native network byte
// order.  No extra code should be run in this case.  This should be
// easy with #defines since the AIT library defines a macro if network
// byte order is not native byte order

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>

#if defined(WIN32)
#       include <stdlib.h>
#elif defined(vxWorks)
#       include <time.h>
#       include <timers.h> // for timespec under v51 vxWorks
#elif defined(UNIX)
        // hopefully a posix compliant OS
#       include <stdlib.h>
#       include <unistd.h>
#       include <sys/time.h>
#endif

// gddNewDel.h - a simple bunch of macros to make a class use free lists
//            with new/remove operators

#include "gddNewDel.h"
#include "gddUtils.h"
#include "gddErrorCodes.h"
#include "aitTypes.h"
#include "aitConvert.h"

class gddContainer;
class gddAtomic;
class gddScalar;

// Not Complete in this prototype:
// - Read only DD.
// - Small array management using free lists

// - need bit in DD to indicate that no referencing is allowed for the DD
// - need bit to indicate that DD should not be modified, for a container
//   this means that the container cannot be added to or removed from.

// Notes:
//  gdds do not need to be linked lists.  I could have used a container
//  to describe arrays of DDs, and manage the commonly used ones (DD array
//  sizes on free lists when the container was destroyed.  The constructor
//  to the container could take the number of DDs the container will
//  manage.  This would be similar to the gddBounds method.
//
//	Has weak support for changing the dimension of a DD.  In other words
//	it is not easy to get a "value" DD (defaulting to scaler) and change
//	the dimension to a two dimensional array.  Need to add routines to
//	restructure the bounds.
//
//	Need to add methods for inserting const pointers into the DD.
//
//  Overriding the operator[] can make the linked list contained in the
//  container appear as an array.

// ---------------------------------------------------------------------
// class structure for DDs:
//
//      gddScalar
//          \
//        gddAtomic   gddContainer
//                \   /
//                 gdd
//
// All the subclasses of gdd are around to simplify creation and use of
// DDs.

// ---------------------------------------------------------------------
// This is the main Data Descriptor (DD).

class gdd
{
public:
	gdd(void);
	gdd(gdd*);
	gdd(int app);
	gdd(int app,aitEnum prim);
	gdd(int app,aitEnum prim,int dimen);
	gdd(int app,aitEnum prim,int dimen,aitUint32* size_array);

	enum
	{
		GDD_MANAGED_MASK=0x01,
		GDD_FLAT_MASK=0x02,
		GDD_NET_MASK=0x04,
		GDD_NOREF_MASK=0x08,
		GDD_CONSTANT_MASK=0x10
	};

	unsigned applicationType(void) const;
	aitEnum primitiveType(void) const;
	unsigned dimension(void) const;
	aitType& getData(void);
	aitType* dataUnion(void);
	gddDestructor* destructor(void) const;
	gdd* next(void);
	void setNext(gdd*);

	const gddBounds* getBounds(void) const;
	const gddBounds* getBounds(int bn) const;

	void dump(void);
	void test(void);

	void setPrimType(aitEnum t);
	void setApplType(int t);
	void setDimension(int d,const gddBounds* = NULL);
	void destroyData(void);
	gddStatus reset(aitEnum primtype,int dimension, aitIndex* dim_counts);
	gddStatus clear(void); // clear all fields of the DD, including arrays
	gddStatus changeType(int appltype, aitEnum primtype);
	gddStatus setBound(unsigned dim_to_set, aitIndex first, aitIndex count);
	gddStatus getBound(unsigned dim_to_get, aitIndex& first, aitIndex& count);
	gddStatus registerDestructor(gddDestructor*);
	gddStatus replaceDestructor(gddDestructor*);
	void* dataVoid(void) const;
	void* dataAddress(void) const;
	void* dataPointer(void) const;
	void* dataPointer(aitIndex element_offset) const;

	void getTimeStamp(struct timespec* const ts) const;
	void getTimeStamp(aitTimeStamp* const ts) const;
	void setTimeStamp(const struct timespec* const ts);
	void setTimeStamp(const aitTimeStamp* const ts);
	void setStatus(aitUint32);
	void setStatus(aitUint16 high, aitUint16 low);
	void getStatus(aitUint32&);
	void getStatus(aitUint16& high, aitUint16& low);

	void setStat(aitUint16);
	void setSevr(aitUint16);
	aitUint16 getStat(void) const;
	aitUint16 getSevr(void) const;
	void setStatSevr(aitInt16 stat, aitInt16 sevr);
	void getStatSevr(aitInt16& stat, aitInt16& sevr);

	size_t getTotalSizeBytes(void) const;
	size_t getDataSizeBytes(void) const;
	aitUint32 getDataSizeElements(void) const;

	// Note for all copy operations:
	//  Cannot change a container into atomic or scaler type, cannot change
	//  scaler or atomic type to container

	// copyInfo() will copy  DD info only, this means appl, primitive type
	//   and bounds.  Scalar data will be copied, but no arrays.
	// copy() will copy DD info, bounds, allocate array data buffer and
	//   copy data into it.
	// Dup() will copy DD info. bounds, data references copied only.

	gddStatus copyInfo(gdd*);
	gddStatus copy(gdd*);
	gddStatus Dup(gdd*);

	// copy the entire DD structure into a contiguous buffer, return the
	// last byte of the buffer that was used.
	size_t flattenWithAddress(void* buffer,size_t size,aitIndex* total_dd=0);
	size_t flattenWithOffsets(void* buffer,size_t size,aitIndex* total_dd=0);
	gddStatus convertOffsetsToAddress(void);
	gddStatus convertAddressToOffsets(void);

	int isScalar(void) const;
	int isContainer(void) const;
	int isAtomic(void) const;

	int isManaged(void) const;
	int isFlat(void) const;
	int isLocalDataFormat(void) const;
	int isNetworkDataFormat(void) const;
	int isConstant(void) const;
	int isNoRef(void) const;

	void markConstant(void);
	void markManaged(void);
	void markUnmanaged(void);
	void markLocalDataFormat(void);
	void markNotLocalDataFormat(void);

	// The only way for a user to get rid of a DD is to Unreference it.
	// NoReferencing() means that the DD cannot be referenced.
	gddStatus noReferencing(void);
	gddStatus reference(void);
	gddStatus unreference(void);

	gdd& operator=(const gdd& v);

	// get a pointer to the data in the DD
	void getRef(aitFloat64*& d);
	void getRef(aitFloat32*& d);
	void getRef(aitUint32*& d);
	void getRef(aitInt32*& d);
	void getRef(aitUint16*& d);
	void getRef(aitInt16*& d);
	void getRef(aitUint8*& d);
	void getRef(aitInt8*& d);
	void getRef(aitString*& d);
	void getRef(aitFixedString*& d);
	void getRef(void*& d);

	// make the DD points to user data with a destroy method,
	// put the referenced in and adjust the primitive type
	void putRef(void*,aitEnum,gddDestructor* =0);
	void putRef(aitFloat64*,gddDestructor* =0);
	void putRef(aitFloat32*,gddDestructor* =0);
	void putRef(aitUint8*,gddDestructor* =0);
	void putRef(aitInt8*,gddDestructor* =0);
	void putRef(aitUint16*,gddDestructor* =0);
	void putRef(aitInt16*,gddDestructor* =0);
	void putRef(aitUint32*,gddDestructor* =0);
	void putRef(aitInt32*,gddDestructor* =0);
	void putRef(aitString*,gddDestructor* =0);
	void putRef(aitFixedString*,gddDestructor* =0);
	// work with constants
	void putRef(const aitFloat64*,gddDestructor* =0);
	void putRef(const aitFloat32*,gddDestructor* =0);
	void putRef(const aitUint8*,gddDestructor* =0);
	void putRef(const aitInt8*,gddDestructor* =0);
	void putRef(const aitUint16*,gddDestructor* =0);
	void putRef(const aitInt16*,gddDestructor* =0);
	void putRef(const aitUint32*,gddDestructor* =0);
	void putRef(const aitInt32*,gddDestructor* =0);
	void putRef(const aitString*,gddDestructor* =0);
	void putRef(const aitFixedString*,gddDestructor* =0);
	gddStatus putRef(const gdd*);

	// get the data in the form the user wants (do conversion)
	void getConvert(aitFloat64& d);
	void getConvert(aitFloat32& d);
	void getConvert(aitUint32& d);
	void getConvert(aitInt32& d);
	void getConvert(aitUint16& d);
	void getConvert(aitInt16& d);
	void getConvert(aitUint8& d);
	void getConvert(aitInt8& d);
	void getConvert(aitString& d);
	void getConvert(aitFixedString& d);

	// convert the user data to the type in the DD and set value
	void putConvert(aitFloat64 d);
	void putConvert(aitFloat32 d);
	void putConvert(aitUint32 d);
	void putConvert(aitInt32 d);
	void putConvert(aitUint16 d);
	void putConvert(aitInt16 d);
	void putConvert(aitUint8 d);
	void putConvert(aitInt8 d);
	void putConvert(const aitString& d);
	void putConvert(const aitFixedString& d);

	// copy the user data into the already set up DD array
	gddStatus put(const aitFloat64* const d);
	gddStatus put(const aitFloat32* const d);
	gddStatus put(const aitUint32* const d);
	gddStatus put(const aitInt32* const d);
	gddStatus put(const aitUint16* const d);
	gddStatus put(const aitInt16* const d);
	gddStatus put(const aitUint8* const d);
	gddStatus put(const aitInt8* const d);
	gddStatus put(const aitString* const d);
	gddStatus put(const aitFixedString* const d);
	gddStatus put(const gdd* dd);

	// put the user data into the DD and reset the primitive type
	gddStatus put(aitFloat64 d);
	gddStatus put(aitFloat32 d);
	gddStatus put(aitUint32 d);
	gddStatus put(aitInt32 d);
	gddStatus put(aitUint16 d);
	gddStatus put(aitInt16 d);
	gddStatus put(aitUint8 d);
	gddStatus put(aitInt8 d);
	gddStatus put(aitString& d);
	gddStatus put(const aitString& d);
	gddStatus put(const aitFixedString& d);
	gddStatus put(aitType* d);

	// copy the array data out of the DD
	void get(void* d);
	void get(void* d,aitEnum);
	void get(aitFloat64* d);
	void get(aitFloat32* d);
	void get(aitUint32* d);
	void get(aitInt32* d);
	void get(aitUint16* d);
	void get(aitInt16* d);
	void get(aitUint8* d);
	void get(aitInt8* d);
	void get(aitString* d);
	void get(aitFixedString* d);

	// copy the data out of the DD
	void get(aitFloat64& d);
	void get(aitFloat32& d);
	void get(aitUint32& d);
	void get(aitInt32& d);
	void get(aitUint16& d);
	void get(aitInt16& d);
	void get(aitUint8& d);
	void get(aitInt8& d);
	void get(aitString& d);
	void get(aitFixedString& d);
	void get(aitType& d);

	// Same as putRef() methods
	gdd& operator=(aitFloat64* v);
	gdd& operator=(aitFloat32* v);
	gdd& operator=(aitUint32* v);
	gdd& operator=(aitInt32* v);
	gdd& operator=(aitUint16* v);
	gdd& operator=(aitInt16* v);
	gdd& operator=(aitUint8* v);
	gdd& operator=(aitInt8* v);
	gdd& operator=(aitString* v);
	gdd& operator=(aitFixedString* v);

	// Same as put() methods
	gdd& operator=(aitFloat64 d);
	gdd& operator=(aitFloat32 d);
	gdd& operator=(aitUint32 d);
	gdd& operator=(aitInt32 d);
	gdd& operator=(aitUint16 d);
	gdd& operator=(aitInt16 d);
	gdd& operator=(aitUint8 d);
	gdd& operator=(aitInt8 d);
	gdd& operator=(aitString& d);
	gdd& operator=(const aitString& d);
	// gdd& operator=(aitFixedString d); // not present

	// Same as getRef() methods
	operator aitFloat64*(void) const;
	operator aitFloat32*(void) const;
	operator aitUint32*(void) const;
	operator aitInt32*(void) const;
	operator aitUint16*(void) const;
	operator aitInt16*(void) const;
	operator aitUint8*(void) const;
	operator aitInt8*(void) const;
	operator aitString*(void) const;
	operator aitFixedString*(void) const;

	// Same as get() methods
	operator aitFloat64(void);
	operator aitFloat32(void);
	operator aitUint32(void);
	operator aitInt32(void);
	operator aitUint16(void);
	operator aitInt16(void);
	operator aitUint8(void);
	operator aitInt8(void);
	operator aitString(void);
	// gdd::operator aitFixedString(void); // not present

	gddStatus genCopy(aitEnum t, const void* d,
		aitDataFormat f=aitLocalDataFormat);
	void adjust(gddDestructor* d,void* v,aitEnum type,
		aitDataFormat f=aitLocalDataFormat);
	void get(aitEnum t,void* v,aitDataFormat f=aitLocalDataFormat);
	void set(aitEnum t,const void* v,aitDataFormat f=aitLocalDataFormat);

	// following methods are used to import and export data into and out
	// of this gdd.  For the out methods, the returned count in the number
	// of bytes put into the user's buffer.  For the in methods, the
	// returned count in the number of bytes put into the gdd from the
	// user's buffer.  The in methods always change the data format to local
	// from whatever input format is specified with the argument.  The out
	// methods convert the gdd to whatever format is specified with the 
	// aitDataFormat argument.  If aitEnum argument left as invalid, then
	// data in the user's buffer will be assumed to be the same as the
	// type defined in the gdd.

	size_t out(void* buf,aitUint32 bufsize,aitDataFormat =aitNetworkDataFormat);
	size_t outHeader(void* buf,aitUint32 bufsize);
	size_t outData(void* buf,aitUint32 bufsize,
		aitEnum = aitEnumInvalid, aitDataFormat = aitNetworkDataFormat);

	size_t in(void* buf, aitDataFormat =aitNetworkDataFormat);
	size_t inHeader(void* buf);
	size_t inData(void* buf,aitUint32 number_of_elements = 0,
		aitEnum = aitEnumInvalid, aitDataFormat = aitNetworkDataFormat);

	gdd_NEWDEL_FUNC(bounds) // managed on free lists

protected:
	~gdd(void);  // users must call Unreference()

	void init(int app, aitEnum prim, int dimen);
	void freeBounds(void);
	void dumpInfo(void);
	gddStatus copyStuff(gdd*,int type);
	gddStatus clearData(void);

	size_t describedDataSizeBytes(void) const;
	aitUint32 describedDataSizeElements(void) const;

	void markFlat(void);
	gddStatus flattenData(gdd* dd, int tot_dds, void* buf, size_t size);
	int flattenDDs(gddContainer* dd, void* buf, size_t size);
	aitUint32 align8(unsigned long count) const;

	void setData(void* d);

	aitType data;				// array pointer or scaler data
	gddBounds* bounds;			// array of bounds information length dim
	gdd* nextgdd;
	gddDestructor* destruct;	// NULL=none supplied, use remove
	aitTimeStamp time_stamp;
	aitStatus status;
	aitUint16 appl_type;		// application type code
	aitUint8 prim_type;			// primitive type code
	aitUint8 dim;				// 0=scaler, >0=array

	gdd_NEWDEL_DATA		// required for using generic new and remove
private:
	aitUint16 ref_cnt;
	aitUint8 flags;
};

#include "gddI.h"

// include these to be backward compatible with first gdd library version
#include "gddArray.h"
#include "gddScalar.h"
#include "gddContainer.h"

#endif
