/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#include "yaksi.h"
#include "yaksu.h"
#include "yaksuri_xei.h"
#include <level_zero/ze_api.h>

int yaksuri_xei_get_ptr_attr(const void *buf, yaksur_ptr_attr_s * ptrattr)
{
    int rc = YAKSA_SUCCESS;

    struct {
        ze_memory_allocation_properties_t prop;
        ze_device_handle_t device;
    } attr;

    ze_result_t zerr = zeDriverGetMemAllocProperties(yaksuri_xei_global.driver, buf,
                                                     &attr.prop, &attr.device);
    YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

    if (attr.prop.type == ZE_MEMORY_TYPE_UNKNOWN) {
        ptrattr->type = YAKSUR_PTR_TYPE__UNREGISTERED_HOST;
        ptrattr->device = -1;
    } else if (attr.prop.type == ZE_MEMORY_TYPE_HOST) {
        ptrattr->type = YAKSUR_PTR_TYPE__REGISTERED_HOST;
        ptrattr->device = -1;
    } else {
        ptrattr->type = YAKSUR_PTR_TYPE__GPU;
        ptrattr->device = -1;
        for (int i = 0; i < yaksuri_xei_global.ndevices; i++)
            if (yaksuri_xei_global.device[i] == attr.device)
                ptrattr->device = i;
    }

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}
