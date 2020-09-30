/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#include <stdlib.h>
#include <assert.h>
#include "yaksa.h"
#include "yaksi.h"
#include "yaksu.h"
#include "yaksuri.h"

yaksuri_global_s yaksuri_global;

static void *pool_host_malloc(uintptr_t size, void *state)
{
    yaksuri_gpudriver_id_e id;
    void *ret = NULL;
    int rc = YAKSA_SUCCESS;

    for (id = YAKSURI_GPUDRIVER_ID__UNSET; id < YAKSURI_GPUDRIVER_ID__LAST; id++) {
        if (id == YAKSURI_GPUDRIVER_ID__UNSET)
            continue;

        if (yaksuri_global.gpudriver[id].info) {
            if (&yaksuri_global.gpudriver[id].tmpbuf_pool.host == state) {
                /* this is a host malloc */
                ret = yaksuri_global.gpudriver[id].info->host_malloc(size);
                break;
            }
        }
    }

    return ret;
}

static void pool_host_free(void *buf)
{
    yaksuri_global.gpudriver[id].info->host_free(buf);
}

static void *pool_gpu_malloc(uintptr_t size, void *state)
{
    yaksuri_gpudriver_id_e id;
    void *ret = NULL;
    int rc = YAKSA_SUCCESS;

    for (id = YAKSURI_GPUDRIVER_ID__UNSET; id < YAKSURI_GPUDRIVER_ID__LAST; id++) {
        if (id == YAKSURI_GPUDRIVER_ID__UNSET)
            continue;

        if (yaksuri_global.gpudriver[id].info) {
            int ndev;
            rc = yaksuri_global.gpudriver[id].info->get_num_devices(&ndev);
            assert(rc);

            uintptr_t dev_first = (uintptr_t) &yaksuri_global.gpudriver[id].tmpbuf_pool.device[0];
            uintptr_t dev_last = (uintptr_t) &yaksuri_global.gpudriver[id].tmpbuf_pool.device[ndev - 1];
            if (((uintptr_t) state >= dev_first) && ((uintptr_t) state <= dev_last)) {
                int dev = ((uintptr_t) state - dev_first) / sizeof(yaksu_buffer_pool_s);
                ret = yaksuri_global.gpudriver[id].info->gpu_malloc(size, dev);
                break;
            }
        }
    }

    return ret;
}

static void pool_gpu_free(void *buf)
{
    yaksuri_global.gpudriver[id].info->gpu_free(buf);
}

int yaksur_init_hook(void)
{
    int rc = YAKSA_SUCCESS;
    yaksuri_gpudriver_id_e id;

    rc = yaksuri_seq_init_hook();
    YAKSU_ERR_CHECK(rc, fn_fail);

    /* CUDA hooks */
    id = YAKSURI_GPUDRIVER_ID__CUDA;
    yaksuri_global.gpudriver[id].info = NULL;
    rc = yaksuri_cuda_init_hook(&yaksuri_global.gpudriver[id].info);
    YAKSU_ERR_CHECK(rc, fn_fail);


    /* final setup for all backends */
    for (id = YAKSURI_GPUDRIVER_ID__UNSET; id < YAKSURI_GPUDRIVER_ID__LAST; id++) {
        if (id == YAKSURI_GPUDRIVER_ID__UNSET)
            continue;

        if (yaksuri_global.gpudriver[id].info) {
            rc = yaksu_buffer_pool_alloc(YAKSURI_TMPBUF_ELEM_SIZE, 1, YAKSURI_TMPBUF_NUM_ELEMS,
                                         pool_host_malloc, pool_host_free,
                                         &yaksuri_global.gpudriver[id].tmpbuf_pool.host,
                                         &yaksuri_global.gpudriver[id].tmpbuf_pool.host);
            YAKSU_ERR_CHECK(rc, fn_fail);

            int ndevices;
            rc = yaksuri_global.gpudriver[id].info->get_num_devices(&ndevices);
            YAKSU_ERR_CHECK(rc, fn_fail);

            yaksuri_global.gpudriver[id].device = (yaksuri_slab_s *)
                malloc(ndevices * sizeof(yaksuri_slab_s));
            for (int i = 0; i < ndevices; i++) {
                rc = yaksu_buffer_pool_alloc(YAKSURI_TMPBUF_ELEM_SIZE, 1, YAKSURI_TMPBUF_NUM_ELEMS,
                                             pool_gpu_malloc, pool_gpu_free,
                                             &yaksuri_global.gpudriver[id].tmpbuf_pool.device[i],
                                             &yaksuri_global.gpudriver[id].tmpbuf_pool.device[i]);
                YAKSU_ERR_CHECK(rc, fn_fail);
            }
        }
    }

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}

