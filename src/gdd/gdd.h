#ifndef GDD_H
#define GDD_H

/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
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
#ifndef vxWorks
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#else
#include <time.h>
#endif

// strdup is not defined under POSIX
#if defined(_POSIX_C_SOURCE) || defined(vxWorks)
#ifdef __GNUC__
#define strdup(X) strcpy(new char[strlen(X)+1],X)
#else
#ifndef __EXTENSIONS__
inline char* strdup(const char* x)
{
	char* y;
	y = new char[strlen(x)+1];
	strcpy(y,x);
	return y;
}
#endif
#endif
#endif

#ifndef vxWorks
#if (_POSIX_C_SOURCE < 3) && !defined(solaris) && !defined(SOLARIS)
struct timespec
{
	time_t tv_sec;
	long tv_nsec;
};
typedef struct timespec timespec;
#endif
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
// Describe the bounds of a single dimension in terms of the first
// element and the number of elements in the dimension.

class gddBounds
{
public:
	// I found a weird memory management quirk with SunOS when constructors
	// are present in a class that is used as an array in another class
	// as it is in gddBounds2D.  The memory manager reallocates the entire
	// gddBounds2D in some sort of buffer pool which stays allocated when the
	// program completes

	// gddBounds(void);
	// gddBounds(aitIndex c);
	// gddBounds(aitIndex f, aitIndex c);

	void setSize(aitIndex c);
	void set(aitIndex f, aitIndex c);
	void get(aitIndex& f, aitIndex& c);
	aitIndex size(void) const;
	aitIndex first(void) const;

private:
	aitIndex start;
	aitIndex count;
};

// inline gddBounds::gddBounds(aitIndex c) { start=0; count=c; }
// inline gddBounds::gddBounds(aitIndex f, aitIndex c) { start=f; count=c; }

inline void gddBounds::setSize(aitIndex c)			{ count=c; }
inline void gddBounds::set(aitIndex f, aitIndex c)	{ start=f; count=c; }
inline void gddBounds::get(aitIndex& f, aitIndex& c){ f=start; c=count; }
inline aitIndex gddBounds::size(void) const			{ return count; }
inline aitIndex gddBounds::first(void) const		{ return start; }

// ---------------------------------------------------------------------
// Special managment for 1D-2D-3D bounds - others use normal new/remove.
// Since bounds are maintained as arrays, where the length of the array is
// the dimension of the data, we will manage the normal cases using 
// free lists.

class gddBounds1D
{
public:
	gddBounds1D(void) { }
	gddBounds* boundArray(void);
	gdd_NEWDEL_FUNC(gddBounds1D) // required for using generic new and remove
private:
	gddBounds b[1];
	gdd_NEWDEL_DATA(gddBounds1D) // required for using generic new/remove
};
inline gddBounds* gddBounds1D::boundArray(void) { return (gddBounds*)b; }

class gddBounds2D
{
public:
	gddBounds2D(void) { }
	gddBounds* boundArray(void);
	gdd_NEWDEL_FUNC(gddBounds2D) // required for using generic new and remove
private:
	gddBounds b[2];
	gdd_NEWDEL_DATA(gddBounds2D) // required for using generic new/remove
};
inline gddBounds* gddBounds2D::boundArray(void) { return (gddBounds*)b; }

class gddBounds3D
{
public:
	gddBounds3D(void) { }
	gddBounds* boundArray(void);
	gdd_NEWDEL_FUNC(gddBounds3D) // for using generic new and remove
private:
	gddBounds b[3];
	gdd_NEWDEL_DATA(gddBounds3D) // required for using generic new/remove
};
inline gddBounds* gddBounds3D::boundArray(void) { return (gddBounds*)b; }

// ---------------------------------------------------------------------
// This is a class for managing the destruction of user data held within
// a DD.  It allows multiple DDs to reference the same chunk of user
// data.  The data would remain alive until the reference count goes to
// zero, at this time the user destructor function will be called with
// a pointer to the piece of data to deal with.  The intention here is
// that user will create a subclass of this and supply the virtual
// function run(void*).  The default behavior will be to treat the void*
// data as an array of aitInt8 and call remove.

class gddDestructor
{
public:
	gddDestructor(void);
	gddDestructor(void* user_arg);

	void reference(void);
	int refCount(void) const;

	gddStatus destroy(void* thing_to_remove);
	virtual void run(void* thing_to_remove);

	gdd_NEWDEL_FUNC(gddDestructor) // for using generic new and remove
protected:
	aitUint16 ref_cnt;
	void* arg;
private:
	gdd_NEWDEL_DATA(gddDestructor)
};

inline gddDestructor::gddDestructor(void) { ref_cnt=1; arg=NULL; }
inline gddDestructor::gddDestructor(void* usr_arg) { ref_cnt=1; arg=usr_arg; }
inline void gddDestructor::reference(void)		{ ref_cnt++; }
inline int gddDestructor::refCount(void) const	{ return ref_cnt; }

class gddFlattenDestructor : public gddDestructor
{
public:
	gddFlattenDestructor(void) { }
	gddFlattenDestructor(void* user_arg):gddDestructor(user_arg) { }
	void run(void*);
};

class gddContainerCleaner : public gddDestructor
{
public:
	gddContainerCleaner(void) { }
	gddContainerCleaner(void* user_arg):gddDestructor(user_arg) { }
	void run(void*);
};

