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

#define epicsExportSharedSymbols
#include "gddAppTable.h"

// -----------------general destructor for managed gdds--------------------

void gddApplicationTypeDestructor::run(void* v)
{
	gdd* ec = (gdd*)v;
	// fprintf(stderr," destructing %8.8x\n",v);
	gddApplicationTypeTable* db = (gddApplicationTypeTable*)arg;
	db->freeDD(ec);
}

gddApplicationTypeElement::gddApplicationTypeElement(void) { }
gddApplicationTypeElement::~gddApplicationTypeElement(void) { }

// --------------------------app table stuff-----------------------------

gddApplicationTypeTable gddApplicationTypeTable::app_table;

gddApplicationTypeTable& gddApplicationTypeTable::AppTable(void)
{
	return gddApplicationTypeTable::app_table;
}

gddApplicationTypeTable::gddApplicationTypeTable(aitUint32 tot)
{
	aitUint32 i,total;

	// round tot up to nearest power of 2
	for(i=1u<<31;i && !(tot&i);i>>=1);
	if(i==0)
		total=1;
	else if(i==tot)
		total=tot;
	else
		total=i<<1;

	max_groups=total/APPLTABLE_GROUP_SIZE;
	if((max_groups*APPLTABLE_GROUP_SIZE) != total) ++max_groups;
	max_allowed=total;
	total_registered=1;
	attr_table=new gddApplicationTypeElement*[max_groups];

	for(i=0;i<max_groups;i++) attr_table[i]=NULL;
	GenerateTypes();
}

gddApplicationTypeTable::~gddApplicationTypeTable(void)
{
	unsigned i,j;
	gdd* dd;
	aitUint8* blk;

	// fprintf(stderr,"in gddApplicationTypeTable dest\n");

	if(this!=&app_table) return;
	
	for(i=0u;i<max_groups;i++)
	{
		if(attr_table[i])
		{
			// fprintf(stderr,"Delete TypeTable: group %d exists\n",i);
			for(j=0u;j<APPLTABLE_GROUP_SIZE;j++)
			{
				switch(attr_table[i][j].type)
				{
				case gddApplicationTypeNormal:
					// fprintf(stderr,"Delete TypeTable: app %u normal\n",j);
					if(attr_table[i][j].app_name)
						delete [] attr_table[i][j].app_name;
					break;
				case gddApplicationTypeProto:
					// fprintf(stderr,"Delete TypeTable: app %u has proto\n",j);
					if(attr_table[i][j].app_name)
						delete [] attr_table[i][j].app_name;

					if(attr_table[i][j].proto)
					{
						// if(attr_table[i][j].free_list)
							// fprintf(stderr," app %u has free_list\n",j);

						// The proto is stored as flattened gdd
						blk=(aitUint8*)attr_table[i][j].proto;
						delete [] blk;

						for(dd=attr_table[i][j].free_list; dd;)
						{
							blk=(aitUint8*)dd;
							// fprintf(stderr,"  delete dd %8.8x\n",blk);
							dd=(gdd*)dd->next();
							delete [] blk;
						}
					}

					if(attr_table[i][j].map)
						delete [] attr_table[i][j].map;
					break;
				case gddApplicationTypeUndefined: break;
				default: break;
				}
			}
			delete [] attr_table[i];
		}
	}
	delete [] attr_table;
}

int gddApplicationTypeTable::describeDD(gddContainer* dd, FILE* fd,
	int level, char* tn)
{
	gddCursor cur = dd->getCursor();
	gdd* pdd;
	char tmp[8];
	char* cp;
	char* str;

	strcpy(tmp,"unknown");

	for(pdd=cur.first();pdd;pdd=pdd->next())
	{
		if((cp=getName(pdd->applicationType()))==NULL) cp=tmp;
		fprintf(fd,"#define gddAppTypeIndex_%s_%s %d\n",tn,cp,level++);
	}

	for(pdd=cur.first();pdd;pdd=pdd->next())
	{
		if((cp=getName(pdd->applicationType()))==NULL) cp=tmp;
		if(pdd->isContainer())
		{
			str = new char[strlen(cp)+strlen(tn)+3];
			strcpy(str,tn);
			strcat(str,"_");
			strcat(str,cp);
			level=describeDD((gddContainer*)pdd,fd,level,str);
			delete [] str;
		}
	}
	return level;
}

