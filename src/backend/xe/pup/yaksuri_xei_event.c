/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#include <stdio.h>
#include <level_zero/ze_api.h>
#include "yaksi.h"
#include "yaksu.h"
#include "yaksuri_xei.h"

int yaksuri_xei_event_destroy(void *event)
{
    int rc = YAKSA_SUCCESS;

    if (event) {
        ze_result_t zerr = zeEventDestroy((ze_event_handle_t) event);
        YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);
    }

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}

int yaksuri_xei_event_query(void *event, int *completed)
{
    int rc = YAKSA_SUCCESS;

    if (event) {
        ze_result_t zerr = zeEventQueryStatus((ze_event_handle_t) event);
        if (zerr == ZE_RESULT_SUCCESS) {
            *completed = 1;
        } else if (zerr == ZE_RESULT_NOT_READY) {
            *completed = 0;
        } else {
            YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);
        }
    } else {
        *completed = 1;
    }

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}

int yaksuri_xei_event_synchronize(void *event)
{
    int rc = YAKSA_SUCCESS;

    if (event) {
        ze_result_t zerr = zeEventHostSynchronize((ze_event_handle_t) event, UINT32_MAX);
        YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);
    }

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}

int yaksuri_xei_event_add_dependency(void *event, int device)
{
    int rc = YAKSA_SUCCESS;

    ze_result_t zerr = zeCommandListAppendWaitOnEvents(yaksuri_xei_global.cl[device], 1,
                                                       (ze_event_handle_t *) & event);
    YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}