#define GDD_MANAGED_MASK	0x01
#define GDD_FLAT_MASK		0x02
#define GDD_NET_MASK		0x04
#define GDD_NOREF_MASK		0x08
#define GDD_CONSTANT_MASK	0x10

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

	unsigned applicationType(void) const;
	aitEnum primitiveType(void) const;
	unsigned dimension(void) const;
	aitType& getData(void);
	aitType* dataUnion(void);
	gddDestructor* destructor(void) const;

	const gddBounds* getBounds(void) const;
	const gddBounds* getBounds(int bn) const;

	void dump(void);
	void test(void);

	void setPrimType(aitEnum t);
	void setApplType(int t);
	void setDimension(int d);
	void destroyData(void);
	gddStatus reset(aitEnum primtype,int dimension, aitIndex* dim_counts);
	gddStatus clear(void); // clear all fields of the DD, including arrays
	gddStatus clearData(void);
	gddStatus changeType(int appltype, aitEnum primtype);
	gddStatus setBound(unsigned dim_to_set, aitIndex first, aitIndex count);
	gddStatus getBound(unsigned dim_to_get, aitIndex& first, aitIndex& count);
	gddStatus registerDestructor(gddDestructor*);
	gddStatus replaceDestructor(gddDestructor*);
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

	gddStatus copyData(const gdd*);
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
	int isNetworkByteOrder(void) const;
	int isConstant(void) const;
	int isNoRef(void) const;

	void markConstant(void);
	void markManaged(void);
	void markUnmanaged(void);

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
	void putRef(void* v,aitEnum code, gddDestructor* d = NULL);
	void putRef(aitFloat64* v, gddDestructor* d = NULL);
	void putRef(aitFloat32* v, gddDestructor* d = NULL);
	void putRef(aitUint8* v, gddDestructor* d  = NULL);
	void putRef(aitInt8* v, gddDestructor* d = NULL);
	void putRef(aitUint16* v, gddDestructor* d = NULL);
	void putRef(aitInt16* v, gddDestructor* d = NULL);
	void putRef(aitUint32* v, gddDestructor* d = NULL);
	void putRef(aitInt32* v, gddDestructor* d = NULL);
	void putRef(aitString* v, gddDestructor* d = NULL);
	void putRef(aitFixedString* v, gddDestructor* d = NULL);
	// work with constants
	void putRef(const aitFloat64* v, gddDestructor* d = NULL);
	void putRef(const aitFloat32* v, gddDestructor* d = NULL);
	void putRef(const aitUint8* v, gddDestructor* d  = NULL);
	void putRef(const aitInt8* v, gddDestructor* d = NULL);
	void putRef(const aitUint16* v, gddDestructor* d = NULL);
	void putRef(const aitInt16* v, gddDestructor* d = NULL);
	void putRef(const aitUint32* v, gddDestructor* d = NULL);
	void putRef(const aitInt32* v, gddDestructor* d = NULL);
	void putRef(const aitString* v,gddDestructor* d=NULL);
	void putRef(const aitFixedString* v,gddDestructor* d=NULL);
	void putRef(const gdd*);

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
	void putConvert(aitString d);
	void putConvert(aitFixedString& d);

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
	gddStatus put(aitString d);
	gddStatus put(aitFixedString& d);
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
	gdd& operator=(aitString d);
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

	gddStatus genCopy(aitEnum t, const void* d);
	void adjust(gddDestructor* d, void* v, aitEnum type);
	void get(aitEnum t,void* v);
	void set(aitEnum t,void* v);

	gdd_NEWDEL_FUNC(gdd) // managed on free lists

protected:
	~gdd(void);  // users must call Unreference()

	void init(int app, aitEnum prim, int dimen);
	void freeBounds(void);
	void dumpInfo(void);
	gddStatus copyStuff(gdd*,int type);

	size_t describedDataSizeBytes(void) const;
	aitUint32 describedDataSizeElements(void) const;

	void markFlat(void);
	gddStatus flattenData(gdd* dd, int tot_dds, void* buf, size_t size);
	int flattenDDs(gddContainer* dd, void* buf, size_t size);
	aitUint32 align8(unsigned long count) const;

	void setData(void* d);

	aitType data;				// array pointer or scaler data
	gdd_NEWDEL_DATA(gdd)		// required for using generic new and remove
	gddBounds* bounds;			// array of bounds information length dim
	gddDestructor* destruct;	// NULL=none supplied, use remove
	aitTimeStamp time_stamp;
	aitStatus status;
	aitUint16 appl_type;		// application type code
	aitUint8 prim_type;			// primitive type code
	aitUint8 dim;				// 0=scaler, >0=array
private:
	aitUint16 ref_cnt;
	aitUint8 flags;
};

inline void gdd::setData(void* d)					{ data.Pointer=d; }
inline gddDestructor* gdd::destructor(void) const	{ return destruct; }

inline gdd::gdd(void)					{ init(0,aitEnumInvalid,0); }
inline gdd::gdd(int app)				{ init(app,aitEnumInvalid,0); }
inline gdd::gdd(int app,aitEnum prim)	{ init(app,prim,0); }

inline unsigned gdd::applicationType(void) const{ return appl_type; }
inline aitEnum gdd::primitiveType(void) const	{ return (aitEnum)prim_type; }
inline const gddBounds* gdd::getBounds(void) const	{ return bounds; }
inline const gddBounds* gdd::getBounds(int bn) const { return &bounds[bn]; }

