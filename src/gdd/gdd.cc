// Author: Jim Kowalkowski
// Date: 2/96
// 
// $Id$
// 
// $Log$
// Revision 1.20  1997/03/21 01:56:01  jbk
// *** empty log message ***
//
// Revision 1.19  1997/03/17 17:14:46  jbk
// fixed a problem with gddDestructor and reference counting
//
// Revision 1.18  1997/01/21 15:13:06  jbk
// free up resources in clearData
//
// Revision 1.17  1997/01/12 20:32:45  jbk
// many errors fixed
//
// Revision 1.15  1996/12/17 15:04:42  jbk
// fixed bug in copyData, sets bounds now
//
// Revision 1.14  1996/12/06 22:27:44  jhill
// fixed wrong start bug in getDD()
//
// Revision 1.13  1996/11/02 01:24:46  jhill
// strcpy => styrcpy (shuts up purify)
//
// Revision 1.12  1996/10/29 15:39:58  jbk
// Much new doc added.  Fixed bug in gdd.cc.
//
// Revision 1.11  1996/08/27 13:05:05  jbk
// final repairs to string functions, put() functions, and error code printing
//
// Revision 1.10  1996/08/23 20:29:36  jbk
// completed fixes for the aitString and fixed string management
//
// Revision 1.9  1996/08/22 21:05:40  jbk
// More fixes to make strings and fixed string work better.
//
// Revision 1.8  1996/08/14 12:30:14  jbk
// fixes for converting aitString to aitInt8* and back
// fixes for managing the units field for the dbr types
//
// Revision 1.7  1996/08/13 15:07:46  jbk
// changes for better string manipulation and fixes for the units field
//
// Revision 1.6  1996/08/06 19:14:11  jbk
// Fixes to the string class.
// Changes units field to a aitString instead of aitInt8.
//
// Revision 1.5  1996/07/26 02:23:16  jbk
// Fixed the spelling error with Scalar.
//
// Revision 1.4  1996/07/23 17:13:31  jbk
// various fixes - dbmapper incorrectly worked with enum types
//
// Revision 1.3  1996/06/26 21:00:07  jbk
// Fixed up code in aitHelpers, removed unused variables in others
// Fixed potential problem in gddAppTable.cc with the map functions
//
// Revision 1.2  1996/06/26 02:42:06  jbk
// more correction to the aitString processing - testing menus
//
// Revision 1.1  1996/06/25 19:11:37  jbk
// new in EPICS base
//
//

// *Revision 1.4  1996/06/25 18:59:01  jbk
// *more fixes for the aitString management functions and mapping menus
// *Revision 1.3  1996/06/24 03:15:32  jbk
// *name changes and fixes for aitString and fixed string functions
// *Revision 1.2  1996/06/13 21:31:55  jbk
// *Various fixes and correction - including ref_cnt change to unsigned short
// *Revision 1.1  1996/05/31 13:15:24  jbk
// *add new stuff

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define epicsExportSharedSymbols
#include "gdd.h"

gdd_NEWDEL_NEW(gdd)
gdd_NEWDEL_DEL(gdd)
gdd_NEWDEL_STAT(gdd)

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

void gddFlattenDestructor::run(void*)
{
	return;
}

void gddContainerCleaner::run(void* v)
{
	gddContainer* cdd = (gddContainer*)v;
	int tot = cdd->total();
	int i;
	for(i=0;i<tot;i++) cdd->remove(0);
}

// --------------------------The gdd functions-------------------------

gdd::gdd(int app, aitEnum prim, int dimen)
{
	init(app,prim,dimen);
}

gdd::gdd(int app, aitEnum prim, int dimen, aitUint32* val)
{
	int i;
	init(app,prim,dimen);
	for(i=0;i<dimen;i++) bounds[i].set(0,val[i]);
}

void gdd::init(int app, aitEnum prim, int dimen)
{
	setApplType(app);
	setPrimType(prim);
	dim=(aitUint8)dimen;
	destruct=NULL;
	ref_cnt=1;
	flags=0;
	bounds=NULL;
	setData(NULL);
	setStatSevr(0u,0u);

	if(dim)
	{
		switch(dim)
		{
		case 1:  bounds=(gddBounds*)new gddBounds1D; bounds->set(0,0); break;
		case 2:  bounds=(gddBounds*)new gddBounds2D; break;
		case 3:  bounds=(gddBounds*)new gddBounds3D; break;
		default: bounds=(gddBounds*)new gddBounds[dim]; break;
		}
	}
	else if(primitiveType()==aitEnumString)
	{
		aitString* str=(aitString*)dataAddress();
		str->init();
	}
}

gdd::gdd(gdd* dd)
{
	ref_cnt=1;
	copyInfo(dd);
}

gdd::~gdd(void)
{
	gdd* dd;
	gdd* temp;

	// fprintf(stderr,"A gdd is really being deleted %8.8x!!\n",this);

	// this function need to be corrected for use of aitEnumString!

	if(isScalar())
	{
		if(primitiveType()==aitEnumFixedString)
		{
			// aitString type could have destructors
			if(destruct)
				destruct->destroy(dataPointer());
			else
				if(data.FString) delete [] data.FString;
		}
		else if(primitiveType()==aitEnumString)
		{
			// aitString type could have destructors
			if(destruct)
				destruct->destroy(dataAddress());
			else
			{
				aitString* s = (aitString*)dataAddress();
				s->clear();
			}
		}
	}
	else if(isContainer())
	{
		if(destruct)
			destruct->destroy(dataPointer());
		else
		{
			for(dd=(gdd*)dataPointer();dd;)
			{
				temp=dd;
				dd=dd->next();
				temp->unreference();
			}
			freeBounds();
		}
	}
	else
	{
		if(destruct) destruct->destroy(dataPointer());
		if(bounds) freeBounds();
	}
	setPrimType(aitEnumInvalid);
	setApplType(0);
}

