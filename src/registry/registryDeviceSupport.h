#ifndef INCregistryDeviceSupporth
#define INCregistryDeviceSupporth


#ifdef __cplusplus
extern "C" {
#endif

int registryDeviceSupportAdd(const char *name,struct dset *pdset);
struct dset *registryDeviceSupportFind(const char *name);

#ifdef __cplusplus
}
#endif


#endif /* INCregistryDeviceSupporth */
