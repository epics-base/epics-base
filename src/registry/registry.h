#ifndef INCregistryh
#define INCregistryh

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_TABLE_SIZE 1024

int registryAdd(void *registryID,const char *name,void *data);
void *registryFind(void *registryID,const char *name);

int registrySetTableSize(int size);
void registryFree();
int registryDump(void);

#ifdef __cplusplus
}
#endif

#endif /* INCregistryh */