void gdd::freeBounds(void)
{
	if(bounds)
	{
		switch(dim)
		{
		case 0: break;
		case 1: { gddBounds1D* d1=(gddBounds1D*)bounds; delete d1; } break;
		case 2: { gddBounds2D* d2=(gddBounds2D*)bounds; delete d2; } break;
		case 3: { gddBounds3D* d3=(gddBounds3D*)bounds; delete d3; } break;
		default: delete bounds; break;
		}
		bounds=NULL;
	}
	dim=0;	// Reflect the fact that this now a scalar? Bounds and dim
			// should be tightly coupled, so no bounds means no dimension.
}

void gdd::setDimension(int d, const gddBounds* bnds)
{
	int i;

	if(dim!=d)
	{
		freeBounds();
		dim=(aitUint8)d;
		switch(dim)
		{
		case 0:  break;
		case 1:  bounds=(gddBounds*)new gddBounds1D; bounds->set(0,0); break;
		case 2:  bounds=(gddBounds*)new gddBounds2D; break;
		case 3:  bounds=(gddBounds*)new gddBounds3D; break;
		default: bounds=(gddBounds*)new gddBounds[dim]; break;
		}
	}
	if(bnds)
	{
		for(i=0;i<dim;i++)
			bounds[i]=bnds[i];
	}
}

gddStatus gdd::registerDestructor(gddDestructor* dest)
{
	// this is funky, will not register a destructor if one is present
	if(destruct)
	{
		gddAutoPrint("gdd::registerDestructor()",gddErrorAlreadyDefined);
		return gddErrorAlreadyDefined;
	}
	else
		return replaceDestructor(dest);
}

gddStatus gdd::replaceDestructor(gddDestructor* dest)
{
	destruct=dest;
	destruct->reference();

	if(isContainer()||isFlat())
		markManaged();

	return 0;
}

gddStatus gdd::genCopy(aitEnum t, const void* d, aitDataFormat f)
{
	size_t sz;
	aitInt8* buf;
	gddStatus rc=0;

	if(isScalar())
		set(t,d,f);
	else if(isAtomic())
	{
		if(!dataPointer())
		{
			sz=describedDataSizeBytes();
			if((buf=new aitInt8[sz])==NULL)
			{
				gddAutoPrint("gdd::genCopy()",gddErrorNewFailed);
				rc=gddErrorNewFailed;
			}
			else
			{
				setData(buf);
				destruct=new gddDestructor;
				destruct->reference();
			}
		}
		if(rc==0)
		{
			if(f==aitLocalDataFormat)
				aitConvert(primitiveType(),dataPointer(),t,d,
					getDataSizeElements());
			else
				aitConvertFromNet(primitiveType(),dataPointer(),t,d,
					getDataSizeElements());

			markLocalDataFormat();
		}
	}
	else
	{
		gddAutoPrint("gdd::genCopy()",gddErrorTypeMismatch);
		rc=gddErrorTypeMismatch;
	}

	return rc;
}


gddStatus gdd::changeType(int app,aitEnum prim)
{
	gddStatus rc=0;

	// this should only be allowed for setting the type if it is
	// undefined or if the data is a scaler

	if(isScalar() || primitiveType()==aitEnumInvalid)
	{
		setApplType(app);
		setPrimType(prim);
	}
	else
	{
		gddAutoPrint("gdd::changeType()",gddErrorTypeMismatch);
		rc=gddErrorTypeMismatch;
	}

	return rc;
}

gddStatus gdd::setBound(unsigned index_dim, aitIndex first, aitIndex count)
{
	gddStatus rc=0;
	if(index_dim<dimension())
		bounds[index_dim].set(first,count);
	else
	{
		gddAutoPrint("gdd::setBound()",gddErrorOutOfBounds);
		rc=gddErrorOutOfBounds;
	}
	return rc;
}

gddStatus gdd::getBound(unsigned index_dim, aitIndex& first, aitIndex& count)
{
	gddStatus rc=0;
	if(index_dim<dimension())
		bounds[index_dim].get(first,count);
	else
	{
		gddAutoPrint("gdd::getBound()",gddErrorOutOfBounds);
		rc=gddErrorOutOfBounds;
	}
	return rc;
}

// should the copy functions in gdd use the flatten technique?
gddStatus gdd::copyStuff(gdd* dd,int ctype)
{
	unsigned i;
	gddStatus rc=0;
	gddContainer* cdd;
	gdd *pdd,*ndd;

	// blow me out quickly here
	if(isFlat()||isManaged())
	{
		gddAutoPrint("gdd::copyStuff()",gddErrorNotAllowed);
		return gddErrorNotAllowed;
	}

	clear();

	if(dd->isContainer())
	{
		changeType(dd->applicationType(),dd->primitiveType());
		cdd=(gddContainer*)dd;
		gddCursor cur=cdd->getCursor();

		for(ndd=cur.first();ndd;ndd=cur.next())
		{
			pdd=new gdd(ndd->applicationType(),
				ndd->primitiveType(),ndd->dimension());
			pdd->setNext((gdd*)dataPointer());
			setData(pdd);
			bounds->setSize(bounds->size()+1);
			pdd->copyStuff(ndd,ctype);
		}
	}
	else
	{
		init(dd->applicationType(),dd->primitiveType(),dd->dimension());

		if(dd->isScalar())
			data=dd->data;
		else // atomic
		{
			const gddBounds* bnds = dd->getBounds();
			for(i=0;i<dd->dimension();i++) bounds[i]=bnds[i];
	
			switch(ctype)
			{
			case 1: // copy()
				aitUint8* array;
				size_t a_size;
				a_size=dd->getDataSizeBytes();
				if(array=new aitUint8[a_size])
				{
					destruct=new gddDestructor;
					destruct->reference();
					memcpy(array,dd->dataPointer(),a_size);
					setData(array);
				}
				else
				{
					gddAutoPrint("gdd::copyStuff()",gddErrorNewFailed);
					rc=gddErrorNewFailed;
				}
				break;
			case 2: // Dup()
				data=dd->getData(); // copy the data reference
				destruct=dd->destruct;
				if(destruct) destruct->reference();
				break;
			case 0: // copyInfo()
			default: break;
			}
		}
	}
	return rc;
}