int yaksur_finalize_hook(void)
{
    int rc = YAKSA_SUCCESS;
    yaksuri_gpudriver_id_e id;

    rc = yaksuri_seq_finalize_hook();
    YAKSU_ERR_CHECK(rc, fn_fail);

    for (id = YAKSURI_GPUDRIVER_ID__UNSET; id < YAKSURI_GPUDRIVER_ID__LAST; id++) {
        if (id == YAKSURI_GPUDRIVER_ID__UNSET)
            continue;

        if (yaksuri_global.gpudriver[id].info) {
            rc = yaksu_buffer_pool_free(yaksuri_global.gpudriver[id].tmpbuf_pool.host);
            YAKSU_ERR_CHECK(rc, fn_fail);

            int ndevices;
            rc = yaksuri_global.gpudriver[id].info->get_num_devices(&ndevices);
            YAKSU_ERR_CHECK(rc, fn_fail);

            for (int i = 0; i < ndevices; i++) {
                rc = yaksu_buffer_pool_free(yaksuri_global.gpudriver[id].tmpbuf_pool.device[i]);
                YAKSU_ERR_CHECK(rc, fn_fail);
            }
            free(yaksuri_global.gpudriver[id].tmpbuf_pool.device);

            rc = yaksuri_global.gpudriver[id].info->finalize();
            YAKSU_ERR_CHECK(rc, fn_fail);
            free(yaksuri_global.gpudriver[id].info);
        }
    }

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}

int yaksur_type_create_hook(yaksi_type_s * type)
{
    int rc = YAKSA_SUCCESS;

    rc = yaksuri_seq_type_create_hook(type);
    YAKSU_ERR_CHECK(rc, fn_fail);

    for (yaksuri_gpudriver_id_e id = YAKSURI_GPUDRIVER_ID__UNSET;
         id < YAKSURI_GPUDRIVER_ID__LAST; id++) {
        if (id == YAKSURI_GPUDRIVER_ID__UNSET)
            continue;

        if (yaksuri_global.gpudriver[id].info) {
            rc = yaksuri_global.gpudriver[id].info->type_create(type);
            YAKSU_ERR_CHECK(rc, fn_fail);
        }
    }

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}

int yaksur_type_free_hook(yaksi_type_s * type)
{
    int rc = YAKSA_SUCCESS;

    rc = yaksuri_seq_type_free_hook(type);
    YAKSU_ERR_CHECK(rc, fn_fail);

    for (yaksuri_gpudriver_id_e id = YAKSURI_GPUDRIVER_ID__UNSET;
         id < YAKSURI_GPUDRIVER_ID__LAST; id++) {
        if (id == YAKSURI_GPUDRIVER_ID__UNSET)
            continue;

        if (yaksuri_global.gpudriver[id].info) {
            rc = yaksuri_global.gpudriver[id].info->type_free(type);
            YAKSU_ERR_CHECK(rc, fn_fail);
        }
    }

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}

int yaksur_request_create_hook(yaksi_request_s * request)
{
    int rc = YAKSA_SUCCESS;

    request->backend.priv = malloc(sizeof(yaksuri_request_s));
    yaksuri_request_s *backend = (yaksuri_request_s *) request->backend.priv;

    backend->kind = YAKSURI_REQUEST_KIND__UNSET;

    return rc;
}

int yaksur_request_free_hook(yaksi_request_s * request)
{
    int rc = YAKSA_SUCCESS;
    yaksuri_request_s *backend = (yaksuri_request_s *) request->backend.priv;
    yaksuri_gpudriver_id_e id = backend->gpudriver_id;

    if (backend->event) {
        assert(yaksuri_global.gpudriver[id].info);
        rc = yaksuri_global.gpudriver[id].info->event_destroy(backend->event);
        YAKSU_ERR_CHECK(rc, fn_fail);
    }

    free(backend);

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}

int yaksur_info_create_hook(yaksi_info_s * info)
{
    int rc = YAKSA_SUCCESS;

    rc = yaksuri_seq_info_create_hook(info);
    YAKSU_ERR_CHECK(rc, fn_fail);

    for (yaksuri_gpudriver_id_e id = YAKSURI_GPUDRIVER_ID__UNSET;
         id < YAKSURI_GPUDRIVER_ID__LAST; id++) {
        if (id == YAKSURI_GPUDRIVER_ID__UNSET)
            continue;

        if (yaksuri_global.gpudriver[id].info) {
            rc = yaksuri_global.gpudriver[id].info->info_create(info);
            YAKSU_ERR_CHECK(rc, fn_fail);
        }
    }

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}

int yaksur_info_free_hook(yaksi_info_s * info)
{
    int rc = YAKSA_SUCCESS;

    rc = yaksuri_seq_info_free_hook(info);
    YAKSU_ERR_CHECK(rc, fn_fail);

    for (yaksuri_gpudriver_id_e id = YAKSURI_GPUDRIVER_ID__UNSET;
         id < YAKSURI_GPUDRIVER_ID__LAST; id++) {
        if (id == YAKSURI_GPUDRIVER_ID__UNSET)
            continue;

        if (yaksuri_global.gpudriver[id].info) {
            rc = yaksuri_global.gpudriver[id].info->info_free(info);
            YAKSU_ERR_CHECK(rc, fn_fail);
        }
    }

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}

int yaksur_info_keyval_append(yaksi_info_s * info, const char *key, const void *val,
                              unsigned int vallen)
{
    int rc = YAKSA_SUCCESS;

    rc = yaksuri_seq_info_keyval_append(info, key, val, vallen);
    YAKSU_ERR_CHECK(rc, fn_fail);

    for (yaksuri_gpudriver_id_e id = YAKSURI_GPUDRIVER_ID__UNSET;
         id < YAKSURI_GPUDRIVER_ID__LAST; id++) {
        if (id == YAKSURI_GPUDRIVER_ID__UNSET)
            continue;

        if (yaksuri_global.gpudriver[id].info) {
            rc = yaksuri_global.gpudriver[id].info->info_keyval_append(info, key, val, vallen);
            YAKSU_ERR_CHECK(rc, fn_fail);
        }
    }

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}