inline unsigned gdd::dimension(void) const	{ return dim; }
inline aitType& gdd::getData(void) 			{ return data; }
inline aitType* gdd::dataUnion(void)		{ return &data; }
inline void gdd::setPrimType(aitEnum t)		{ prim_type=(aitUint8)t; }
inline void gdd::setApplType(int t)			{ appl_type=(aitUint16)t; }
inline gddStatus gdd::copyInfo(gdd* dd)		{ return copyStuff(dd,0); }
inline gddStatus gdd::copy(gdd* dd)			{ return copyStuff(dd,1); }
inline gddStatus gdd::Dup(gdd* dd)			{ return copyStuff(dd,2); }
inline void* gdd::dataAddress(void)	const	{ return (void*)&data; }
inline void* gdd::dataPointer(void)	const	{ return data.Pointer; }

inline aitUint32 gdd::align8(unsigned long count) const
{
	unsigned long tmp=count&(~((unsigned long)0x07));
	return (tmp!=count)?tmp+8:tmp;
}

inline void* gdd::dataPointer(aitIndex f) const
	{ return (void*)(((aitUint8*)dataPointer())+aitSize[primitiveType()]*f); }

inline int gdd::isManaged(void) const	{ return flags&GDD_MANAGED_MASK; }
inline int gdd::isFlat(void) const		{ return flags&GDD_FLAT_MASK; }
inline int gdd::isNoRef(void) const		{ return flags&GDD_NOREF_MASK; }
inline int gdd::isConstant(void) const	{ return flags&GDD_CONSTANT_MASK; }
inline int gdd::isNetworkByteOrder(void) const { return flags&GDD_NET_MASK; }

inline void gdd::markConstant(void)		{ flags|=GDD_CONSTANT_MASK; }
inline void gdd::markFlat(void)			{ flags|=GDD_FLAT_MASK; }
inline void gdd::markManaged(void)		{ flags|=GDD_MANAGED_MASK; }
inline void gdd::markUnmanaged(void)	{ flags&=~GDD_MANAGED_MASK; }

inline void gdd::getTimeStamp(struct timespec* const ts) const
	{ ts->tv_sec=time_stamp.tv_sec; ts->tv_nsec=time_stamp.tv_nsec; }
inline void gdd::setTimeStamp(const struct timespec* const ts) {
	time_stamp.tv_sec=(aitUint32)ts->tv_sec;
	time_stamp.tv_nsec=(aitUint32)ts->tv_nsec; }
inline void gdd::getTimeStamp(aitTimeStamp* const ts) const { *ts=time_stamp; }
inline void gdd::setTimeStamp(const aitTimeStamp* const ts) { time_stamp=*ts; }

inline void gdd::setStatus(aitUint32 s)		{ status=s; }
inline void gdd::getStatus(aitUint32& s)	{ s=status; }
inline void gdd::setStatus(aitUint16 high, aitUint16 low)
	{ status=(((aitUint32)high)<<16)|low; }
inline void gdd::getStatus(aitUint16& high, aitUint16& low)
	{ high=(aitUint16)(status>>16); low=(aitUint16)(status&0x0000ffff); }

inline void gdd::setStat(aitUint16 s)
	{ aitUint16* x = (aitUint16*)&status; x[0]=s; }
inline void gdd::setSevr(aitUint16 s)
	{ aitUint16* x = (aitUint16*)&status; x[1]=s; }
inline aitUint16 gdd::getStat(void) const
	{ aitUint16* x = (aitUint16*)&status; return x[0]; }
inline aitUint16 gdd::getSevr(void) const
	{ aitUint16* x = (aitUint16*)&status; return x[1]; }
inline void gdd::getStatSevr(aitInt16& st, aitInt16& se)
	{ st=getStat(); se=getSevr(); }
inline void gdd::setStatSevr(aitInt16 st, aitInt16 se)
	{ setStat(st); setSevr(se); }

inline gdd& gdd::operator=(const gdd& v)
	{ memcpy(this,&v,sizeof(gdd)); return *this; }

inline int gdd::isScalar(void) const { return dimension()==0?1:0; }
inline int gdd::isContainer(void) const
	{ return (primitiveType()==aitEnumContainer)?1:0; }
inline int gdd::isAtomic(void) const
	{ return (dimension()>0&&primitiveType()!=aitEnumContainer)?1:0; }
inline gddStatus gdd::noReferencing(void)
{
	int rc=0;
	if(ref_cnt>1)	rc=gddErrorNotAllowed;
	else			flags|=GDD_NOREF_MASK;
	return rc;
}
inline gddStatus gdd::reference(void)
{
	int rc=0;

	if(isNoRef())	rc=gddErrorNotAllowed;
	else			ref_cnt++;

	if(ref_cnt>((1u<<(sizeof(ref_cnt)*CHAR_BIT))-2u))
	{
		fprintf(stderr,"gdd reference count overflow!!\n");
		rc=gddErrorOverflow;
	}
	return rc;
}

inline gddStatus gdd::unreference(void)
{
	int rc=0;

	if(ref_cnt==0u)
	{
		fprintf(stderr,"gdd reference count underflow!!\n");
		rc=gddErrorUnderflow;
	}
	else if(--ref_cnt<=0u)
	{
		if(isManaged())
		{
			// managed dd always destroys the entire thing
			ref_cnt=1;
			if(destruct) destruct->run(this);
		}
		else if(!isFlat())
			delete this;
	}
	return rc;
}