size_t gdd::getDataSizeBytes(void) const
{
	size_t sz=0;
	gdd* pdd;
	aitString* str;

	if(isContainer())
	{
		const gddContainer* cdd=(const gddContainer*)this;
		gddCursor cur=cdd->getCursor();
		for(pdd=cur.first();pdd;pdd=cur.next())
			sz+=pdd->getTotalSizeBytes();
	}
	else
	{
		if(aitValid(primitiveType()))
		{
			if(primitiveType()==aitEnumString)
			{
				if(dimension()) str=(aitString*)dataPointer();
				else str=(aitString*)dataAddress();
				sz+=(size_t)(aitString::totalLength(str,
					getDataSizeElements()));
			}
			else
				sz+=(size_t)(getDataSizeElements())*aitSize[primitiveType()];
		}
	}
	return sz;
}

size_t gdd::describedDataSizeBytes(void) const
{
	size_t sz=0;

	// does not work well for aitString - only reports the aitString info

	if(!isContainer())
		sz+=(size_t)(describedDataSizeElements())*aitSize[primitiveType()];

	return sz;
}

size_t gdd::getTotalSizeBytes(void) const
{
	size_t sz;
	unsigned long tsize;
	gdd* pdd;

	// add up size of bounds + size of this DD
	sz=sizeof(gdd)+(sizeof(gddBounds)*dimension());

	// special case the aitString/aitFixedString here - sucks bad
	if(isScalar())
	{
		if(primitiveType()==aitEnumString)
		{
			aitString* str=(aitString*)dataAddress();
			sz+=str->length()+1; // include the NULL
		}
		else if(primitiveType()==aitEnumFixedString)
			sz+=sizeof(aitFixedString);
	}
	else if(isAtomic())
	{
		if(aitValid(primitiveType()))
		{
			if(primitiveType()==aitEnumString)
			{
				// special case the aitString here
				tsize=(size_t)(aitString::totalLength((aitString*)dataPointer(),
					getDataSizeElements()));
			}
			else
				tsize=(size_t)(getDataSizeElements())*aitSize[primitiveType()];

			sz+=(size_t)align8(tsize); // include alignment
		}
	}
	else if(isContainer())
	{
		const gddContainer* cdd=(const gddContainer*)this;
		gddCursor cur=cdd->getCursor();
		for(pdd=cur.first();pdd;pdd=cur.next())
			sz+=pdd->getTotalSizeBytes();
	}
	return sz;
}

aitUint32 gdd::getDataSizeElements(void) const
{
	unsigned long total=0;
	unsigned i;

	if(dimension()==0)
		total=1;
	else
	{
		if(dataPointer())
			for(i=0;i<dimension();i++) total+=bounds[i].size();
	}

	return total;
}

aitUint32 gdd::describedDataSizeElements(void) const
{
	unsigned long total=0;
	unsigned i;

	if(dimension()==0)
		total=1;
	else
		for(i=0;i<dimension();i++) total+=bounds[i].size();

	return total;
}

size_t gdd::flattenWithOffsets(void* buf, size_t size, aitIndex* total_dd)
{
	gdd* flat_dd;
	size_t sz;
	
	sz = flattenWithAddress(buf,size,total_dd);
	flat_dd=(gdd*)buf;
	if(sz>0) flat_dd->convertAddressToOffsets();
	return sz;
}

// IMPORTANT NOTE:
// Currently the user cannot register an empty container as a prototype.
// The destructor will not be installed to clean up the container when
// it is freed.

// This is an important function
// Data should be flattened as follows:
// 1) all the GDDs, seen as an array an GDDs
// 2) all the bounds info for all the GDDs
// 3) all the data for all GDDs
//
// In addition, the user should be able to flatten GDDs without the data
// and flatten portions or all the data without flattening GDDs