void gddApplicationTypeTable::describe(FILE* fd)
{
	unsigned i,j;
	gdd* dd;
	char* tn;

	fprintf(fd,"\n");
	for(i=0;i<max_groups;i++)
	{
		if(attr_table[i])
		{
			for(j=0;j<APPLTABLE_GROUP_SIZE;j++)
			{
				switch(attr_table[i][j].type)
				{
				case gddApplicationTypeNormal:
				case gddApplicationTypeProto:
					tn=attr_table[i][j].app_name;
					fprintf(fd,"#define gddAppType_%s\t%u\n",
						tn,i*APPLTABLE_GROUP_SIZE+j);

					if( (dd=attr_table[i][j].proto) )
					{
						fprintf(fd,"#define gddAppTypeIndex_%s 0\n",tn);
						if(dd->isContainer())
							describeDD((gddContainer*)dd,fd,1,tn);
						// fprintf(fd,"\n");
					}
					break;
				case gddApplicationTypeUndefined: break;
				default: break;
				}
			}
		}
	}
	fprintf(fd,"\n");
}

gddStatus gddApplicationTypeTable::registerApplicationType(
	const char* const name,aitUint32& new_app)
{
	aitUint32 i,group,app,rapp;
	gddStatus rc;

	if( (new_app=getApplicationType(name)) )
	{
		// gddAutoPrint(gddErrorAlreadyDefined);
		return gddErrorAlreadyDefined;
	}
	if(total_registered>max_allowed)
	{
		gddAutoPrint("gddAppTable::registerApplicationType()",gddErrorAtLimit);
		return gddErrorAtLimit;
	}

    {
        epicsGuard < epicsMutex > guard ( sem );
	    rapp=total_registered++;
    }

	if((rc=splitApplicationType(rapp,group,app))<0) return rc;

	if(attr_table[group])
	{
		// group already allocated - check is app already refined
		if(attr_table[group][app].type!=gddApplicationTypeUndefined)
		{
			// gddAutoPrint(gddErrorAlreadyDefined);
			return gddErrorAlreadyDefined;
		}
	}
	else
	{
		// group must be allocated
		attr_table[group]=new gddApplicationTypeElement[APPLTABLE_GROUP_SIZE];

		// initialize each element of the group as undefined
		for(i=0;i<APPLTABLE_GROUP_SIZE;i++)
		{
			attr_table[group][i].type=gddApplicationTypeUndefined;
			attr_table[group][i].map=NULL;
		}
	}

	attr_table[group][app].app_name=strDup(name);
	attr_table[group][app].type=gddApplicationTypeNormal;
	attr_table[group][app].proto=NULL;
	attr_table[group][app].free_list=NULL;

	new_app=rapp;
	// fprintf(stderr,"registered <%s> %d\n",name,(int)new_app);
	return 0;
}

// registering a prototype of an empty container causes problems.
// The current implementation does not monitor this so the container
// is not cleared when free.  A user may, however, include an empty
// container within a prototype container, and everything will work
// correctly.

gddStatus gddApplicationTypeTable::registerApplicationTypeWithProto(
	const char* const name, gdd* protoDD, aitUint32& new_app)
{
	aitUint32 group,app,rapp;
	aitUint8* blk;
	size_t sz;
	aitIndex tot;
	aitUint16 x;
	aitUint16 i;
	gddStatus rc;

	if( (rc=registerApplicationType(name,new_app)) ) return rc;

	rapp=new_app;
	protoDD->setApplType(rapp);
	splitApplicationType(rapp,group,app);

	// user gives me the protoDD, so I should not need to reference it.
	// Warning, it the user does unreference() on it unknowningly, it will
	// go away and cause big problems

	// make sure that the currently registered destructor gets called
	// before setting the new one, this should occur in protoDD.

	// be sure to copy data from DD and create buffer that can be copied
	// easily when user asks for a DD with proto

	// important!! put destructor into each managed DD - what if atomic?
	// protoDD->registerDestructor(new gddApplicationTypeDestructor(protoDD));

	sz=protoDD->getTotalSizeBytes();
	blk=new aitUint8[sz];
	protoDD->flattenWithAddress(blk,sz,&tot);
	attr_table[group][app].proto_size=sz;
	attr_table[group][app].total_dds=tot;
	protoDD->unreference();

	attr_table[group][app].type=gddApplicationTypeProto;
	attr_table[group][app].proto=(gdd*)blk;
	attr_table[group][app].free_list=NULL;

	// create the stupid mapping table - bad implementation for now
	attr_table[group][app].map=new aitUint16[total_registered];
	attr_table[group][app].map_size=total_registered;
	for(i=0;i<total_registered;i++) attr_table[group][app].map[i]=0;
	for(i=0;i<tot;i++)
	{
		x=attr_table[group][app].proto[i].applicationType();
		if(x<total_registered) attr_table[group][app].map[x]=i;
	}

	return 0;
}

aitUint32 gddApplicationTypeTable::getApplicationType(const char* const name) const
{
	unsigned i,j,rc;

	for(i=0,rc=0;i<max_groups && attr_table[i] && rc==0;i++)
	{
		for(j=0; j<APPLTABLE_GROUP_SIZE && rc==0; j++)
		{
			if(attr_table[i][j].type!=gddApplicationTypeUndefined &&
			   strcmp(name,attr_table[i][j].app_name)==0)
				rc=APPLTABLE_GROUP_SIZE*i+j;
		}
	}

	return rc;
}