inline void gdd::destroyData(void)
{
	if(destruct)
	{
		// up to destructor to free the bounds
		if(isContainer())
			destruct->run(this);
		else
			destruct->run(dataPointer());
	}
	else
	{
		if(primitiveType()==aitEnumString && isScalar())
		{
			aitString* str = (aitString*)dataAddress();
			str->clear();
		}
	}
}

inline void gdd::adjust(gddDestructor* d, void* v, aitEnum type)
{
	if(destruct) destruct->destroy(dataPointer());
	destruct=d;
	if(destruct) destruct->reference();
	setPrimType(type);
	setData(v);
}

// These function do NOT work well for aitFixedString because
// fixed strings are always stored by reference.  You WILL get
// into trouble is the gdd does not contain a fixed string
// before you putConvert() something into it.

inline void gdd::get(aitEnum t,void* v)
{
	if(primitiveType()==aitEnumFixedString)
	{
		if(dataPointer()) aitConvert(t,v,primitiveType(),dataPointer(),1);
	}
	else
		aitConvert(t,v,primitiveType(),dataAddress(),1);
}

inline void gdd::set(aitEnum t,void* v)
{
	if(primitiveType()==aitEnumFixedString)
	{
		if(dataPointer()==NULL) data.FString=new aitFixedString;
		aitConvert(primitiveType(),dataPointer(),t,v,1);
	}
	else
		aitConvert(primitiveType(),dataAddress(),t,v,1);
}

// -------------------getRef(data pointer) functions----------------
inline void gdd::getRef(aitFloat64*& d)	{ d=(aitFloat64*)dataPointer(); }
inline void gdd::getRef(aitFloat32*& d)	{ d=(aitFloat32*)dataPointer(); }
inline void gdd::getRef(aitUint32*& d)	{ d=(aitUint32*)dataPointer(); }
inline void gdd::getRef(aitInt32*& d)	{ d=(aitInt32*)dataPointer(); }
inline void gdd::getRef(aitUint16*& d)	{ d=(aitUint16*)dataPointer(); }
inline void gdd::getRef(aitInt16*& d)	{ d=(aitInt16*)dataPointer(); }
inline void gdd::getRef(aitUint8*& d)	{ d=(aitUint8*)dataPointer(); }
inline void gdd::getRef(aitInt8*& d)	{ d=(aitInt8*)dataPointer(); }
inline void gdd::getRef(void*& d)		{ d=dataPointer(); }
inline void gdd::getRef(aitFixedString*& d) {d=(aitFixedString*)dataPointer();}
inline void gdd::getRef(aitString*& d) {
	if(isScalar()) d=(aitString*)dataAddress();
	else d=(aitString*)dataPointer();
}

// -------------------putRef(data pointer) functions----------------
inline void gdd::putRef(void* v,aitEnum code, gddDestructor* d)
	{ adjust(d, v, code); }
inline void gdd::putRef(aitFloat64* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumFloat64); }
inline void gdd::putRef(aitFloat32* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumFloat32); }
inline void gdd::putRef(aitUint8* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumUint8); }
inline void gdd::putRef(aitInt8* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumInt8); }
inline void gdd::putRef(aitUint16* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumUint16); }
inline void gdd::putRef(aitInt16* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumInt16); }
inline void gdd::putRef(aitUint32* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumUint32); }
inline void gdd::putRef(aitInt32* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumInt32); }
inline void gdd::putRef(aitString* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumString); }
inline void gdd::putRef(aitFixedString* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumFixedString); }

// -------------------putRef(const data pointer) functions----------------
inline void gdd::putRef(const aitFloat64* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumFloat64); markConstant(); }
inline void gdd::putRef(const aitFloat32* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumFloat32); markConstant(); }
inline void gdd::putRef(const aitUint8* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumUint8); markConstant(); }
inline void gdd::putRef(const aitInt8* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumInt8); markConstant(); }
inline void gdd::putRef(const aitUint16* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumUint16); markConstant(); }
inline void gdd::putRef(const aitInt16* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumInt16); markConstant(); }
inline void gdd::putRef(const aitUint32* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumUint32); markConstant(); }
inline void gdd::putRef(const aitInt32* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumInt32); markConstant(); }
inline void gdd::putRef(const aitString* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumString); markConstant(); }
inline void gdd::putRef(const aitFixedString* v, gddDestructor* d)
	{ adjust(d, (void*)v, aitEnumFixedString); markConstant(); }

// -------------------getConvert(scalar) functions ----------------------
inline void gdd::getConvert(aitFloat64& d)	{ get(aitEnumFloat64,&d); }
inline void gdd::getConvert(aitFloat32& d)	{ get(aitEnumFloat32,&d); }
inline void gdd::getConvert(aitUint32& d)	{ get(aitEnumUint32,&d); }
inline void gdd::getConvert(aitInt32& d)	{ get(aitEnumInt32,&d); }
inline void gdd::getConvert(aitUint16& d)	{ get(aitEnumUint16,&d); }
inline void gdd::getConvert(aitInt16& d)	{ get(aitEnumInt16,&d); }
inline void gdd::getConvert(aitUint8& d)	{ get(aitEnumUint8,&d); }
inline void gdd::getConvert(aitInt8& d)		{ get(aitEnumInt8,&d); }

