/* pvAdapter.h */
/* broken out of cadef.h so that new database access can include it*/
#ifndef INCLpvAdapterh
#define INCLpvAdapterh

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Warning - this is an temporary CA client API
 * which will probably vanish in the future.
 */
typedef void * pvId;
typedef void * putNotifyId;
struct db_field_log;
struct putNotify;
typedef void (*putNotifyCallback)(void *);
typedef void (*eventQueueInit)(void *);
typedef void (*eventQueueEventUser)
    (void *usrArg, pvId id, int eventsRemaining, struct db_field_log *);
typedef void (*eventQueueExtraLaborUser)(void *);
typedef struct pvAdapter {
    pvId (*p_pvNameToId) (const char *pname);
    int (*p_pvPutField) (pvId, int src_type,
                    const void *psrc, int no_elements);
    int (*p_pvGetField) (pvId, int dest_type,
                    void *pdest, int no_elements, void *pfl);
    long (*p_pvPutNotifyInitiate) (pvId, unsigned type, unsigned long count, 
        const void *pValue, putNotifyCallback, void *usrPvt, putNotifyId * pID);
    void (*p_pvPutNotifyDestroy) (putNotifyId);
    const char * (*p_pvName) (pvId);
    unsigned long (*p_pvNoElements) (pvId);
    short (*p_pvType) (pvId);
    void * (*p_pvEventQueueInit) (void);
    int (*p_pvEventQueueStart) (void *, const char *taskname, 
        eventQueueInit, void *init_func_arg, int priority_offset);
    void (*p_pvEventQueueClose) (void *);
    void (*p_pvEventQueuePostSingleEvent) (void *es);
    void * (*p_pvEventQueueAddEvent) (void *ctx, pvId id,
        eventQueueEventUser, void *user_arg, unsigned select);
    void (*p_pvEventQueueCancelEvent) (void *es);
    int (*p_pvEventQueueAddExtraLaborEvent) (void *,
        eventQueueExtraLaborUser, void *arg);
    int (*p_pvEventQueuePostExtraLabor) (void *ctx);
} pvAdapter;

epicsShareFunc void * epicsShareAPI ca_create_context (pvAdapter *pAdapter);

#ifdef __cplusplus
}
#endif

/*
 * no additions below this endif
 */
#endif /* INCLpvAdapterh */