size_t gdd::flattenWithAddress(void* buf, size_t size, aitIndex* total_dd)
{
	gdd* pdd = (gdd*)buf;
	size_t pos,sz,spos;
	aitUint32 i;
	gddBounds* bnds;

	// copy this gdd (first one) to get things started
	// this is done in two passes - one to copy DDs, one for bounds/data
	// need to be able to check if the DD has been flattened already

	if((sz=getTotalSizeBytes())>size) return 0;
	pdd[0]=*this;
	pdd[0].destruct=NULL;
	pdd[0].flags=0;
	pos=1;

	// not enough to just copy the gdd info if the primitive type is
	// aitString or aitFixedString (even if scaler gdd)
	// must special case the strings - that really sucks

	if(isScalar())
	{
		// here is special case for the string types
		if(primitiveType()==aitEnumFixedString)
		{
			if(data.FString)
				memcpy((char*)&pdd[pos],data.FString,sizeof(aitFixedString));

			pdd[0].data.FString=(aitFixedString*)&pdd[pos];
		}
		else if(primitiveType()==aitEnumString)
		{
			aitString* str=(aitString*)pdd[0].dataAddress();
			if(str->string())
			{
				memcpy((char*)&pdd[pos],str->string(),str->length()+1);
				str->force((char*)&pdd[pos]);
				str->forceConstant();
			}
			else
				str->init();
		}
	}
	else if(isContainer())
	{
		// need to check for bounds in the container and flatten them
		if(dataPointer())
		{
			// process all the container's DDs
			spos=pos;
			pos+=flattenDDs((gddContainer*)this,&pdd[pos],
				size-(pos*sizeof(gdd)));

			// copy all the data from the entire container into the buffer
			flattenData(&pdd[0],pos,&pdd[pos],size-(pos*sizeof(gdd)));

			pdd[0].markFlat();
			pdd[0].setData(&pdd[spos]);
		}
		else
			sz=0; // failure should occur - cannot flatten an empty container
	}
	else if(isAtomic())
	{
		// Just copy the data from this DD into the buffer, copy bounds
		if(bounds)
		{
			pdd[0].markFlat();
			bnds=(gddBounds*)(&pdd[pos]);
			for(i=0;i<dimension();i++) bnds[i]=bounds[i];
			pdd[0].bounds=bnds;
			if(dataPointer())
			{
				if(primitiveType()==aitEnumString)
				{
					// not very good way to do it, size info bad
					aitString* str = (aitString*)dataPointer();
					aitString::compact(str,getDataSizeElements(),&bnds[i],size);
				}
				else
					memcpy(&bnds[i],dataPointer(),getDataSizeBytes());

				pdd[0].setData(&bnds[i]);
			}
			else
				sz=0; // should return a failure
		}
		else
			sz=0; // should return failure
	}
	if(total_dd) *total_dd=pos;
	return sz;
}

gddStatus gdd::flattenData(gdd* dd, int tot_dds, void* buf,size_t size)
{
	int i;
	unsigned j;
	size_t sz;
	gddBounds* bnds;
	aitUint8* ptr = (aitUint8*)buf;

	// This functions needs to be divided into two sections
	// 1) copy ALL the bounds out
	// 2) copy ALL the data out

	for(i=0;i<tot_dds;i++)
	{
		if(dd[i].isContainer())
		{
			// don't mark flat if container - 1D bounds must be present
			if(dd[i].bounds)
			{
				bnds=(gddBounds*)(ptr);
				for(j=0;j<dd[i].dimension();j++) bnds[j]=dd[i].bounds[j];
				dd[i].bounds=bnds;
				ptr+=j*sizeof(gddBounds);
			}
			else
			{
				// this is an error condition!
				dd[i].bounds=NULL;
			}
		}
		else if(dd[i].isAtomic())
		{
			if(dd[i].bounds)
			{
				// copy the bounds
				// need to mark flat if bounds are present in an atomic type
				dd[i].markFlat();
				bnds=(gddBounds*)(ptr);
				for(j=0;j<dd[i].dimension();j++) bnds[j]=dd[i].bounds[j];
				dd[i].bounds=bnds;
				ptr+=j*sizeof(gddBounds);

				// copy the data
				if(dd[i].dataPointer())
				{
					if(dd[i].primitiveType()==aitEnumString)
					{
						// not very good way to do it, size info bad
						aitString* str = (aitString*)dd[i].dataPointer();
						sz=aitString::compact(str,
							dd[i].getDataSizeElements(),ptr,size);
					}
					else
					{
						// need to copy data here, align to size of double
						sz=dd[i].getDataSizeBytes();
						memcpy(ptr,dd[i].dataPointer(),sz);
					}
					dd[i].setData(ptr);
					ptr+=align8(sz); // do alignment
				}
			}
			else
			{
				// bounds not required
				dd[i].bounds=NULL;
			}
		}
		else if(dd[i].isScalar())
		{
			// here is special case for String types
			if(dd[i].primitiveType()==aitEnumString)
			{
				aitString* str=(aitString*)dd[i].dataAddress();
				if(str->string())
				{
					memcpy(ptr,str->string(),str->length()+1);
					str->force((char*)ptr);
					str->forceConstant();
					ptr+=str->length()+1;
				}
				else
					str->init();
			}
			else if(dd[i].primitiveType()==aitEnumFixedString)
			{
				if(dd[i].data.FString)
					memcpy(ptr,dd[i].data.FString,sizeof(aitFixedString));
				dd[i].data.FString=(aitFixedString*)ptr;
				ptr+=sizeof(aitFixedString);
			}
		}
	}
	return 0;
}

int gdd::flattenDDs(gddContainer* dd, void* buf, size_t size)
{
	gdd* ptr=(gdd*)buf;
	int i,tot,pos,spos;
	gdd *pdd;
	gddCursor cur;

	cur=dd->getCursor();

	// make first pass to copy all the container's DDs into the buffer
	for(tot=0,pdd=cur.first();pdd;pdd=pdd->next(),tot++)
	{
		ptr[tot]=*pdd;
		ptr[tot].destruct=NULL;
		ptr[tot].setNext(&ptr[tot+1]);
		ptr[tot].noReferencing();
	}
	ptr[tot-1].setNext(NULL);

	// make second pass to copy all child containers into buffer
	for(pos=tot,i=0;i<tot;i++)
	{
		if(ptr[i].isContainer())
		{
			if(ptr[i].dataPointer())
			{
				spos=pos;
				pos+=flattenDDs((gddContainer*)&ptr[i],&ptr[pos],
					size-(pos*sizeof(gdd)));
				ptr[i].markFlat();
				ptr[i].setData(&ptr[spos]);
			}
			else
			{
				ptr[i].setData(NULL);
				ptr[i].destruct=new gddContainerCleaner(&ptr[i]);
				ptr[i].destruct->reference();
			}
		}
	}
	return pos;
}