// -------------------putConvert(scalar) functions ----------------------
inline void gdd::putConvert(aitFloat64 d){ set(aitEnumFloat64,&d); }
inline void gdd::putConvert(aitFloat32 d){ set(aitEnumFloat32,&d); }
inline void gdd::putConvert(aitUint32 d) { set(aitEnumUint32,&d); }
inline void gdd::putConvert(aitInt32 d)  { set(aitEnumInt32,&d); }
inline void gdd::putConvert(aitUint16 d) { set(aitEnumUint16,&d); }
inline void gdd::putConvert(aitInt16 d)  { set(aitEnumInt16,&d); }
inline void gdd::putConvert(aitUint8 d)  { set(aitEnumUint8,&d); }
inline void gdd::putConvert(aitInt8 d)	 { set(aitEnumInt8,&d); }

// ------------------------put(pointer) functions----------------------
inline gddStatus gdd::put(const aitFloat64* const d)
	{ return genCopy(aitEnumFloat64,d); }
inline gddStatus gdd::put(const aitFloat32* const d)
	{ return genCopy(aitEnumFloat32,d); }
inline gddStatus gdd::put(const aitUint32* const d)
	{ return genCopy(aitEnumUint32,d); }
inline gddStatus gdd::put(const aitInt32* const d)
	{ return genCopy(aitEnumInt32,d); }
inline gddStatus gdd::put(const aitUint16* const d)
	{ return genCopy(aitEnumUint16,d); }
inline gddStatus gdd::put(const aitInt16* const d)
	{ return genCopy(aitEnumInt16,d); }
inline gddStatus gdd::put(const aitUint8* const d)
	{ return genCopy(aitEnumUint8,d); }

// special case for aitInt8 array to aitString scalar
inline gddStatus gdd::put(const aitInt8* const d)
{
	gddStatus rc=0;

	if(primitiveType()==aitEnumString && dim==0)
	{
		aitString* p = (aitString*)dataAddress();
		p->installString((char*)d);
	}
	else if(primitiveType()==aitEnumFixedString && dim==0)
		strcpy(data.FString->fixed_string,(char*)d);
	else
		rc=genCopy(aitEnumInt8,d);

	return rc;
}

// ----------------put(scalar) functions----------------
inline gddStatus gdd::put(aitFloat64 d) {
	gddStatus rc=0;
	if(isScalar()) { data.Float64=d; setPrimType(aitEnumFloat64); }
	else rc=gddErrorNotAllowed;
	return rc;
}
inline gddStatus gdd::put(aitFloat32 d) {
	gddStatus rc=0;
	if(isScalar()) { data.Float32=d;setPrimType(aitEnumFloat32); }
	else rc=gddErrorNotAllowed;
	return rc;
}
inline gddStatus gdd::put(aitUint32 d) {
	gddStatus rc=0;
	if(isScalar()) { data.Uint32=d; setPrimType(aitEnumUint32); }
	else rc=gddErrorNotAllowed;
	return rc;
}
inline gddStatus gdd::put(aitInt32 d) {
	gddStatus rc=0;
	if(isScalar()) { data.Int32=d; setPrimType(aitEnumInt32); }
	else rc=gddErrorNotAllowed;
	return rc;
}
inline gddStatus gdd::put(aitUint16 d) {
	gddStatus rc=0;
	if(isScalar()) { data.Uint16=d; setPrimType(aitEnumUint16); }
	else rc=gddErrorNotAllowed;
	return rc;
}
inline gddStatus gdd::put(aitInt16 d) {
	gddStatus rc=0;
	if(isScalar()) { data.Int16=d; setPrimType(aitEnumInt16); }
	else rc=gddErrorNotAllowed;
	return rc;
}
inline gddStatus gdd::put(aitUint8 d) {
	gddStatus rc=0;
	if(isScalar()) { data.Uint8=d; setPrimType(aitEnumUint8); }
	else rc=gddErrorNotAllowed;
	return rc;
}
inline gddStatus gdd::put(aitInt8 d) {
	gddStatus rc=0;
	if(isScalar()) { data.Int8=d; setPrimType(aitEnumInt8); }
	else rc=gddErrorNotAllowed;
	return rc;
}
inline gddStatus gdd::put(aitType* d) {
	gddStatus rc=0;
	if(isScalar()) { data=*d; }
	else rc=gddErrorNotAllowed;
	return rc;
}

// ---------------------get(pointer) functions--------------------------
inline void gdd::get(void* d) {
	aitConvert(primitiveType(),d,primitiveType(),dataPointer(),
		getDataSizeElements());
}
inline void gdd::get(void* d,aitEnum e) {
	aitConvert(e,d,primitiveType(),dataPointer(),
		getDataSizeElements());
}
inline void gdd::get(aitFloat64* d)
{
	aitConvert(aitEnumFloat64,d,primitiveType(),dataPointer(),
		getDataSizeElements());
}
inline void gdd::get(aitFloat32* d) {
	aitConvert(aitEnumFloat32,d,primitiveType(),dataPointer(),
		getDataSizeElements());
}
inline void gdd::get(aitUint32* d) {
	aitConvert(aitEnumUint32,d,primitiveType(),dataPointer(),
		getDataSizeElements());
}
inline void gdd::get(aitInt32* d) {
	aitConvert(aitEnumInt32,d,primitiveType(),dataPointer(),
		getDataSizeElements());
}
inline void gdd::get(aitUint16* d) {
	aitConvert(aitEnumUint16,d,primitiveType(),dataPointer(),
		getDataSizeElements());
}
inline void gdd::get(aitInt16* d) {
	aitConvert(aitEnumInt16,d,primitiveType(),dataPointer(),
		getDataSizeElements());
}
inline void gdd::get(aitUint8* d) {
	aitConvert(aitEnumUint8,d,primitiveType(),dataPointer(),
		getDataSizeElements());
}
inline void gdd::get(aitString* d) {
	aitConvert(aitEnumString,d,primitiveType(),dataPointer(),
		getDataSizeElements());
}
inline void gdd::get(aitFixedString* d) {
	aitConvert(aitEnumFixedString,d,primitiveType(),dataPointer(),
		getDataSizeElements());
}

