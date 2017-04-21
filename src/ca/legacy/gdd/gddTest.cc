/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
// Author: Jim Kowalkowski
// Date: 2/96

#include <stdio.h>
#define epicsExportSharedSymbols
#include "gdd.h"

// -----------------------test routines------------------------

void gdd::dump(void) const
{
	gddScalar* sdd;
	gddAtomic* add;
	gddContainer* cdd;

	if(isScalar())
	{
		sdd=(gddScalar*)this;
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

void gdd::dumpInfo(void) const
{
	unsigned i;
	aitIndex f,c;
	unsigned long sz_tot,sz_data,sz_elem;
	const aitIndex max=20u;
	aitIndex prt_tot;

	sz_tot = getTotalSizeBytes();
	sz_data = getDataSizeBytes();
	sz_elem = getDataSizeElements();

	prt_tot=sz_elem>max?max:sz_elem;

	fprintf(stderr,"----------dump This=%p---------\n", this);
	fprintf(stderr," dimension=%u ", dimension());
	fprintf(stderr,"app-type=%u ", applicationType());

	if(isScalar()) fprintf(stderr,"Scalar\n");
	if(isAtomic()) fprintf(stderr,"Atomic\n");
	if(isContainer()) fprintf(stderr,"Container\n");

	fprintf(stderr," prim-type=%s",aitName[primitiveType()]);
	switch(primitiveType())
	{
	case aitEnumInvalid:
		fprintf(stderr,"(aitEnumInvalid)");
		break;
	case aitEnumInt8:
		fprintf(stderr,"(aitEnumInt8)");
		if(isScalar()) fprintf(stderr," value=0x%2.2x ",data.Int8);
		if(isAtomic()&&dataPointer())
		{
			fprintf(stderr,"\n %d values=<\n",(int)prt_tot);
			aitInt8* i8=(aitInt8*)dataPointer();
			for(i=0;i<prt_tot;i++) fprintf(stderr,"0x%2.2x ",i8[i]);
			fprintf(stderr,">\n");
		}
		break;
	case aitEnumInt16:
		fprintf(stderr,"(aitEnumInt16)");
		if(isScalar()) fprintf(stderr," value=%hd ",data.Int16);
		if(isAtomic()&&dataPointer())
		{
			fprintf(stderr,"\n %d values=<\n",(int)prt_tot);
			aitInt16* i16=(aitInt16*)dataPointer();
			for(i=0;i<prt_tot;i++) fprintf(stderr,"%hd ",i16[i]);
			fprintf(stderr,">\n");
		}
		break;
	case aitEnumInt32:
		fprintf(stderr,"(aitEnumInt32)");
		if(isScalar()) fprintf(stderr," value=%d ",data.Int32);
		if(isAtomic()&&dataPointer())
		{
			fprintf(stderr,"\n %d values=<\n",(int)prt_tot);
			aitInt32* i32=(aitInt32*)dataPointer();
			for(i=0;i<prt_tot;i++) fprintf(stderr,"%d ",i32[i]);
			fprintf(stderr,">\n");
		}
		break;
	case aitEnumUint8:
		fprintf(stderr,"(aitEnumUint8)");
		if(isScalar()) fprintf(stderr," value=0x%2.2x ",data.Uint8);
		if(isAtomic()&&dataPointer())
		{
			fprintf(stderr,"\n %d values=<\n",(int)prt_tot);
			aitUint8* ui8=(aitUint8*)dataPointer();
			for(i=0;i<prt_tot;i++) fprintf(stderr,"0x%2.2x ",ui8[i]);
			fprintf(stderr,">\n");
		}
		break;
	case aitEnumUint16:
		fprintf(stderr,"(aitEnumUint16)");
		if(isScalar()) fprintf(stderr," value=%hu ",data.Uint16);
		if(isAtomic()&&dataPointer())
		{
			fprintf(stderr,"\n %d values=<\n",(int)prt_tot);
			aitUint16* ui16=(aitUint16*)dataPointer();
			for(i=0;i<prt_tot;i++) fprintf(stderr,"%hu ",ui16[i]);
			fprintf(stderr,">\n");
		}
		break;
	case aitEnumEnum16:
		fprintf(stderr,"(aitEnumEnum16)");
		if(isScalar()) fprintf(stderr," value=%hu ",data.Enum16);
		if(isAtomic()&&dataPointer())
		{
			fprintf(stderr,"\n %d values=<\n",(int)prt_tot);
			aitEnum16* e16=(aitEnum16*)dataPointer();
			for(i=0;i<prt_tot;i++) fprintf(stderr,"%hu ",e16[i]);
			fprintf(stderr,">\n");
		}
		break;
	case aitEnumUint32:
		fprintf(stderr,"(aitEnumUint32)"); 
		if(isScalar()) fprintf(stderr," value=%u ",data.Uint32);
		if(isAtomic()&&dataPointer())
		{
			fprintf(stderr,"\n %d values=<\n",(int)prt_tot);
			aitUint32* ui32=(aitUint32*)dataPointer();
			for(i=0;i<prt_tot;i++) fprintf(stderr,"%u ",ui32[i]);
			fprintf(stderr,">\n");
		}
		break;
	case aitEnumFloat32:
		fprintf(stderr,"(aitEnumFloat32)"); 
		if(isScalar()) fprintf(stderr," value=%f ",data.Float32);
		if(isAtomic()&&dataPointer())
		{
			fprintf(stderr,"\n %d values=<\n",(int)prt_tot);
			aitFloat32* f32=(aitFloat32*)dataPointer();
			for(i=0;i<prt_tot;i++) fprintf(stderr,"%f ",f32[i]);
			fprintf(stderr,">\n");
		}
		break;
	case aitEnumFloat64:
		fprintf(stderr,"(aitEnumFloat64)"); 
		if(isScalar()) fprintf(stderr," value=%f ",data.Float64);
		if(isAtomic()&&dataPointer())
		{
			fprintf(stderr,"\n %d values=<\n",(int)prt_tot);
			aitFloat64* f64=(aitFloat64*)dataPointer();
			for(i=0;i<prt_tot;i++) fprintf(stderr,"%f ",f64[i]);
			fprintf(stderr,">\n");
		}
		break;
	case aitEnumFixedString:
		fprintf(stderr,"(aitEnumFixedString)"); 
		if(isScalar())
		{
			if(data.FString)
				fprintf(stderr," value=<%s>\n",(char *)data.FString);
			else
				fprintf(stderr," value=<NULL>\n");
		}
		if(isAtomic()&&dataPointer())
		{
			fprintf(stderr,"\n %d values=<\n",(int)prt_tot);
			aitFixedString* fs=(aitFixedString*)dataPointer();
			for(i=0;i<prt_tot;i++) fprintf(stderr,"<%s> ",fs[i].fixed_string);
			fprintf(stderr,">\n");
		}
		break;
	case aitEnumString:
		fprintf(stderr,"(aitEnumString)"); 
		if(isScalar())
		{
			aitString* str = (aitString*)dataAddress();
			fprintf(stderr,"\n");
			str->dump();
		}
		if(isAtomic()&&dataPointer())
		{
			fprintf(stderr,"\n %d values=<\n",(int)prt_tot);
			aitString* ss=(aitString*)dataPointer();
			for(i=0;i<prt_tot;i++)
				if(ss[i].string()) fprintf(stderr,"<%s> ",ss[i].string());
			fprintf(stderr,">\n");
		}
		break;
	case aitEnumContainer:
		fprintf(stderr,"(aitEnumContainer)"); 
		break;
	default: break;
	}

	fprintf(stderr," ref-count=%d\n",ref_cnt);
	fprintf(stderr," total-bytes=%ld,",sz_tot);
	fprintf(stderr," data-size=%ld,",sz_data);
	fprintf(stderr," element-count=%ld\n",sz_elem);

	if(!isScalar())
	{
		if(destruct)
			fprintf(stderr," destructor=%p\n", destruct);
		else
			fprintf(stderr," destructor=NULL\n");
	}

	for(i=0;i<dimension();i++)
	{
		getBound(i,f,c);
		fprintf(stderr," (%d) %p first=%d count=%d\n",i,&bounds[i],f,c);
	}

	if(isManaged())				fprintf(stderr," Managed");
	if(isFlat())				fprintf(stderr," Flat");
	if(isLocalDataFormat())		fprintf(stderr," LocalDataFormat");
	if(isNetworkDataFormat())	fprintf(stderr," NetworkDataFormat");
	if(isConstant())			fprintf(stderr," Constant");
	if(isNoRef())				fprintf(stderr," NoReferencing");
	fprintf(stderr,"\n");

	if(!isContainer() && !isScalar() && !isAtomic())
		 fprintf(stderr,"--------------------------------------\n");
}

void gddScalar::dump(void) const
{
	gdd::dumpInfo();
	fprintf(stderr,"--------------------------------------\n");
}

void gddAtomic::dump(void) const
{
	gdd::dumpInfo();
	fprintf(stderr,"-------------------------------------\n");
}

void gddContainer::dump(void) const
{
	const gdd* dd;
	const gddAtomic* add;
	const gddScalar* sdd;
	const gddContainer* cdd;

	fprintf(stderr,"----------dumping container:\n");
	gdd::dumpInfo();
	fprintf(stderr," total in container = %d\n",total());

	constGddCursor cur = this->getCursor();
	for(dd=cur.first();dd;dd=cur.next())
	{
		if(dd->isAtomic())		{ add=(gddAtomic*)dd; add->dump(); }
		if(dd->isScalar())		{ sdd=(gddScalar*)dd; sdd->dump(); }
		if(dd->isContainer())	{ cdd=(gddContainer*)dd; cdd->dump(); }
	}
}

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
	delete [] buf;
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
	fprintf(stderr,"**** gddAtomicDestr::run from gddAtomic::test %p\n",v);
}
#endif

#ifdef NO_DUMP_TEST
void gddAtomic::test(void) { }
#else
void gddAtomic::test(void)
{
	aitFloat32 f32[6] = { 32.0f,2.0f,1.0f, 7.0f,8.0f,9.0f };
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
void gddScalar::test(void) { }
#else
void gddScalar::test(void)
{
	int i;
	aitFloat32 fa32,f32 = 32.0f;
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
void gddContainer::test(void) { }
#else
void gddContainer::test(void)
{
	gddScalar* sdd1 = new gddScalar(1,aitEnumInt32);
	gddScalar* sdd2 = new gddScalar(2,aitEnumInt16);
	gddAtomic* add1 = new gddAtomic(3,aitEnumFloat32,1,3);
	gddContainer* cdd1;

	aitInt16 i16 = 5;
	aitInt32 i32 = 6;
	aitFloat32 f32[3] = { 7.0f, 8.0f, 9.0f };
	aitUint8* buf;
	size_t sz;

	*sdd1=i32; *sdd2=i16; *add1=f32;

	// insert two scalers and an atomic into the container
	fprintf(stderr,"*INSERT %p %p %p\n",sdd1,sdd2,add1);
	clear();
	sdd1->reference(); add1->reference(); sdd2->reference();
	insert(sdd1); insert(sdd2); insert(add1); dump();

	fprintf(stderr,"=====TESTING CURSOR:\n");
	gddCursor cur = getCursor();
	gdd* dd;
	int i;
	for(i=0; (dd=cur[i]); i++) fprintf(stderr,"%p ",dd);
	fprintf(stderr,"\n");
	for(dd=cur.first();dd;dd=cur.next()) fprintf(stderr,"%p ",dd);
	fprintf(stderr,"\n");

	remove(0); remove(0); remove(0); dump();
	sdd1->reference(); add1->reference(); sdd2->reference();
	insert(add1); insert(sdd1); insert(sdd2); dump();

	sz = getTotalSizeBytes();
	buf = new aitUint8[sz];

	fprintf(stderr,"=====TESTING FLATTEN FUNCTION BUFFER=%p:\n",buf);
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
	delete [] buf;

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