char* gddApplicationTypeTable::getName(aitUint32 rapp) const
{
	aitUint32 group,app;

	if(splitApplicationType(rapp,group,app)<0) return NULL;
	if(!attr_table[group]) return NULL;
	if(attr_table[group][app].type==gddApplicationTypeUndefined) return NULL;

	return attr_table[group][app].app_name;
}

gddStatus gddApplicationTypeTable::mapAppToIndex(
	aitUint32 c_app, aitUint32 m_app, aitUint32& x)
{
	aitUint32 group,app;
	gddStatus rc=0;

	if((rc=splitApplicationType(c_app,group,app))==0)
	{
		if(attr_table[group][app].map && m_app<attr_table[group][app].map_size)
		{
			x=attr_table[group][app].map[m_app];
			if(x==0 && c_app!=m_app)
				rc=gddErrorNotDefined;
		}
		else
			rc=gddErrorOutOfBounds;
	}
	// gddAutoPrint("gddAppTable::mapAppToIndex()",rc);
	return rc;
}

gdd* gddApplicationTypeTable::getDD(aitUint32 rapp)
{
	gdd* dd=NULL;
	aitUint32 group,app;
	aitUint8* blk;

	if(splitApplicationType(rapp,group,app)<0) return NULL;

	switch(attr_table[group][app].type)
	{
	case gddApplicationTypeProto:
		attr_table[group][app].sem.lock ();
		if( (dd=attr_table[group][app].free_list) )
		{
			//fprintf(stderr,"Popping a proto DD from list! %d %8.8x\n",app,dd);
			attr_table[group][app].free_list=dd->next();
			attr_table[group][app].sem.unlock ();
		}
		else
		{
			attr_table[group][app].sem.unlock ();
			// copy the prototype
			blk=new aitUint8[attr_table[group][app].proto_size];
			// fprintf(stderr,"Creating a new proto DD! %d %8.8x\n",app,blk);
			attr_table[group][app].proto->flattenWithAddress(blk,
				attr_table[group][app].proto_size);
			dd=(gdd*)blk;
		}
		dd->registerDestructor(new gddApplicationTypeDestructor(this));
		dd->markManaged(); // must be sure to mark the thing as managed!
		break;
	case gddApplicationTypeNormal:
		dd=new gdd(app);
		// fprintf(stderr,"Creating a new normal DD! %d\n",app);
		break;
	case gddApplicationTypeUndefined:	dd=NULL; break;
	default: break;
	}

	return dd;
}

gddStatus gddApplicationTypeTable::freeDD(gdd* dd)
{
	aitUint32 group,app,i;
	gddStatus rc;

	if((rc=splitApplicationType(dd->applicationType(),group,app))<0) return rc;

	if(attr_table[group][app].type==gddApplicationTypeProto)
	{
		// this can be time consuming, clear out all user data from the DD
		// this is done because we are allowed to register atomic protos
		// that user can attach data to - which causes problems because
		// the actual structure of the DD is unknown

		for(i=1;i<attr_table[group][app].total_dds;i++)
		{
			dd[i].destroyData();
			dd[i].setPrimType(attr_table[group][app].proto[i].primitiveType());
			dd[i].setApplType(attr_table[group][app].proto[i].applicationType());
		}

		// fprintf(stderr,"Adding DD to free_list %d\n",app);
		attr_table[group][app].sem.lock ();
		dd->setNext(attr_table[group][app].free_list);
		attr_table[group][app].free_list=dd;
		attr_table[group][app].sem.unlock ();
	}
	else if (attr_table[group][app].type==gddApplicationTypeNormal)
	{
		// fprintf(stderr,"freeDD a normal DD\n");
		dd->unreference();
	}
    else {
        fprintf ( stderr,"gddApplicationTypeTable::freeDD - unexpected DD type was %d\n", 
            attr_table[group][app].type );
    }

	return 0;
}

gddStatus gddApplicationTypeTable::storeValue(aitUint32 ap, aitUint32 uv)
{
	aitUint32 group,app;
	gddStatus rc=0;

	if((rc=splitApplicationType(ap,group,app))<0) return rc;
	if(attr_table[group]==NULL ||
	   attr_table[group][app].type==gddApplicationTypeUndefined)
		rc=gddErrorNotDefined;
	else
		attr_table[group][app].user_value=uv;

	gddAutoPrint("gddAppTable::storeValue()",rc);
	return rc;
}

aitUint32 gddApplicationTypeTable::getValue(aitUint32 ap)
{
	aitUint32 group,app;
	if(splitApplicationType(ap,group,app)<0) return 0;
	if(attr_table[group]==NULL ||
	   attr_table[group][app].type==gddApplicationTypeUndefined)
		return 0;

	return attr_table[group][app].user_value;
}