// special case for string scalar to aitInt8 array!
inline void gdd::get(aitInt8* d)
{
	if(primitiveType()==aitEnumString && dim==0)
	{
		aitString* str = (aitString*)dataAddress();
		strcpy((char*)d,str->string());
	}
	else if(primitiveType()==aitEnumFixedString && dim==0)
		strcpy((char*)d,data.FString->fixed_string);
	else
		aitConvert(aitEnumInt8,d,primitiveType(),dataPointer(),
			getDataSizeElements());
}

// ------------------get(scalar) functions-----------------
inline void gdd::get(aitFloat64& d) {
	if(primitiveType()==aitEnumFloat64) d=getData().Float64;
	else get(aitEnumFloat64,&d);
}
inline void gdd::get(aitFloat32& d)	{
	if(primitiveType()==aitEnumFloat32) d=getData().Float32;
	else get(aitEnumFloat32,&d);
}
inline void gdd::get(aitUint32& d) {
	if(primitiveType()==aitEnumUint32) d=getData().Uint32;
	else get(aitEnumUint32,&d);
}
inline void gdd::get(aitInt32& d) {
	if(primitiveType()==aitEnumInt32) d=getData().Int32;
	else get(aitEnumInt32,&d);
}
inline void gdd::get(aitUint16& d) {
	if(primitiveType()==aitEnumUint16) d=getData().Uint16;
	else get(aitEnumUint16,&d);
}
inline void gdd::get(aitInt16& d) {
	if(primitiveType()==aitEnumInt16) d=getData().Int16;
	else get(aitEnumInt16,&d);
}
inline void gdd::get(aitUint8& d) {
	if(primitiveType()==aitEnumUint8) d=getData().Uint8;
	else get(aitEnumUint8,&d);
}
inline void gdd::get(aitInt8& d) {
	if(primitiveType()==aitEnumInt8) d=getData().Int8;
	else get(aitEnumInt8,&d);
}
inline void gdd::get(aitType& d)	{ d=data; }

// ---------- gdd x = primitive data type pointer functions----------
inline gdd& gdd::operator=(aitFloat64* v)	{ putRef(v); return *this;}
inline gdd& gdd::operator=(aitFloat32* v)	{ putRef(v); return *this;}
inline gdd& gdd::operator=(aitUint32* v)	{ putRef(v); return *this;}
inline gdd& gdd::operator=(aitInt32* v)		{ putRef(v); return *this;}
inline gdd& gdd::operator=(aitUint16* v)	{ putRef(v); return *this;}
inline gdd& gdd::operator=(aitInt16* v)		{ putRef(v); return *this;}
inline gdd& gdd::operator=(aitUint8* v)		{ putRef(v); return *this;}
inline gdd& gdd::operator=(aitInt8* v)		{ putRef(v); return *this;}
inline gdd& gdd::operator=(aitString* v)	{ putRef(v); return *this;}
inline gdd& gdd::operator=(aitFixedString* v){ putRef(v); return *this;}

// ----------------- gdd x = primitive data type functions----------------
inline gdd& gdd::operator=(aitFloat64 d)
	{ data.Float64=d; setPrimType(aitEnumFloat64); return *this; }
inline gdd& gdd::operator=(aitFloat32 d)
	{ data.Float32=d;setPrimType(aitEnumFloat32); return *this; }
inline gdd& gdd::operator=(aitUint32 d)
	{ data.Uint32=d; setPrimType(aitEnumUint32); return *this; }
inline gdd& gdd::operator=(aitInt32 d)	
	{ data.Int32=d; setPrimType(aitEnumInt32); return *this; }
inline gdd& gdd::operator=(aitUint16 d)
	{ data.Uint16=d; setPrimType(aitEnumUint16); return *this; }
inline gdd& gdd::operator=(aitInt16 d)
	{ data.Int16=d; setPrimType(aitEnumInt16); return *this; }
inline gdd& gdd::operator=(aitUint8 d)
	{ data.Uint8=d; setPrimType(aitEnumUint8); return *this; }
inline gdd& gdd::operator=(aitInt8 d)
	{ data.Int8=d; setPrimType(aitEnumInt8); return *this; }
inline gdd& gdd::operator=(aitString d)
	{ put(d); return *this; }

// ------------- primitive type pointer = gdd x functions --------------
inline gdd::operator aitFloat64*(void) const
	{ return (aitFloat64*)dataPointer(); }
inline gdd::operator aitFloat32*(void) const
	{ return (aitFloat32*)dataPointer(); }
inline gdd::operator aitUint32*(void) const
	{ return (aitUint32*)dataPointer(); }
