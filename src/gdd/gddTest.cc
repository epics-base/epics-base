// Author: Jim Kowalkowski
// Date: 2/96
// 
// $Id$
// 
// $Log$
//

// *Revision 1.3  1996/06/24 03:15:38  jbk
// *name changes and fixes for aitString and fixed string functions
// *Revision 1.2  1996/06/13 21:32:00  jbk
// *Various fixes and correction - including ref_cnt change to unsigned short
// *Revision 1.1  1996/05/31 13:15:35  jbk
// *add new stuff

#include <stdio.h>
#include "gdd.h"

// -----------------------test routines------------------------

#ifdef NO_DUMP_TEST
void gdd::dump(void) { }
#else
void gdd::dump(void)
{
	gddScaler* sdd;
	gddAtomic* add;
	gddContainer* cdd;

	if(isScaler())
	{
		sdd=(gddScaler*)this;
		sdd->dump();
		return;
	}
	if(isAtomic())
	{
		add=(gddAtomic*)this;
		add->dump();
		return;
	}
	if(isContainer())
	{
		cdd=(gddContainer*)this;
		cdd->dump();
		return;
	}
}
#endif

#ifdef NO_DUMP_TEST
void gdd::dumpInfo(void) { }
#else
void gdd::dumpInfo(void)
{
	unsigned i;
	aitIndex f,c;
	long sz_tot,sz_data,sz_elem;

	sz_tot = getTotalSizeBytes();
	sz_data = getDataSizeBytes();
	sz_elem = getDataSizeElements();

	fprintf(stderr,"----------dump This=%8.8x---------\n",(unsigned int)this);
	fprintf(stderr," app=%d, prim=%d",applicationType(),primitiveType());
	fprintf(stderr," dim=%d\n",dimension());
	fprintf(stderr," tot=%ld, data=%ld, elem=%ld ",sz_tot,sz_data,sz_elem);
	fprintf(stderr," ref count=%d\n",ref_cnt);

	for(i=0;i<dimension();i++)
	{
		getBound(i,f,c);
		fprintf(stderr," (%d) %8.8x first=%d count=%d\n",i,&bounds[i],f,c);
	}

	if(isScaler()) fprintf(stderr," Is a Scaler\n");
	if(isAtomic()) fprintf(stderr," Is a Atomic\n");
	if(isContainer()) fprintf(stderr," Is a Container\n");

	if(!isContainer() && !isScaler() && !isAtomic())
		 fprintf(stderr,"--------------------------------------\n");
}
#endif

