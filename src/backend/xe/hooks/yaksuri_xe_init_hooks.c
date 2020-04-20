/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#include "yaksi.h"
#include "yaksuri_xei.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <level_zero/ze_api.h>

static void *xe_host_malloc(uintptr_t size)
{
    void *ptr = NULL;

    ze_host_mem_alloc_desc_t host_desc;
    host_desc.flags = ZE_HOST_MEM_ALLOC_FLAG_DEFAULT;
    host_desc.version = ZE_HOST_MEM_ALLOC_DESC_VERSION_CURRENT;

    ze_result_t zerr = zeDriverAllocHostMem(yaksuri_xei_global.driver, &host_desc, size, 1, &ptr);
    YAKSURI_XEI_XE_ERR_CHECK(zerr);

    return ptr;
}

static void *xe_gpu_malloc(uintptr_t size, int dev)
{
    void *ptr = NULL;

    ze_device_mem_alloc_desc_t device_desc;
    device_desc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_DEFAULT;
    device_desc.ordinal = 0;
    device_desc.version = ZE_DEVICE_MEM_ALLOC_DESC_VERSION_CURRENT;

    ze_result_t zerr = zeDriverAllocDeviceMem(yaksuri_xei_global.driver, &device_desc, size, 1,
                                              yaksuri_xei_global.device[dev], &ptr);
    YAKSURI_XEI_XE_ERR_CHECK(zerr);

    return ptr;
}

static void xe_host_free(void *ptr)
{
    ze_result_t zerr = zeDriverFreeMem(yaksuri_xei_global.driver, ptr);
    YAKSURI_XEI_XE_ERR_CHECK(zerr);
}

static void xe_gpu_free(void *ptr)
{
    ze_result_t zerr = zeDriverFreeMem(yaksuri_xei_global.driver, ptr);
    YAKSURI_XEI_XE_ERR_CHECK(zerr);
}

yaksuri_xei_global_s yaksuri_xei_global;

static int finalize_hook(void)
{
    int rc = YAKSA_SUCCESS;
    ze_result_t zerr;

    for (int i = 0; i < yaksuri_xei_global.ndevices; i++) {
        zerr = zeCommandQueueDestroy(yaksuri_xei_global.cq[i]);
        YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

        free(yaksuri_xei_global.p2p[i]);
    }
    free(yaksuri_xei_global.cq);
    free(yaksuri_xei_global.p2p);
    free(yaksuri_xei_global.device);

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}

static int get_num_devices(int *ndevices)
{
    *ndevices = yaksuri_xei_global.ndevices;

    return YAKSA_SUCCESS;
}

static int check_p2p_comm(int sdev, int ddev, bool * is_enabled)
{
#if XE_P2P == XE_P2P_ENABLED
    *is_enabled = yaksuri_xei_global.p2p[sdev][ddev];
#elif XE_P2P == XE_P2P_CLIQUES
    if ((sdev + ddev) % 2)
        *is_enabled = 0;
    else
        *is_enabled = yaksuri_xei_global.p2p[sdev][ddev];
#else
    *is_enabled = 0;
#endif

    return YAKSA_SUCCESS;
}

int yaksuri_xe_init_hook(yaksur_gpudriver_info_s ** info)
{
    int rc = YAKSA_SUCCESS;
    ze_result_t zerr;

    zerr = zeInit(0);
    YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

    /* FIXME: getting the first driver for now; we need a way to query
     * for the correct driver to use for the Intel GPUs */
    uint32_t num_drivers = 1;
    zerr = zeDriverGet(&num_drivers, &yaksuri_xei_global.driver);
    YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

    /* get the number of devices */
    yaksuri_xei_global.ndevices = 0;
    zerr = zeDeviceGet(yaksuri_xei_global.driver, &yaksuri_xei_global.ndevices, NULL);
    YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);
    assert(yaksuri_xei_global.ndevices);

    /* get handles to each of the available devices */
    yaksuri_xei_global.device = (ze_device_handle_t *) malloc(yaksuri_xei_global.ndevices *
                                                              sizeof(ze_device_handle_t));
    for (int i = 0; i < yaksuri_xei_global.ndevices; i++) {
        zerr = zeDeviceGet(yaksuri_xei_global.driver, &yaksuri_xei_global.ndevices,
                           &yaksuri_xei_global.device[i]);
        YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);
    }

    /* create an event pool for all the devices */
    zerr = zeEventPoolCreate(yaksuri_xei_global.driver, NULL, yaksuri_xei_global.ndevices,
                             yaksuri_xei_global.device, &yaksuri_xei_global.ep);
    YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

    /* create command queues to each available device */
    yaksuri_xei_global.cq = (ze_command_queue_handle_t *)
        malloc(yaksuri_xei_global.ndevices * sizeof(ze_command_queue_handle_t));

    ze_command_queue_desc_t desc = { ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT };
    for (int i = 0; i < yaksuri_xei_global.ndevices; i++) {
        zerr = zeCommandQueueCreate(yaksuri_xei_global.device[i], &desc, &yaksuri_xei_global.cq[i]);
        YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);
    }

    /* check for p2p capability */
    yaksuri_xei_global.p2p = (bool **) malloc(yaksuri_xei_global.ndevices * sizeof(bool *));
    for (int i = 0; i < yaksuri_xei_global.ndevices; i++) {
        yaksuri_xei_global.p2p[i] = (bool *) malloc(yaksuri_xei_global.ndevices * sizeof(bool));

        for (int j = 0; j < yaksuri_xei_global.ndevices; j++) {
            if (i == j) {
                yaksuri_xei_global.p2p[i][j] = 1;
            } else {
                ze_bool_t val;
                zerr =
                    zeDeviceCanAccessPeer(yaksuri_xei_global.device[i],
                                          yaksuri_xei_global.device[j], &val);
                YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

                if (val) {
                    yaksuri_xei_global.p2p[i][j] = true;
                } else {
                    yaksuri_xei_global.p2p[i][j] = false;
                }
            }
        }
    }

    /* set function pointers for the remaining functions */
    *info = (yaksur_gpudriver_info_s *) malloc(sizeof(yaksur_gpudriver_info_s));
    (*info)->get_num_devices = get_num_devices;
    (*info)->check_p2p_comm = check_p2p_comm;
    (*info)->ipack = yaksuri_xei_ipack;
    (*info)->iunpack = yaksuri_xei_iunpack;
    (*info)->pup_is_supported = yaksuri_xei_pup_is_supported;
    (*info)->host_malloc = xe_host_malloc;
    (*info)->host_free = xe_host_free;
    (*info)->gpu_malloc = xe_gpu_malloc;
    (*info)->gpu_free = xe_gpu_free;
    (*info)->event_destroy = yaksuri_xei_event_destroy;
    (*info)->event_query = yaksuri_xei_event_query;
    (*info)->event_synchronize = yaksuri_xei_event_synchronize;
    (*info)->event_add_dependency = yaksuri_xei_event_add_dependency;
    (*info)->type_create = yaksuri_xei_type_create_hook;
    (*info)->type_free = yaksuri_xei_type_free_hook;
    (*info)->get_ptr_attr = yaksuri_xei_get_ptr_attr;
    (*info)->finalize = finalize_hook;

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}
