#ifndef INCcantProceedh
#define INCcantProceedh

#ifdef __cplusplus
extern "C" {
#endif

void cantProceed(const char *errorMessage);
void *callocMustSucceed(size_t count, size_t size, const char *errorMessage);
void *mallocMustSucceed(size_t size, const char *errorMessage);
#ifdef __cplusplus
}
#endif

#endif /* cantProceedh */