gddStatus gdd::convertOffsetsToAddress(void)
{
	aitUint8* pdd = (aitUint8*)this;
	aitUint32 bnds = (aitUint32)(bounds);
	aitUint32 dp = (aitUint32)(dataPointer());
	gdd* tdd;
	gddContainer* cdd;
	gddCursor cur;
	aitString* str;
	aitIndex i;
	const char* cstr;

	if(isContainer())
	{
		// change bounds and data first
		bounds=(gddBounds*)(pdd+bnds);
		setData(pdd+dp);
		cdd=(gddContainer*)this;
		cur=cdd->getCursor();

		for(tdd=cur.first();tdd;tdd=cur.next())
		{
			if(tdd->next()) tdd->setNext((gdd*)(pdd+(aitUint32)tdd->next()));
			tdd->convertOffsetsToAddress();
		}
	}
	else
	{
		if(isAtomic())
		{
			bounds=(gddBounds*)(pdd+bnds);
			setData(pdd+dp);
			if(primitiveType()==aitEnumString)
			{
				// force all the strings in the array to offsets
				str=(aitString*)dataPointer();
				for(i=0;i<getDataSizeElements();i++)
				{
					if(str[i].string())
					{
						cstr=str[i].string();
						str[i].force(pdd+(unsigned long)cstr);
					}
					else
						str[i].init();
				}
			}
		}
		else if(isScalar())
		{
			if(primitiveType()==aitEnumFixedString)
				if(data.FString) setData(pdd+dp);
			else if(primitiveType()==aitEnumString)
			{
				str=(aitString*)dataAddress();
				if(str->string())
				{
					cstr=str->string();
					str->force(pdd+(unsigned long)cstr);
				}
				else
					str->init();
			}
		}
	}
	return 0;
}

gddStatus gdd::convertAddressToOffsets(void)
{
	aitUint8* pdd = (aitUint8*)this;
	aitUint8* bnds = (aitUint8*)(bounds);
	aitUint8* dp = (aitUint8*)(dataPointer());
	gddContainer* tdd;
	gddCursor cur;
	gdd *cdd,*ddd;
	aitString* str;
	aitIndex i;
	const char* cstr;

	// does not ensure that all the members of a container are flat!
	if(!isFlat())
	{
		gddAutoPrint("gdd::convertAddressToOffsets()",gddErrorNotAllowed);
		return gddErrorNotAllowed;
	}

	if(isContainer())
	{
		tdd=(gddContainer*)this;
		cur=tdd->getCursor();

		for(cdd=cur.first();cdd;)
		{
			ddd=cdd;
			cdd=cur.next();
			ddd->convertAddressToOffsets();
			if(cdd) ddd->setNext((gdd*)((aitUint8*)(ddd->next())-pdd));
		}

		// bounds and data of container to offsets
		setData((gdd*)(dp-pdd));
		bounds=(gddBounds*)(bnds-pdd);
	}
	else
	{
		if(isAtomic())
		{
			if(primitiveType()==aitEnumString)
			{
				// force all the strings in the array to offsets
				str=(aitString*)dataPointer();
				for(i=0;i<getDataSizeElements();i++)
				{
					cstr=str[i].string();
					if(cstr) str[i].force(cstr-(const char*)pdd);
					else str[i].init();
				}
			}
			// bounds and data of atomic to offsets
			setData((gdd*)(dp-pdd));
			bounds=(gddBounds*)(bnds-pdd);
		}
		else if(isScalar())
		{
			// handle the special string scaler cases
			if(primitiveType()==aitEnumFixedString)
				if(data.FString) setData((gdd*)(dp-pdd));
			else if(primitiveType()==aitEnumString)
			{
				str=(aitString*)dataAddress();
				cstr=str->string();
				if(cstr) str->force(cstr-(const char*)pdd);
				else str->init();
			}
		}
	}
	return 0;
}

gddStatus gdd::clearData(void)
{
	gddStatus rc=0;
	
	if(isContainer()||isManaged()||isFlat())
	{
		gddAutoPrint("gdd::clearData()",gddErrorNotAllowed);
		rc=gddErrorNotAllowed;
	}
	else
	{
		if(destruct)
		{
			destruct->destroy(dataPointer());
			destruct=NULL;
		}
		freeBounds();
		setData(NULL);
	}
	return rc;
}

gddStatus gdd::clear(void)
{
	if(isFlat()||isManaged())
	{
		gddAutoPrint("gdd::clear()",gddErrorNotAllowed);
		return gddErrorNotAllowed;
	}

	if(isAtomic())
	{
		destroyData();
		changeType(0,aitEnumInvalid);
	}
	else if(isContainer())
	{
		gddContainer* cdd = (gddContainer*)this;
		gddCursor cur = cdd->getCursor();
		gdd *dd,*tdd;

		for(dd=cur.first();dd;)
		{
			tdd=dd;
			dd=cur.next();
			if(tdd->unreference()<0) delete tdd;
		}
		setBound(0,0,0);
		setData(NULL);
	}

	return 0;
}

// Curently gives no indication of failure, which is bad.
// Obviously managed or flattened DDs cannot be redone.
// However, a DD that is within a managed or flattened container can
// use this to describe data - how's that for obscure
// This is required if the "value" is to be allowed within a container
// The "value" could be scaler or an array of unknown size.  The same is
// true of the enum strings and "units" attribute.