// ----------------------smart copy functions------------------------
// the source in the container we must walk through is this called
gddStatus gddApplicationTypeTable::copyDD_src(gdd& dest, const gdd& src)
{
	gddStatus rc=0,s;
	gddCursor cur;
	gdd* dd;
	aitIndex index;

	// this could be done better (faster) if we did not always recurse for
	// each GDD.  I could have checked for type container before recursing.

	if(src.isContainer())
	{
		gddContainer& cdd = (gddContainer&) src;

		// go through src gdd and map app types to index into dest
		cur=cdd.getCursor();
		for(dd=cur.first();dd;dd=dd->next()) copyDD_src(dest,*dd);
	}
	else
	{
		// find src gdd in dest container and just do put()
		s=mapAppToIndex(dest.applicationType(),src.applicationType(),index);

		if(s==0)
			rc=dest[index].put(&src);
	}
	return rc;
}

// the destination in the container we must walk through is this called
gddStatus gddApplicationTypeTable::copyDD_dest(gdd& dest, const gdd& src)
{
	gddStatus rc=0,s;
	gddCursor cur;
	gdd* dd;
	aitIndex index;

	if(dest.isContainer())
	{
		gddContainer& cdd = (gddContainer&) dest;
		// go through dest gdd and map app types to index into src
		cur=cdd.getCursor();
		for(dd=cur.first();dd;dd=dd->next()) copyDD_dest(*dd,src);
	}
	else
	{
		// find dest gdd in src container and just do put()
		s=mapAppToIndex(src.applicationType(),dest.applicationType(),index);

		if(s==0)
			rc=dest.put(&src[index]);
	}
	return rc;
}

gddStatus gddApplicationTypeTable::smartCopy(gdd* dest, const gdd* src)
{
	gddStatus rc = gddErrorNotAllowed;

	// only works with managed containers because app table mapping
	// feature is used.

	if(dest->isContainer() && dest->isManaged())
		rc=copyDD_src(*dest,*src);
	else if(src->isContainer() && src->isManaged())
		rc=copyDD_dest(*dest,*src);
    else if(!src->isContainer() && !dest->isContainer()) {
        if ( src->applicationType() == dest->applicationType() ) {
		    rc=dest->put(src); // both are not containers, let gdd handle it
        }
        else {
            rc=gddErrorNotDefined;
        }
    }

	gddAutoPrint("gddAppTable::smartCopy()",rc);

	return rc;
}

// ----------------------smart reference functions------------------------
// the source in the container we must walk through is this called
gddStatus gddApplicationTypeTable::refDD_src(gdd& dest, const gdd& src)
{
	gddStatus rc=0,s;
	gddCursor cur;
	gdd* dd;
	aitIndex index;

	// this could be done better (faster) if we did not always recurse for
	// each GDD.  I could have checked for type container before recursing.

	if(src.isContainer())
	{
		gddContainer& cdd=(gddContainer&)src;
		// go through src gdd and map app types to index into dest
		cur=cdd.getCursor();
		for(dd=cur.first();dd;dd=dd->next()) refDD_src(dest,*dd);
	}
	else
	{
		// find src gdd in dest container and just do put()
		s=mapAppToIndex(dest.applicationType(),src.applicationType(),index);

		if(s==0)
			rc=dest[index].putRef(&src);
	}
	return rc;
}

// the destination in the container we must walk through is this called
gddStatus gddApplicationTypeTable::refDD_dest(gdd& dest, const gdd& src)
{
	gddStatus rc=0,s;
	gddCursor cur;
	gdd* dd;
	aitIndex index;

	if(dest.isContainer())
	{
		gddContainer& cdd=(gddContainer&)dest;
		// go through dest gdd and map app types to index into src
		cur=cdd.getCursor();
		for(dd=cur.first();dd;dd=dd->next()) refDD_dest(*dd,src);
	}
	else
	{
		// find dest gdd in src container and just do put()
		s=mapAppToIndex(src.applicationType(),dest.applicationType(),index);

		if(s==0)
			rc=dest.putRef(&src[index]);
	}
	return rc;
}

gddStatus gddApplicationTypeTable::smartRef(gdd* dest, const gdd* src)
{
	gddStatus rc=0;

	// only works with managed containers because app table mapping
	// feature is used.

	if(dest->isContainer() && dest->isManaged())
		rc=refDD_src(*dest,*src);
	else if(src->isContainer() && src->isManaged())
		rc=refDD_dest(*dest,*src);
	else if(!src->isContainer() && !dest->isContainer())
		rc=dest->putRef(src); // both are not containers, let gdd handle it
	else
		rc=gddErrorNotAllowed;

	gddAutoPrint("gddAppTable::smartRef()",rc);
	return rc;
}
