#ifndef tsDLHashList_H
#define tsDLHashList_H

extern "C" {
#include "gpHash.h"
}

template <class T>
class tsHash
{
private:
	void* hash_table;

public:
    gpHash(void)
	{
		hash_table=NULL;
		gphInitPvt(&hash_table);
	}

	~gateHash(void) { gphFreeMem(hash_table); }
		 
	int add(const char* key, T& item);
	{
		GPHENTRY* entry;
		int rc;

		entry=gphAdd(hash_table,(char*)key,hash_table);

		if(entry==(GPHENTRY*)NULL)
			rc=-1;
		else
		{
			entry->userPvt=(void*)&item;
			rc=0;
		}
		return rc;
	}

	int remove(const char* key,T*& item);
	{
		int rc;

		if(find(key,item)<0)
			rc=-1;
		else
		{
			gphDelete(hash_table,(char*)key,hash_table);
			rc=0;
		}
		return rc;
	}

	int find(const char* key, T*& item);
	{
		GPHENTRY* entry;
		int rc;

		entry=gphFind(hash_table,(char*)key,hash_table);

		if(entry==(GPHENTRY*)NULL)
			rc=-1;
		else
		{
			item=(T*)entry->userPvt;
			rc=0;
		}
		return rc;
	}
};

template <class T>
class tsDLHashList : public tsDLList<T>
{
private:
	tsHash<T> h;
public:
	int add(const char* key, T& item)
	{
		int rc;
		rc=h.add(key,item);
		add(item);
		return rc;
	}
	int find(const char* key, T*& item);
	{
		int rc=0;
		if(h.find(key,item)!=0)
			rc=-1;
		return rc;
	}
	int remove(const char* key,T*& item);
	{
		int rc=0;
		if(h.find(key,item)==0)
		{
			h.remove(key,item);
			remove(*item);
		}
		else
			rc=-1;
		return rc;
	}
};

#endif