gddStatus gdd::reset(aitEnum prim, int dimen, aitIndex* cnt)
{
	int app=applicationType();
	int i;
	gddStatus rc;

	if((rc=clear())==0)
	{
		init(app,prim,dimen);
		for(i=0;i<dimen;i++)
			setBound(i,0,cnt[i]);
	}
	return rc;
}

void gdd::get(aitString& d)
{
	if(primitiveType()==aitEnumString)
	{
		aitString* s=(aitString*)dataAddress();
		d=*s;
	}
	else if(dim==1 && primitiveType()==aitEnumInt8) 
	{
		if(isConstant())
		{
			const char* ci=(const char*)dataPointer();
			d.installString(ci);
		}
		else
		{
			char* i=(char*)dataPointer();
			d.installString(i);
		}
	}
	else
		get(aitEnumString,&d);
}
void gdd::get(aitFixedString& d)
{
	if(primitiveType()==aitEnumFixedString){
		strncpy(d.fixed_string,data.FString->fixed_string,
			sizeof(d));
		d.fixed_string[sizeof(d)-1u] = '\0';
	}
	else if(primitiveType()==aitEnumInt8 && dim==1) {
		strncpy(d.fixed_string,(char*)dataPointer(), 
			sizeof(d));
		d.fixed_string[sizeof(d)-1u] = '\0';
	}
	else
		get(aitEnumFixedString,&d);
}

void gdd::getConvert(aitString& d)
{
	if(primitiveType()==aitEnumInt8 && dim==1)
	{
		if(isConstant())
		{
			const char* ci=(const char*)dataPointer();
			d.installString(ci);
		}
		else
		{
			char* i=(char*)dataPointer();
			d.installString(i);
		}
	}
	else
		get(aitEnumString,&d);
}

void gdd::getConvert(aitFixedString& d)
{
	if(primitiveType()==aitEnumInt8 && dim==1){
		strncpy(d.fixed_string,(char*)dataPointer(),
			sizeof(d));
		d.fixed_string[sizeof(d)-1u] = '\0';
	}
	else
		get(aitEnumFixedString,d.fixed_string);
}

gddStatus gdd::put(const aitString& d)
{
	gddStatus rc=0;
	if(isScalar())
	{
		aitString* s=(aitString*)dataAddress();
		*s=d;
		setPrimType(aitEnumString);
	}
	else
	{
		gddAutoPrint("gdd::put(aitString&)",gddErrorNotAllowed);
		rc=gddErrorNotAllowed;
	}

	return rc;
}

gddStatus gdd::put(aitString& d)
{
	gddStatus rc=0;
	if(isScalar())
	{
		aitString* s=(aitString*)dataAddress();
		*s=d;
		setPrimType(aitEnumString);
	}
	else
	{
		gddAutoPrint("gdd::put(aitString&)",gddErrorNotAllowed);
		rc=gddErrorNotAllowed;
	}

	return rc;
}

// this is dangerous, should the fixed string be copied here?
gddStatus gdd::put(const aitFixedString& d)
{
	gddStatus rc=0;

	if(primitiveType()==aitEnumFixedString)
	{
		if(data.FString==NULL)
			data.FString=new aitFixedString;
	}
	else
	{
		// this is so bad, like the other put() functions, need fixen
		setPrimType(aitEnumFixedString);
		data.FString=new aitFixedString;
	}

	memcpy(data.FString->fixed_string,d.fixed_string,sizeof(d.fixed_string));
	return rc;
}

void gdd::putConvert(const aitString& d)
{
	if(primitiveType()==aitEnumInt8 && dim==1)
	{
		aitUint32 len = getDataSizeElements();
		char* cp = (char*)dataPointer();
		if(d.length()<len) len=d.length();
		strncpy(cp,d.string(),len);
		cp[len]='\0';
	}
	else
		set(aitEnumString,(aitString*)&d);
}

void gdd::putConvert(const aitFixedString& d)
{
	if(primitiveType()==aitEnumInt8 && dim==1)
	{
		aitUint32 len = getDataSizeElements();
		char* cp = (char*)dataPointer();
		if(sizeof(d.fixed_string)<len) len=sizeof(d.fixed_string);
		strncpy(cp,d.fixed_string,len);
		cp[len]='\0';
	}
	else
		set(aitEnumFixedString,(void*)d.fixed_string);
}

// copy each of the strings into this DDs storage area
gddStatus gdd::put(const aitString* const d)
{
	return genCopy(aitEnumString,d);
}

// copy each of the strings into this DDs storage area
gddStatus gdd::put(const aitFixedString* const d)
{
	gddStatus rc=0;

	if(isAtomic())
		if(dataPointer())
			aitConvert(primitiveType(),dataPointer(),aitEnumFixedString,d,
				getDataSizeElements());
		else
			genCopy(aitEnumFixedString,d);
	else
	{
		gddAutoPrint("gdd::put(const aitFixedString*const)",gddErrorNotAllowed);
		rc=gddErrorTypeMismatch;
	}

	return rc;
}

gddStatus gdd::putRef(const gdd*)
{
	gddAutoPrint("gdd::putRef(const gdd*) - NOT IMPLEMENTED",
		gddErrorNotSupported);
	return gddErrorNotSupported;
}

