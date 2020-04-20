/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#ifndef YAKSURI_XEI_H_INCLUDED
#define YAKSURI_XEI_H_INCLUDED

#include "yaksi.h"
#include <stdint.h>
#include <pthread.h>
#include <level_zero/ze_api.h>
#include "yaksuri_xei_md.h"

#define XE_P2P_ENABLED  (1)
#define XE_P2P_DISABLED (2)
#define XE_P2P_CLIQUES  (3)

/* *INDENT-OFF* */
#ifdef __cplusplus
extern "C" {
#endif
/* *INDENT-ON* */

#define YAKSURI_XEI_XE_ERR_CHECK(zerr)                                  \
    do {                                                                \
        if (zerr != ZE_RESULT_SUCCESS) {                                \
            fprintf(stderr, "XE Error (%s:%s,%d): %d\n", __func__, __FILE__, __LINE__, zerr); \
        }                                                               \
    } while (0)

#define YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail)                \
    do {                                                                \
        if (zerr != ZE_RESULT_SUCCESS) {                                \
            fprintf(stderr, "XE Error (%s:%s,%d): %d\n", __func__, __FILE__, __LINE__, zerr); \
            rc = YAKSA_ERR__INTERNAL;                                   \
            goto fn_fail;                                               \
        }                                                               \
    } while (0)

typedef struct {
    ze_driver_handle_t driver;
    ze_event_pool_handle_t ep;
    uint32_t ndevices;
    ze_device_handle_t *device;
    ze_command_queue_handle_t *cq;
    ze_command_list_handle_t *cl;
    bool **p2p;
} yaksuri_xei_global_s;
extern yaksuri_xei_global_s yaksuri_xei_global;

typedef struct yaksuri_xei_type_s {
    ze_module_handle_t module;

    const char *pack;
    const char *unpack;
    ze_kernel_handle_t pack_kernel;
    ze_kernel_handle_t unpack_kernel;

    yaksuri_xei_md_s *md;
    pthread_mutex_t mdmutex;
    uintptr_t num_elements;
} yaksuri_xei_type_s;

extern const char yaksuri_xei_pup_kernels[];

int yaksuri_xei_finalize_hook(void);
int yaksuri_xei_type_create_hook(yaksi_type_s * type);
int yaksuri_xei_type_free_hook(yaksi_type_s * type);

int yaksuri_xei_event_destroy(void *event);
int yaksuri_xei_event_query(void *event, int *completed);
int yaksuri_xei_event_synchronize(void *event);
int yaksuri_xei_event_add_dependency(void *event, int device);

int yaksuri_xei_get_ptr_attr(const void *buf, yaksur_ptr_attr_s * ptrattr);

int yaksuri_xei_md_alloc(yaksi_type_s * type);
int yaksuri_xei_populate_pupfns(yaksi_type_s * type);

int yaksuri_xei_ipack(const void *inbuf, void *outbuf, uintptr_t count, yaksi_type_s * type,
                      void *device_tmpbuf, int device, void **event);
int yaksuri_xei_iunpack(const void *inbuf, void *outbuf, uintptr_t count, yaksi_type_s * type,
                        void *device_tmpbuf, int device, void **event);
int yaksuri_xei_pup_is_supported(yaksi_type_s * type, bool * is_supported);

/* *INDENT-OFF* */
#ifdef __cplusplus
}
#endif
/* *INDENT-ON* */

#endif /* YAKSURI_XEI_H_INCLUDED */
