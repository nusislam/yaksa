/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#include "yaksi.h"
#include "yaksu.h"
#include "yaksuri_xei.h"
#include <assert.h>
#include <level_zero/ze_api.h>
#include <string.h>

static yaksuri_xei_md_s *type_to_md(yaksi_type_s * type)
{
    yaksuri_xei_type_s *xe = type->backend.xe.priv;

    return xe->md;
}

int yaksuri_xei_md_alloc(yaksi_type_s * type)
{
    int rc = YAKSA_SUCCESS;
    yaksuri_xei_type_s *xe = (yaksuri_xei_type_s *) type->backend.xe.priv;
    ze_result_t zerr;

    ze_host_mem_alloc_desc_t host_desc;
    host_desc.flags = ZE_HOST_MEM_ALLOC_FLAG_DEFAULT;
    host_desc.version = ZE_HOST_MEM_ALLOC_DESC_VERSION_CURRENT;

    ze_device_mem_alloc_desc_t device_desc;
    device_desc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_DEFAULT;
    device_desc.ordinal = 0;
    device_desc.version = ZE_DEVICE_MEM_ALLOC_DESC_VERSION_CURRENT;

    pthread_mutex_lock(&xe->mdmutex);

    assert(type->kind != YAKSI_TYPE_KIND__STRUCT);
    assert(type->kind != YAKSI_TYPE_KIND__SUBARRAY);

    /* if the metadata is already allocated, return */
    if (xe->md) {
        goto fn_exit;
    } else {
        zerr = zeDriverAllocSharedMem(yaksuri_xei_global.driver, &device_desc, &host_desc,
                                      sizeof(yaksuri_xei_md_s), 1, NULL, (void **) &xe->md);
        YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);
    }

    switch (type->kind) {
        case YAKSI_TYPE_KIND__BUILTIN:
            break;

        case YAKSI_TYPE_KIND__DUP:
            rc = yaksuri_xei_md_alloc(type->u.dup.child);
            YAKSU_ERR_CHECK(rc, fn_fail);
            xe->md->u.dup.child = type_to_md(type->u.dup.child);
            break;

        case YAKSI_TYPE_KIND__RESIZED:
            rc = yaksuri_xei_md_alloc(type->u.resized.child);
            YAKSU_ERR_CHECK(rc, fn_fail);
            xe->md->u.resized.child = type_to_md(type->u.resized.child);
            break;

        case YAKSI_TYPE_KIND__HVECTOR:
            xe->md->u.hvector.count = type->u.hvector.count;
            xe->md->u.hvector.blocklength = type->u.hvector.blocklength;
            xe->md->u.hvector.stride = type->u.hvector.stride;

            rc = yaksuri_xei_md_alloc(type->u.hvector.child);
            YAKSU_ERR_CHECK(rc, fn_fail);
            xe->md->u.hvector.child = type_to_md(type->u.hvector.child);

            break;

        case YAKSI_TYPE_KIND__BLKHINDX:
            xe->md->u.blkhindx.count = type->u.blkhindx.count;
            xe->md->u.blkhindx.blocklength = type->u.blkhindx.blocklength;

            zerr = zeDriverAllocSharedMem(yaksuri_xei_global.driver, &device_desc, &host_desc,
                                          type->u.blkhindx.count * sizeof(intptr_t), 1, NULL,
                                          (void **) &xe->md->u.blkhindx.array_of_displs);
            YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

            memcpy(xe->md->u.blkhindx.array_of_displs, type->u.blkhindx.array_of_displs,
                   type->u.blkhindx.count * sizeof(intptr_t));

            rc = yaksuri_xei_md_alloc(type->u.blkhindx.child);
            YAKSU_ERR_CHECK(rc, fn_fail);
            xe->md->u.blkhindx.child = type_to_md(type->u.blkhindx.child);

            break;

        case YAKSI_TYPE_KIND__HINDEXED:
            xe->md->u.hindexed.count = type->u.hindexed.count;

            zerr = zeDriverAllocSharedMem(yaksuri_xei_global.driver, &device_desc, &host_desc,
                                          type->u.hindexed.count * sizeof(intptr_t), 1, NULL,
                                          (void **) &xe->md->u.hindexed.array_of_displs);
            YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

            memcpy(xe->md->u.hindexed.array_of_displs, type->u.hindexed.array_of_displs,
                   type->u.hindexed.count * sizeof(intptr_t));

            zerr = zeDriverAllocSharedMem(yaksuri_xei_global.driver, &device_desc, &host_desc,
                                          type->u.hindexed.count * sizeof(int), 1, NULL,
                                          (void **) &xe->md->u.hindexed.array_of_blocklengths);
            YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

            memcpy(xe->md->u.hindexed.array_of_blocklengths,
                   type->u.hindexed.array_of_blocklengths, type->u.hindexed.count * sizeof(int));

            rc = yaksuri_xei_md_alloc(type->u.hindexed.child);
            YAKSU_ERR_CHECK(rc, fn_fail);
            xe->md->u.hindexed.child = type_to_md(type->u.hindexed.child);

            break;

        case YAKSI_TYPE_KIND__CONTIG:
            xe->md->u.contig.count = type->u.contig.count;
            xe->md->u.contig.stride = type->u.contig.child->extent;

            rc = yaksuri_xei_md_alloc(type->u.contig.child);
            YAKSU_ERR_CHECK(rc, fn_fail);
            xe->md->u.contig.child = type_to_md(type->u.contig.child);

            break;

        default:
            assert(0);
    }

    xe->md->extent = type->extent;
    xe->md->num_elements = xe->num_elements;

  fn_exit:
    pthread_mutex_unlock(&xe->mdmutex);
    return rc;
  fn_fail:
    goto fn_exit;
}