inline gdd::operator aitInt32*(void) const
	{ return (aitInt32*)dataPointer(); }
inline gdd::operator aitUint16*(void) const
	{ return (aitUint16*)dataPointer(); }
inline gdd::operator aitInt16*(void) const
	{ return (aitInt16*)dataPointer(); }
inline gdd::operator aitUint8*(void) const
	{ return (aitUint8*)dataPointer(); }
inline gdd::operator aitInt8*(void)	 const
	{ return (aitInt8*)dataPointer(); }
inline gdd::operator aitString*(void) const
	{ return (aitString*)dataPointer(); }
inline gdd::operator aitFixedString*(void) const
	{ return (aitFixedString*)dataPointer(); }

// ------------- primitive type = gdd x functions --------------
inline gdd::operator aitFloat64(void)	{ aitFloat64 d; get(d); return d; }
inline gdd::operator aitFloat32(void)	{ aitFloat32 d; get(d); return d; }
inline gdd::operator aitUint32(void)	{ aitUint32 d; get(d); return d; }
inline gdd::operator aitInt32(void)		{ aitInt32 d; get(d); return d; }
inline gdd::operator aitUint16(void)	{ aitUint16 d; get(d); return d; }
inline gdd::operator aitInt16(void)		{ aitInt16 d; get(d); return d; }
inline gdd::operator aitUint8(void)		{ aitUint8 d; get(d); return d; }
inline gdd::operator aitInt8(void)		{ aitInt8 d; get(d); return d; }
inline gdd::operator aitString(void)	{ aitString d; get(d); return d; }

// ***********************************************************************
// Adds ability to put array data into a DD, get it out, and adjust it
// ***********************************************************************

class gddAtomic : public gdd
{
public:
	gddAtomic(void) { }
	gddAtomic(gddAtomic* ad) : gdd(ad) { }
	gddAtomic(int app) : gdd(app) { }
	gddAtomic(int app, aitEnum prim) : gdd(app,prim) { }
	gddAtomic(int app, aitEnum prim, int dimen, aitUint32* size_array):
		gdd(app,prim,dimen,size_array) { }
	gddAtomic(int app, aitEnum prim, int dimen, ...);

	// view dimension size info as a bounding box instead of first/count
	gddStatus getBoundingBoxSize(aitUint32* put_info_here);
	gddStatus setBoundingBoxSize(const aitUint32* const get_info_from_here);
	gddStatus getBoundingBoxOrigin(aitUint32* put_info_here);
	gddStatus setBoundingBoxOrigin(const aitUint32* const get_info_from_here);

	void dump(void);
	void test(void);

	gddAtomic& operator=(aitFloat64* v) { *((gdd*)this)=v; return *this; }
	gddAtomic& operator=(aitFloat32* v) { *((gdd*)this)=v; return *this; }
	gddAtomic& operator=(aitUint32* v) { *((gdd*)this)=v; return *this; }
	gddAtomic& operator=(aitInt32* v) { *((gdd*)this)=v; return *this; }
	gddAtomic& operator=(aitUint16* v) { *((gdd*)this)=v; return *this; }
	gddAtomic& operator=(aitInt16* v) { *((gdd*)this)=v; return *this; }
	gddAtomic& operator=(aitUint8* v) { *((gdd*)this)=v; return *this; }
	gddAtomic& operator=(aitInt8* v) { *((gdd*)this)=v; return *this; }

protected:
	~gddAtomic(void) { }
private:
};

// ------------------------------------------------------------------------
// Add handling of the special case where the data is a scaler - the
// dimension is zero

class gddScalar : public gddAtomic
{
public:
	gddScalar(void) { }
	gddScalar(gddScalar* ad) : gddAtomic(ad) { }
	gddScalar(int app) : gddAtomic(app) { }
	gddScalar(int app,aitEnum prim) : gddAtomic(app,prim) { }

	void dump(void);
	void test(void);

	gddScalar& operator=(aitFloat64 d) { *((gdd*)this)=d; return *this; }
	gddScalar& operator=(aitFloat32 d) { *((gdd*)this)=d; return *this; }
	gddScalar& operator=(aitUint32 d) { *((gdd*)this)=d; return *this; }
	gddScalar& operator=(aitInt32 d) { *((gdd*)this)=d; return *this; }
	gddScalar& operator=(aitUint16 d) { *((gdd*)this)=d; return *this; }
	gddScalar& operator=(aitInt16 d) { *((gdd*)this)=d; return *this; }
	gddScalar& operator=(aitUint8 d) { *((gdd*)this)=d; return *this; }
	gddScalar& operator=(aitInt8 d) { *((gdd*)this)=d; return *this; }

protected:
	gddScalar(int app, aitEnum prim, int dimen, aitUint32* size_array):
		gddAtomic(app,prim,dimen,size_array) { }
	~gddScalar(void) { }

	// disallow
	const gddBounds* getBounds(void)				{ return NULL; }
	gddStatus getBoundingBoxSize(aitUint32*)	{ return gddErrorNotAllowed; }
	gddStatus setBoundingBoxSize(const aitUint32* const)
		{ return gddErrorNotAllowed; }
	gddStatus getBoundingBoxOrigin(aitUint32*)	{ return gddErrorNotAllowed; }
	gddStatus setBoundingBoxOrigin(const aitUint32* const)
		{ return gddErrorNotAllowed; }
	gddStatus setBound(int,aitIndex,aitIndex)	{ return gddErrorNotAllowed; }
	gddStatus getBound(int,aitIndex&,aitIndex&)	{ return gddErrorNotAllowed; }

