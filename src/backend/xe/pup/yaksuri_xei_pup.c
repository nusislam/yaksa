/*
* Copyright (C) by Argonne National Laboratory
*     See COPYRIGHT in top-level directory
*/

#include <assert.h>
#include <level_zero/ze_api.h>
#include "yaksi.h"
#include "yaksuri_xei.h"

#define THREAD_BLOCK_SIZE  (256)
#define MAX_GRIDSZ_X       ((1ULL << 31) - 1)
#define MAX_GRIDSZ_Y       (65535)
#define MAX_GRIDSZ_Z       (65535)

static int get_thread_block_dims(uint64_t count, yaksi_type_s * type, int *n_threads,
                                 int *n_blocks_x, int *n_blocks_y, int *n_blocks_z)
{
    int rc = YAKSA_SUCCESS;
    yaksuri_xei_type_s *xe_type = (yaksuri_xei_type_s *) type->backend.xe.priv;

    *n_threads = THREAD_BLOCK_SIZE;
    uint64_t n_blocks = count * xe_type->num_elements / THREAD_BLOCK_SIZE;
    n_blocks += ! !(count * xe_type->num_elements % THREAD_BLOCK_SIZE);

    if (n_blocks <= MAX_GRIDSZ_X) {
        *n_blocks_x = (int) n_blocks;
        *n_blocks_y = 1;
        *n_blocks_z = 1;
    } else if (n_blocks <= MAX_GRIDSZ_X * MAX_GRIDSZ_Y) {
        *n_blocks_x = YAKSU_CEIL(n_blocks, MAX_GRIDSZ_Y);
        *n_blocks_y = YAKSU_CEIL(n_blocks, (*n_blocks_x));
        *n_blocks_z = 1;
    } else {
        int n_blocks_xy = YAKSU_CEIL(n_blocks, MAX_GRIDSZ_Z);
        *n_blocks_x = YAKSU_CEIL(n_blocks_xy, MAX_GRIDSZ_Y);
        *n_blocks_y = YAKSU_CEIL(n_blocks_xy, (*n_blocks_x));
        *n_blocks_z = YAKSU_CEIL(n_blocks, (uintptr_t) (*n_blocks_x) * (*n_blocks_y));
    }

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}

int yaksuri_xei_pup_is_supported(yaksi_type_s * type, bool * is_supported)
{
    int rc = YAKSA_SUCCESS;
    yaksuri_xei_type_s *xe_type = (yaksuri_xei_type_s *) type->backend.xe.priv;

    if (type->is_contig || xe_type->pack)
        *is_supported = true;
    else
        *is_supported = false;

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}

int yaksuri_xei_ipack(const void *inbuf, void *outbuf, uintptr_t count, yaksi_type_s * type,
                      void *device_tmpbuf, int target, void **event)
{
    int rc = YAKSA_SUCCESS;
    yaksuri_xei_type_s *xe_type = (yaksuri_xei_type_s *) type->backend.xe.priv;
    ze_result_t zerr;

    struct {
        ze_memory_allocation_properties_t prop;
        ze_device_handle_t device;
    } outattr, inattr;

    zerr = zeDriverGetMemAllocProperties(yaksuri_xei_global.driver, (char *) inbuf + type->true_lb,
                                         &inattr.prop, &inattr.device);
    YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

    zerr =
        zeDriverGetMemAllocProperties(yaksuri_xei_global.driver, outbuf, &outattr.prop,
                                      &outattr.device);
    YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

    if (*event == NULL) {
        zerr = zeEventCreate(yaksuri_xei_global.ep, NULL, (ze_event_handle_t *) event);
        YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);
    }

    /* shortcut for contiguous types */
    if (type->is_contig) {
        /* xe performance is optimized when we synchronize on the
         * source buffer's GPU */
        zerr = zeCommandListAppendMemoryCopy(yaksuri_xei_global.cl[target], outbuf, inbuf,
                                             count * type->size, NULL);
        YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);
    } else {
        rc = yaksuri_xei_md_alloc(type);
        YAKSU_ERR_CHECK(rc, fn_fail);

        int n_threads;
        int n_blocks_x, n_blocks_y, n_blocks_z;
        rc = get_thread_block_dims(count, type, &n_threads, &n_blocks_x, &n_blocks_y, &n_blocks_z);
        YAKSU_ERR_CHECK(rc, fn_fail);

        /* zerr = zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &inbuf); */
        /* YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */
        /* zerr = zeKernelSetArgumentValue(kernel, 1, sizeof(void *), &outbuf); */
        /* YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */
        /* zerr = zeKernelSetArgumentValue(kernel, 2, sizeof(uintptr_t), &count); */
        /* YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */
        /* zerr = zeKernelSetArgumentValue(kernel, 3, sizeof(yaksuri_xei_md_s *), &xe_type->md); */
        /* YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */

        /* FIXME: are these grid sizes? */
        ze_group_count_t launchArgs = { 1, 1, 1 };

        if ((inattr.prop.type == ZE_MEMORY_TYPE_SHARED &&
             outattr.prop.type == ZE_MEMORY_TYPE_SHARED) ||
            (inattr.prop.type == ZE_MEMORY_TYPE_DEVICE &&
             outattr.prop.type == ZE_MEMORY_TYPE_SHARED) ||
            (inattr.prop.type == ZE_MEMORY_TYPE_DEVICE && outattr.prop.type == ZE_MEMORY_TYPE_DEVICE
             && inattr.device == outattr.device)) {
            /* zerr = zeCommandListAppendLaunchKernel(yaksuri_xei_global.cl[target], */
            /*                                        xe_type->pack, &launchArgs, NULL, 0, NULL); */
            /* YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */
        } else if (inattr.prop.type == ZE_MEMORY_TYPE_SHARED &&
                   outattr.prop.type == ZE_MEMORY_TYPE_DEVICE) {
            /* zerr = zeCommandListAppendLaunchKernel(yaksuri_xei_global.cl[target], */
            /*                                        xe_type->pack, &launchArgs, NULL, 0, NULL); */
            /* YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */
        } else if ((outattr.prop.type == ZE_MEMORY_TYPE_DEVICE && inattr.device != outattr.device)
                   || (outattr.prop.type == ZE_MEMORY_TYPE_HOST)) {
            assert(inattr.prop.type == ZE_MEMORY_TYPE_DEVICE);

            /* zerr = zeCommandListAppendLaunchKernel(yaksuri_xei_global.cl[target], */
            /*                                        xe_type->pack, &launchArgs, NULL, 0, NULL); */
            /* YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */

            zerr =
                zeCommandListAppendMemoryCopy(yaksuri_xei_global.cl[target], outbuf, device_tmpbuf,
                                              count * type->size, NULL);
            YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);
        } else {
            rc = YAKSA_ERR__INTERNAL;
            goto fn_fail;
        }
    }

    zerr =
        zeCommandListAppendSignalEvent(yaksuri_xei_global.cl[target], (ze_event_handle_t) * event);
    YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}

