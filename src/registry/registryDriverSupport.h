#ifndef INCregistryDriverSupporth
#define INCregistryDriverSupporth


#ifdef __cplusplus
extern "C" {
#endif

/* c interface definitions */
int registryDriverSupportAdd(const char *name,struct drvet *pdrvet);
struct drvet *registryDriverSupportFind(const char *name);

#ifdef __cplusplus
}
#endif


#endif /* INCregistryDriverSupporth */