	// disallow
	void adjust(aitFloat64* const, gddDestructor*)	{ }
	void adjust(aitFloat32* const, gddDestructor*)	{ }
	void adjust(aitUint32* const, gddDestructor*)	{ }
	void adjust(aitInt32* const, gddDestructor*)	{ }
	void adjust(aitUint16* const, gddDestructor*)	{ }
	void adjust(aitInt16* const, gddDestructor*)	{ }
	void adjust(aitUint8* const, gddDestructor*)	{ }
	void adjust(aitInt8* const, gddDestructor*)		{ }

private:
};

// ------------------------------------------------------------------------

class gddCursor;

// this class needs to be able to register a destructor for the container

class gddContainer : public gdd
{
public:
	gddContainer(void);
	gddContainer(int app);
	gddContainer(gddContainer* ac);
	gddContainer(int app,int number_of_things_in_it);

	gddStatus insert(gdd*);
	gddStatus remove(aitIndex index);
	int total(void);

	void dump(void);
	void test(void);

	// The following are slow and inefficient
	gdd* getDD(aitIndex index);
	gdd* getDD(aitIndex index,gddScalar*&);
	gdd* getDD(aitIndex index,gddAtomic*&);
	gdd* getDD(aitIndex index,gddContainer*&);
	gdd* operator[](aitIndex index);

	// preferred method for looking into a container
	gddCursor getCursor(void) const;
	gdd* cData(void) const;

protected:
	gddContainer(int,int,int,int*) { }
	~gddContainer(void) { }

	void cInit(int num_things_within);
	gddStatus changeType(int,aitEnum)			{ return gddErrorNotAllowed; }
	gddStatus setBound(int,aitIndex,aitIndex)	{ return gddErrorNotAllowed; }
	gddStatus getBound(int,aitIndex&,aitIndex&)	{ return gddErrorNotAllowed; }
	gddStatus setBound(aitIndex,aitIndex);

private:
	friend class gddCursor;
};

inline gdd* gddContainer::cData(void) const
	{ return (gdd*)dataPointer(); }
inline int gddContainer::total(void)
	{ return bounds->size(); }
inline gdd* gddContainer::operator[](aitIndex index)
	{ return getDD(index); }
inline gdd* gddContainer::getDD(aitIndex index,gddScalar*& dd)
	{ return (gdd*)(dd=(gddScalar*)getDD(index)); }
inline gdd* gddContainer::getDD(aitIndex index,gddAtomic*& dd)
	{ return (gdd*)(dd=(gddAtomic*)getDD(index)); }
inline gdd* gddContainer::getDD(aitIndex index,gddContainer*& dd)
	{ return (gdd*)(dd=(gddContainer*)getDD(index)); }
inline gddStatus gddContainer::setBound(aitIndex f, aitIndex c)
	{ bounds->set(f,c); return 0; }

class gddCursor
{
public:
	gddCursor(void);
	gddCursor(const gddContainer* ec);

	gdd* first(void);
	gdd* first(gddScalar*&);
	gdd* first(gddAtomic*&);
	gdd* first(gddContainer*&);

	gdd* next(void);
	gdd* next(gddScalar*&);
	gdd* next(gddAtomic*&);
	gdd* next(gddContainer*&);

	gdd* current(void);
	gdd* current(gddScalar*&);
	gdd* current(gddAtomic*&);
	gdd* current(gddContainer*&);

	gdd* operator[](int index);
private:
	const gddContainer* list;
	gdd* curr;
	int curr_index;
};

inline gddCursor::gddCursor(void):list(NULL)
	{ curr=NULL; }
inline gddCursor::gddCursor(const gddContainer* ec):list(ec)
	{ curr=ec->cData(); curr_index=0; }

inline gdd* gddCursor::first(void)
	{ curr=list->cData(); curr_index=0; return curr; }
inline gdd* gddCursor::first(gddScalar*& dd)
	{ return (gdd*)(dd=(gddScalar*)first()); }
inline gdd* gddCursor::first(gddAtomic*& dd)
	{ return (gdd*)(dd=(gddAtomic*)first()); }
inline gdd* gddCursor::first(gddContainer*& dd)
	{ return (gdd*)(dd=(gddContainer*)first()); }

inline gdd* gddCursor::next(void)
	{ if(curr) { curr_index++;curr=curr->next(); } return curr; }
inline gdd* gddCursor::next(gddScalar*& dd)
	{ return (gdd*)(dd=(gddScalar*)next()); }
inline gdd* gddCursor::next(gddAtomic*& dd)
	{ return (gdd*)(dd=(gddAtomic*)next()); }
inline gdd* gddCursor::next(gddContainer*& dd)
	{ return (gdd*)(dd=(gddContainer*)next()); }

inline gdd* gddCursor::current(void)
	{ return curr; }
inline gdd* gddCursor::current(gddScalar*& dd)
	{ return (gdd*)(dd=(gddScalar*)current()); }
inline gdd* gddCursor::current(gddAtomic*& dd)
	{ return (gdd*)(dd=(gddAtomic*)current()); }
inline gdd* gddCursor::current(gddContainer*& dd)
	{ return (gdd*)(dd=(gddContainer*)current()); }

#endif