#ifdef NO_DUMP_TEST
void gddAtomic::dump(void) { }
#else
void gddAtomic::dump(void)
{
	aitFloat64* f64;	aitFloat32* f32;
	aitUint32* ui32;	aitInt32* i32;
	aitUint16* ui16;	aitInt16* i16;
	aitUint8* ui8;		aitInt8* i8;
	char* str;

	gdd::dumpInfo();

	switch(primitiveType())
	{
	case aitEnumFloat64:
		getRef(f64);
		if(f64) fprintf(stderr," Convert: float64 %8.8x %lf ",f64,f64[0]);
		f64=*this;
		if(f64) fprintf(stderr," Normal: float64 %8.8x %lf\n",f64,f64[0]);
		break;
	case aitEnumFloat32:
		getRef(f32);
		if(f32) fprintf(stderr," Convert: float32 %8.8x %f ",f32,f32[0]);
		f32=*this;
		if(f32) fprintf(stderr," Normal: float32 %8.8x %f\n",f32,f32[0]);
		break;
	case aitEnumUint32:
		getRef(ui32);
		if(ui32) fprintf(stderr," Convert: uint32 %8.8x %d ",ui32,ui32[0]);
		ui32=*this;
		if(ui32) fprintf(stderr," Normal: uint32 %8.8x %d\n",ui32,ui32[0]);
		break;
	case aitEnumInt32:
		getRef(i32);
		if(i32) fprintf(stderr," Convert: int32 %8.8x %d ",i32,i32[0]);
		i32=*this;
		if(i32) fprintf(stderr," Normal: int32 %8.8x %d\n",i32,i32[0]);
		break;
	case aitEnumUint16:
		getRef(ui16);
		if(ui16) fprintf(stderr," Convert: uint16 %8.8x %hu ",ui16,ui16[0]);
		ui16=*this;
		if(ui16) fprintf(stderr," Normal: uint16 %8.8x %hu\n",ui16,ui16[0]);
		break;
	case aitEnumInt16:
		getRef(i16);
		if(i16) fprintf(stderr," Convert: int16 %8.8x %hd ",i16,i16[0]);
		i16=*this;
		if(i16) fprintf(stderr," Normal: int16 %8.8x %hd\n",i16,i16[0]);
		break;
	case aitEnumUint8:
		getRef(ui8);
		if(ui8) fprintf(stderr," Convert: uint8 %8.8x %d ",ui8,ui8[0]);
		ui8=*this;
		if(ui8) fprintf(stderr," Normal: uint8 %8.8x %d\n",ui8,ui8[0]);
		break;
	case aitEnumInt8:
		getRef(i8);
		if(i8) fprintf(stderr," Convert: int8 %8.8x %d ",i8,i8[0]);
		i8=*this;
		if(i8) fprintf(stderr," Normal: int8 %8.8x %d\n",i8,i8[0]);
		break;
	case aitEnumString:
		getRef(str);
		if(str) fprintf(stderr," <%s>\n",str);
		break;
	default:
		fprintf(stderr," unknown primitive type\n"); break;
	}
	fprintf(stderr,"-------------------------------------\n");
}
#endif

#ifdef NO_DUMP_TEST
void gddScaler::dump(void) { }
#else
void gddScaler::dump(void)
{
	aitFloat64 f64;	aitFloat32 f32;	aitUint32 ui32;	aitInt32 i32;
	aitUint16 ui16;	aitInt16 i16;		aitUint8 ui8;		aitInt8 i8;

	gdd::dumpInfo();

	switch(primitiveType())
	{
	case aitEnumFloat64:
		get(f64); if(f64) fprintf(stderr," Convert: float64 %lf ",f64);
		f64=*this; if(f64) fprintf(stderr," Normal: float64 %lf\n",f64);
		break;
	case aitEnumFloat32:
		get(f32); if(f32) fprintf(stderr," Convert: float32 %f ",f32);
		f32=*this; if(f32) fprintf(stderr," Normal: float32 %f\n",f32);
		break;
	case aitEnumUint32:
		get(ui32); if(ui32) fprintf(stderr," Convert: uint32 %d ",ui32);
		ui32=*this; if(ui32) fprintf(stderr," Normal: uint32 %d\n",ui32);
		break;
	case aitEnumInt32:
		get(i32); if(i32) fprintf(stderr," Convert: int32 %d ",i32);
		i32=*this; if(i32) fprintf(stderr," Normal: int32 %d\n",i32);
		break;
	case aitEnumUint16:
		get(ui16); if(ui16) fprintf(stderr," Convert: uint16 %hu ",ui16);
		ui16=*this; if(ui16) fprintf(stderr," Normal: uint16 %hu\n",ui16);
		break;
	case aitEnumInt16:
		get(i16); if(i16) fprintf(stderr," Convert: int16 %hd ",i16);
		i16=*this; if(i16) fprintf(stderr," Normal: int16 %hd\n",i16);
		break;
	case aitEnumUint8:
		get(ui8); if(ui8) fprintf(stderr," Convert: uint8 %2.2x ",ui8);
		ui8=*this; if(ui8) fprintf(stderr," Normal: uint8 %2.2x\n",ui8);
		break;
	case aitEnumInt8:
		get(i8); if(i8) fprintf(stderr," Convert: int8 %d ",i8);
		i8=*this; if(i8) fprintf(stderr," Normal: int8 %d\n",i8);
		break;
	default:
		fprintf(stderr," unknown primitive type\n"); break;
	}
	fprintf(stderr,"--------------------------------------\n");
}
#endif