gddStatus gdd::put(const gdd* dd)
{
	gddStatus rc=0;
	aitTimeStamp ts;
	aitUint32 esz;
	aitUint8* arr;
	size_t sz;

	// bail out quickly is either dd is a container
	if(isContainer() || dd->isContainer())
	{
		gddAutoPrint("gdd::put(const gdd*)",gddErrorNotSupported);
		return gddErrorNotSupported;
	}
		
	// this primitive must be valid for this is work - set to dd if invalid
	if(!aitValid(primitiveType()))
	{
		// note that flags, etc. are not set here - just data related stuff
		destroyData();
		setPrimType(dd->primitiveType());
		setDimension(dd->dimension(),dd->getBounds());
	}

	if(isScalar() && dd->isScalar())
	{
		// this is the simple case - just make this scaler look like the other
		set(dd->primitiveType(),dd->dataVoid());
	}
	else if(isScalar()) // dd must be atomic if this is true
	{
		if((primitiveType()==aitEnumString ||
			primitiveType()==aitEnumFixedString) &&
			dd->primitiveType()==aitEnumInt8)
		{
			// special case for aitInt8--->aitString (hate this)
			aitInt8* i1=(aitInt8*)dd->dataPointer();
			put(i1);
		}
		else
			set(dd->primitiveType(),dd->dataPointer());
	}
	else if(dd->isScalar()) // this must be atomic if true
	{
		if(primitiveType()==aitEnumInt8 && dd->primitiveType()==aitEnumString)
		{
			// special case for aitString--->aitInt8
			aitString* s1=(aitString*)dd->dataAddress();
			put(*s1);
		}
		else if(primitiveType()==aitEnumInt8 &&
				dd->primitiveType()==aitEnumFixedString)
		{
			// special case for aitFixedString--->aitInt8
			aitFixedString* s2=data.FString;
			if(s2) put(*s2);
		}
		else
		{
			if(getDataSizeElements()>0)
				aitConvert(primitiveType(),dataPointer(),
					dd->primitiveType(),dd->dataVoid(),1);
			else
			{
				// this may expose an inconsistancy in the library.
				// you can register a gdd with bounds and no data
				// which marks it flat

				if(isFlat() || isManaged())
				{
					gddAutoPrint("gdd::put(const gdd*)",gddErrorNotAllowed);
					rc=gddErrorNotAllowed;
				}
				else
				{
					destroyData();
					set(dd->primitiveType(),dd->dataVoid());
				}
			}
		}
	}
	else // both dd and this must be atomic
	{
		// carefully place values from dd into this
		if(dataPointer()==NULL)
		{
			if(isFlat() || isManaged() || isContainer())
			{
				gddAutoPrint("gdd::put(const gdd*)",gddErrorNotAllowed);
				rc=gddErrorNotAllowed;
			}
			else
			{
				setPrimType(dd->primitiveType());
				sz=describedDataSizeBytes();

				// allocate a data buffer for the user
				if((arr=new aitUint8[sz]))
				{
					destruct=new gddDestructor;
					destruct->reference();
					setData(arr);
				}
				else
				{
					gddAutoPrint("gdd::copyData(const gdd*)",gddErrorNewFailed);
					rc=gddErrorNewFailed;
				}
			}
		}

		if(rc==0)
		{
			esz=describedDataSizeElements();

			// this code currently only works correctly with one
			// dimensional arrays.

			arr=(aitUint8*)dataPointer();
			aitConvert(primitiveType(),
				&arr[aitSize[primitiveType()]*dd->getBounds()->first()],
				dd->primitiveType(), dd->dataPointer(),esz);
		}
	}

	setStatSevr(dd->getStat(),dd->getSevr());
	dd->getTimeStamp(&ts);
	setTimeStamp(&ts);
	return rc;
}

size_t gdd::outHeader(void* buf,aitUint32 bufsize)
{
	// simple encoding for now..  will change later
	// this is the SLOW, simple version

	aitUint8* b = (aitUint8*)buf;
	aitUint8* app = (aitUint8*)&appl_type;
	aitUint8* stat = (aitUint8*)&status;
	aitUint8* ts_sec = (aitUint8*)&time_stamp.tv_sec;
	aitUint8* ts_nsec = (aitUint8*)&time_stamp.tv_nsec;
	int i,j,sz;
	aitIndex ff,ss;
	aitUint8 *f,*s;

	// verify that header will fit into buffer first
	sz=4+sizeof(status)+sizeof(time_stamp)+sizeof(appl_type)+
		sizeof(prim_type)+sizeof(dim)+(dim*sizeof(gddBounds));

	if(sz>bufsize) return 0; // blow out here!

	*(b++)='H'; *(b++)='E'; *(b++)='A'; *(b++)='D';

	// how's this for putrid
	*(b++)=dim;
	*(b++)=prim_type;

	if(aitLocalNetworkDataFormatSame)
	{
		*(b++)=app[0]; *(b++)=app[1];
		for(i=0;i<sizeof(status);i++) *(b++)=stat[i];
		for(i=0;i<sizeof(time_stamp.tv_sec);i++) *(b++)=ts_sec[i];
		for(i=0;i<sizeof(time_stamp.tv_nsec);i++) *(b++)=ts_nsec[i];
	}
	else
	{
		*(b++)=app[1]; *(b++)=app[0];
		for(i=sizeof(status)-1;i>=0;i--) *(b++)=stat[i];
		for(i=sizeof(time_stamp.tv_sec)-1;i>=0;i--) *(b++)=ts_sec[i];
		for(i=sizeof(time_stamp.tv_nsec)-1;i>=0;i--) *(b++)=ts_nsec[i];
	}

	// put out the bounds info
	for(j=0;j<dim;j++)
	{
		ff=bounds[j].first(); f=(aitUint8*)&ff;
		ss=bounds[j].size(); s=(aitUint8*)&ss;
		if(aitLocalNetworkDataFormatSame)
		{
			for(i=0;i<sizeof(aitIndex);i++) *(b++)=s[i];
			for(i=0;i<sizeof(aitIndex);i++) *(b++)=f[i];
		}
		else
		{
			for(i=sizeof(aitIndex)-1;i>=0;i--) *(b++)=s[i];
			for(i=sizeof(aitIndex)-1;i>=0;i--) *(b++)=f[i];
		}
	}
	return sz;
}
size_t gdd::outData(void* buf,aitUint32 bufsize, aitEnum e, aitDataFormat f)
{
	// put data into user's buffer in the format that the user wants (e/f).
	// if e is invalid, then use whatever format this gdd describes.

	aitUint32 sz = getDataSizeElements();
	aitUint32 len = getDataSizeBytes();
	aitEnum type=(e==aitEnumInvalid)?primitiveType():e;

	if(len>bufsize) return 0; // blow out early

	if(sz>0)
	{
		if(f==aitLocalDataFormat)
			aitConvert(type,buf,primitiveType(),dataVoid(),sz);
		else
			aitConvertToNet(type,buf,primitiveType(),dataVoid(),sz);
	}

	return len;
}
size_t gdd::out(void* buf,aitUint32 bufsize,aitDataFormat f)
{
	size_t index = outHeader(buf,bufsize);
	size_t rc;

	if(index>0)
		rc=outData(((char*)buf)+index,bufsize-index,aitEnumInvalid,f)+index;
	else
		rc=0;

	return rc;
}