int yaksuri_xei_iunpack(const void *inbuf, void *outbuf, uintptr_t count, yaksi_type_s * type,
                        void *device_tmpbuf, int target, void **event)
{
    int rc = YAKSA_SUCCESS;
    yaksuri_xei_type_s *xe_type = (yaksuri_xei_type_s *) type->backend.xe.priv;
    ze_result_t zerr;

    struct {
        ze_memory_allocation_properties_t prop;
        ze_device_handle_t device;
    } outattr, inattr;

    zerr =
        zeDriverGetMemAllocProperties(yaksuri_xei_global.driver, inbuf, &inattr.prop,
                                      &inattr.device);
    YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

    zerr = zeDriverGetMemAllocProperties(yaksuri_xei_global.driver, (char *) outbuf + type->true_lb,
                                         &outattr.prop, &outattr.device);
    YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

    if (*event == NULL) {
        zerr = zeEventCreate(yaksuri_xei_global.ep, NULL, (ze_event_handle_t *) event);
        YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);
    }

    /* shortcut for contiguous types */
    if (type->is_contig) {
        /* xe performance is optimized when we synchronize on the
         * source buffer's GPU */
        zerr = zeCommandListAppendMemoryCopy(yaksuri_xei_global.cl[target], outbuf, inbuf,
                                             count * type->size, NULL);
        YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);
    } else {
        rc = yaksuri_xei_md_alloc(type);
        YAKSU_ERR_CHECK(rc, fn_fail);

        int n_threads;
        int n_blocks_x, n_blocks_y, n_blocks_z;
        rc = get_thread_block_dims(count, type, &n_threads, &n_blocks_x, &n_blocks_y, &n_blocks_z);
        YAKSU_ERR_CHECK(rc, fn_fail);

        /* zerr = zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &inbuf); */
        /* YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */
        /* zerr = zeKernelSetArgumentValue(kernel, 1, sizeof(void *), &outbuf); */
        /* YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */
        /* zerr = zeKernelSetArgumentValue(kernel, 2, sizeof(uintptr_t), &count); */
        /* YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */
        /* zerr = zeKernelSetArgumentValue(kernel, 3, sizeof(yaksuri_xei_md_s *), &xe_type->md); */
        /* YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */

        /* FIXME: are these grid sizes? */
        ze_group_count_t launchArgs = { 1, 1, 1 };

        if ((inattr.prop.type == ZE_MEMORY_TYPE_SHARED &&
             outattr.prop.type == ZE_MEMORY_TYPE_SHARED) ||
            (inattr.prop.type == ZE_MEMORY_TYPE_DEVICE &&
             outattr.prop.type == ZE_MEMORY_TYPE_SHARED) ||
            (inattr.prop.type == ZE_MEMORY_TYPE_DEVICE && outattr.prop.type == ZE_MEMORY_TYPE_DEVICE
             && inattr.device == outattr.device)) {
            /* zerr = zeCommandListAppendLaunchKernel(yaksuri_xei_global.cl[target], */
            /*                                        xe_type->unpack, &launchArgs, NULL, 0, NULL); */
            /* YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */
        } else if (inattr.prop.type == ZE_MEMORY_TYPE_SHARED &&
                   outattr.prop.type == ZE_MEMORY_TYPE_DEVICE) {
            /* zerr = zeCommandListAppendLaunchKernel(yaksuri_xei_global.cl[target], */
            /*                                        xe_type->unpack, &launchArgs, NULL, 0, NULL); */
            /* YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */
        } else if ((outattr.prop.type == ZE_MEMORY_TYPE_DEVICE && inattr.device != outattr.device)
                   || (outattr.prop.type == ZE_MEMORY_TYPE_HOST)) {
            assert(inattr.prop.type == ZE_MEMORY_TYPE_DEVICE);

            zerr =
                zeCommandListAppendMemoryCopy(yaksuri_xei_global.cl[target], device_tmpbuf, inbuf,
                                              count * type->size, NULL);
            YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

            /* zerr = zeCommandListAppendLaunchKernel(yaksuri_xei_global.cl[target], */
            /*                                        xe_type->unpack, &launchArgs, NULL, 0, NULL); */
            /* YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */
        } else {
            rc = YAKSA_ERR__INTERNAL;
            goto fn_fail;
        }
    }

    zerr =
        zeCommandListAppendSignalEvent(yaksuri_xei_global.cl[target], (ze_event_handle_t) * event);
    YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}