#ifdef NO_DUMP_TEST
void gdd::test() { }
#else
void gdd::test()
{
	aitInt32 i32[3] = { -32,4,3 };
	aitIndex bnds = 3;
	gddAtomic* add = (gddAtomic*)this;
	gddAtomic* dd = new gddAtomic(98,aitEnumInt32,1,3);

	reset(aitEnumInt32,1,&bnds);
	add->put(i32);
	fprintf(stderr,"----TESTING DD DUMP:\n");
	add->dump();

	// test copy(), copyInfo(), Dup()
	fprintf(stderr,"----TESTING COPYINFO(): (1)COPYINFO, (2)ORIGINAL\n");
	dd->copyInfo(this); dd->dump(); add->dump();
	fprintf(stderr,"----TESTING DUP(): (1)DUP, (2)ORIGINAL\n");
	dd->clear(); dd->Dup(this); dd->dump(); add->dump();
	fprintf(stderr,"----TESTING COPY(): (1)COPY, (2)ORIGINAL\n");
	dd->clear(); dd->copy(this); dd->dump(); add->dump();
	dd->unreference();

	// test flatten functions and Convert functions
	size_t sz = getTotalSizeBytes();
	aitUint8* buf = new aitUint8[sz];
	gddAtomic* pdd = (gddAtomic*)buf;
	flattenWithAddress(buf,sz);
	fprintf(stderr,"----TESTING FLATTENWITHADDRESS():\n");
	pdd->dump();
	fprintf(stderr,"----CONVERTADDRESSTOOFFSETS() THEN BACK AND DUMP:\n");
	pdd->convertAddressToOffsets();
	pdd->convertOffsetsToAddress();
	pdd->dump();
	pdd->unreference();
	delete buf;
}
#endif

#ifndef NO_DUMP_TEST
class gddAtomicDestr : public gddDestructor
{
public:
	gddAtomicDestr(void) { }
	void run(void*);
};

void gddAtomicDestr::run(void* v)
{
	fprintf(stderr,"**** gddAtomicDestr::run from gddAtomic::test %8.8x\n",v);
}
#endif

#ifdef NO_DUMP_TEST
void gddAtomic::test(void) { }
#else
void gddAtomic::test(void)
{
	aitFloat32 f32[6] = { 32.0,2.0,1.0, 7.0,8.0,9.0 };
	aitFloat64 f64[6] = { 64.0,5.0,4.0, 10.0,11.0,12.0 };
	aitInt8 i8[6] = { -8,2,1, 13,14,15 };
	aitInt16 i16[6] = { -16,3,2, 16,17,18 };
	aitInt32 i32[6] = { -32,4,3, 19,20,21 };
	aitUint8 ui8[6] = { 8,5,4, 22,23,24 };
	aitUint16 ui16[6] = { 16,6,5, 25,26,27 };
	aitUint32 ui32[6] = { 32,7,6, 28,29,30 };
	aitIndex bnds[2] = { 2,3 };

	// Rules: 
	// reset() clear out everything and set data pointer to NULL
	// put() will auto allocate if data pointer is NULL, otherwise copy
	// putRef() will clear current data pointer and set new one

	// if the data pointer is NULL when put() is called, then a 
	// generic gddDestructor will be created to delete the buffer
	
	reset(aitEnumFloat32,2,bnds);
	put(f32); dump();
	putRef(f32,new gddAtomicDestr); dump();

	reset(aitEnumFloat64,2,bnds);
	put(f64); dump();
	putRef(f64,new gddAtomicDestr); dump();

	reset(aitEnumInt8,2,bnds);
	put(i8); dump();
	putRef(i8,new gddAtomicDestr); dump();

	reset(aitEnumUint8,2,bnds);
	put(ui8); dump();
	putRef(ui8,new gddAtomicDestr); dump();

	reset(aitEnumInt16,2,bnds);
	put(i16); dump();
	putRef(i16,new gddAtomicDestr); dump();

	reset(aitEnumUint16,2,bnds);
	put(ui16); dump();
	putRef(ui16,new gddAtomicDestr); dump();

	reset(aitEnumInt32,2,bnds);
	put(i32); dump();
	putRef(i32,new gddAtomicDestr); dump();

	reset(aitEnumUint32,2,bnds);
	put(ui32); dump();
	putRef(ui32,new gddAtomicDestr); dump();
}
#endif