size_t gdd::inHeader(void* buf)
{
	// simple encoding for now..  will change later
	// this is the SLOW, simple version

	aitUint16 inapp;
	aitUint8 inprim;
	aitUint8 indim;

	aitUint8* b = (aitUint8*)buf;
	aitUint8* b1 = b;
	aitUint8* app = (aitUint8*)&inapp;
	aitUint8* stat = (aitUint8*)&status;
	aitUint8* ts_sec = (aitUint8*)&time_stamp.tv_sec;
	aitUint8* ts_nsec = (aitUint8*)&time_stamp.tv_nsec;
	int i,j;
	aitIndex ff,ss;
	aitUint8 *f,*s;

	if(strncmp((char*)b,"HEAD",4)!=0) return 0;
	b+=4;

	indim=*(b++);
	inprim=*(b++);

	if(aitLocalNetworkDataFormatSame)
	{
		app[0]=*(b++); app[1]=*(b++);
		init(inapp,(aitEnum)inprim,indim);
		for(i=0;i<sizeof(status);i++) stat[i]=*(b++);
		for(i=0;i<sizeof(time_stamp.tv_sec);i++) ts_sec[i]=*(b++);
		for(i=0;i<sizeof(time_stamp.tv_nsec);i++) ts_nsec[i]=*(b++);
	}
	else
	{
		app[1]=*(b++); app[0]=*(b++);
		init(inapp,(aitEnum)inprim,indim);
		for(i=sizeof(status)-1;i>=0;i--) stat[i]=*(b++);
		for(i=sizeof(time_stamp.tv_sec)-1;i>=0;i--) ts_sec[i]=*(b++);
		for(i=sizeof(time_stamp.tv_nsec)-1;i>=0;i--) ts_nsec[i]=*(b++);
	}

	// read in the bounds info
	f=(aitUint8*)&ff;
	s=(aitUint8*)&ss;
	
	for(j=0;j<dim;j++)
	{
		if(aitLocalNetworkDataFormatSame)
		{
			for(i=0;i<sizeof(aitIndex);i++) s[i]=*(b++);
			for(i=0;i<sizeof(aitIndex);i++) f[i]=*(b++);
		}
		else
		{
			for(i=sizeof(aitIndex)-1;i>=0;i--) s[i]=*(b++);
			for(i=sizeof(aitIndex)-1;i>=0;i--) f[i]=*(b++);
		}
		bounds[j].setFirst(ff);
		bounds[j].setSize(ss);
	}
	return (size_t)(b-b1);
}
size_t gdd::inData(void* buf,aitUint32 tot, aitEnum e, aitDataFormat f)
{
	size_t rc;

	// Get data from a buffer and put it into this gdd.  Lots of rules here.
	// 1) tot is the total number of elements to copy from buf to this gdd.
	// * if tot is zero, then use the element count described in the gdd.
	// * if tot is not zero, then set the gdd up as a 1 dimensional array
	//   with tot elements in it.
	// 2) e is the primitive data type of the incoming data.
	// * if e is aitEnumInvalid, then use the gdd primitive type.
	// * if e is valid and gdd primitive type is invalid, set the gdd
	//   primitive type to e.
	// * if e is valid and so is this gdd primitive type, then convert the
	//   incoming data from type e to gdd primitive type.

	// Bad error condition, don't do anything.
	if(e==aitEnumInvalid && primitiveType()==aitEnumInvalid)
		return 0;

	aitIndex sz=tot;
	aitEnum src_type=(e==aitEnumInvalid)?primitiveType():e;
	aitEnum dest_type=(primitiveType()==aitEnumInvalid)?e:primitiveType();

	// I'm not sure if this is the best way to do this.
	if(sz>0)
		reset(dest_type,dimension(),&sz);

	if(genCopy(src_type,buf,f)==0)
		rc=getDataSizeBytes();
	else 
		rc=0;

	return rc;
}
size_t gdd::in(void* buf, aitDataFormat f)
{
	size_t index = inHeader(buf);
	size_t rc;

	if(index>0)
		rc=inData(((char*)buf)+index,0,aitEnumInvalid,f)+index;
	else
		rc=0;

	return rc;

}