#ifdef NO_DUMP_TEST
void gddScaler::test(void) { }
#else
void gddScaler::test(void)
{
	int i;
	aitFloat32 fa32,f32 = 32.0;
	aitFloat64 fa64,f64 = 64.0;
	aitInt8 ia8,i8 = -8;
	aitInt16 ia16,i16 = -16;
	aitInt32 ia32,i32 = -32;
	aitUint8 uia8,ui8 = 8;
	aitUint16 uia16,ui16 = 16;
	aitUint32 uia32,ui32 = 32;

	// printf("get float32 = %f\n",fa32);
	// printf("op= float32 = %f\n",fa32);

	fprintf(stderr,"float32====");
	for(i=0;i<aitConvertTotal;i++)
	{
		setPrimType((aitEnum)i);
		put(f32);
		get(fa32);
		dump();
		*this=f32;
		fa32=*this;
		dump();
	}
	fprintf(stderr,"float64====");
	for(i=0;i<aitConvertTotal;i++)
	{
		setPrimType((aitEnum)i);
		put(f64); get(fa64); dump(); *this=f64; fa64=*this; dump();
	}
	fprintf(stderr,"int8====");
	for(i=0;i<aitConvertTotal;i++)
	{
		setPrimType((aitEnum)i);
		put(i8); get(ia8); dump(); *this=i8; ia8=*this; dump();
	}
	fprintf(stderr,"uint8====");
	for(i=0;i<aitConvertTotal;i++)
	{
		setPrimType((aitEnum)i);
		put(ui8); get(uia8); dump(); *this=ui8; uia8=*this; dump();
	}
	fprintf(stderr,"int16====");
	for(i=0;i<aitConvertTotal;i++)
	{
		setPrimType((aitEnum)i);
		put(i16); get(ia16); dump(); *this=i16; ia16=*this; dump();
	}
	fprintf(stderr,"uint16====");
	for(i=0;i<aitConvertTotal;i++)
	{
		setPrimType((aitEnum)i);
		put(ui16); get(uia16);	 dump(); *this=ui16;	uia16=*this; dump();
	}
	fprintf(stderr,"int32====");
	for(i=0;i<aitConvertTotal;i++)
	{
		setPrimType((aitEnum)i);
		put(i32); get(ia32); dump(); *this=i32;	ia32=*this; dump();
	}
	fprintf(stderr,"uint32====");
	for(i=0;i<aitConvertTotal;i++)
	{
		setPrimType((aitEnum)i);
		put(ui32); get(uia32); dump(); *this=ui32; uia32=*this; dump();
	}
}
#endif

#ifdef NO_DUMP_TEST
void gddContainer::dump(void) { }
#else
void gddContainer::dump(void)
{
	int i;
	gdd* dd;
	gddAtomic* add;
	gddScaler* sdd;
	gddContainer* cdd;

	fprintf(stderr,"----------dumping container:\n");
	gdd::dumpInfo();
	fprintf(stderr," total in container = %d\n",total());

	// should use a cursor

	for(i=0;dd=getDD(i);i++)
	{
		if(dd->isAtomic())		{ add=(gddAtomic*)dd; add->dump(); }
		if(dd->isScaler())		{ sdd=(gddScaler*)dd; sdd->dump(); }
		if(dd->isContainer())	{ cdd=(gddContainer*)dd; cdd->dump(); }
	}
}
#endif

#ifdef NO_DUMP_TEST
void gddContainer::test(void) { }
#else
void gddContainer::test(void)
{
	gddScaler* sdd1 = new gddScaler(1,aitEnumInt32);
	gddScaler* sdd2 = new gddScaler(2,aitEnumInt16);
	gddAtomic* add1 = new gddAtomic(3,aitEnumFloat32,1,3);
	gddContainer* cdd1;

	aitInt16 i16 = 5;
	aitInt32 i32 = 6;
	aitFloat32 f32[3] = { 7.0, 8.0, 9.0 };
	aitUint8* buf;
	size_t sz;

	*sdd1=i32; *sdd2=i16; *add1=f32;

	// insert two scalers and an atomic into the container
	fprintf(stderr,"*INSERT %8.8x %8.8x %8.8x\n",sdd1,sdd2,add1);
	clear();
	sdd1->reference(); add1->reference(); sdd2->reference();
	insert(sdd1); insert(sdd2); insert(add1); dump();

	fprintf(stderr,"=====TESTING CURSOR:\n");
	gddCursor cur = getCursor();
	gdd* dd;
	int i;
	for(i=0;dd=cur[i];i++) fprintf(stderr,"%8.8x ",dd);
	fprintf(stderr,"\n");
	for(dd=cur.first();dd;dd=cur.next()) fprintf(stderr,"%8.8x ",dd);
	fprintf(stderr,"\n");

	remove(0); remove(0); remove(0); dump();
	sdd1->reference(); add1->reference(); sdd2->reference();
	insert(add1); insert(sdd1); insert(sdd2); dump();

	sz = getTotalSizeBytes();
	buf = new aitUint8[sz];

	fprintf(stderr,"=====TESTING FLATTEN FUNCTION BUFFER=%8.8x:\n",buf);
	flattenWithAddress(buf,sz);
	cdd1=(gddContainer*)buf;
	cdd1->dump();
	fprintf(stderr,"=====CHANGE ADDRESSES TO OFFSETS:\n");
	cdd1->convertAddressToOffsets();
	fprintf(stderr,"=====CHANGE OFFSETS TO ADDRESSES:\n");
	cdd1->convertOffsetsToAddress();
	fprintf(stderr,"=====RE-DUMP OF FLATTENED CONTAINER:\n");
	cdd1->dump();
	fprintf(stderr,"=====RE-DUMP OF ORIGINAL CONTAINER:\n");
	dump();
	cdd1->unreference();
	delete buf;

	// test copy(), Dup(), copyInfo()
	fprintf(stderr,"=======CREATING TEST CONTAINER FOR *COPY* TEST:\n");
	cdd1 = new gddContainer;
	fprintf(stderr,"=======COPYINFO():\n");
	cdd1->copyInfo(this); cdd1->dump();
	fprintf(stderr,"=======DUP():\n");
	cdd1->Dup(this); cdd1->dump();
	fprintf(stderr,"=======COPY():\n");
	cdd1->copy(this); cdd1->dump();
	fprintf(stderr,"=======UNREFERENCE THE TEST CONTAINER:\n");
	cdd1->unreference();

	fprintf(stderr,"=====DUMPING ORIGINAL:\n");
	dump();
	clear();

	fprintf(stderr,"=======TEST COMPLETE, DELETE STUFF:\n");
	fprintf(stderr," first scaler:\n "); sdd1->unreference();
	fprintf(stderr," first atomic:\n "); add1->unreference();
	fprintf(stderr," second scaler:\n "); sdd2->unreference();
	dump();
}
#endif

