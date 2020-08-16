/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#include "yaksi.h"
#include "yaksuri_cudai.h"
#include <assert.h>
#include <string.h>
#include <cuda.h>
#include <cuda_runtime_api.h>

static void *cuda_host_malloc(uintptr_t size)
{
    void *ptr = NULL;

    cudaError_t cerr = yaksuri_cudai_global.malloc_host(&ptr, size);
    YAKSURI_CUDAI_CUDA_ERR_CHECK(cerr);

    return ptr;
}

static void *cuda_gpu_malloc(uintptr_t size, int device)
{
    void *ptr = NULL;
    cudaError_t cerr;

    int cur_device;
    cerr = yaksuri_cudai_global.get_device(&cur_device);
    YAKSURI_CUDAI_CUDA_ERR_CHECK(cerr);

    cerr = yaksuri_cudai_global.set_device(device);
    YAKSURI_CUDAI_CUDA_ERR_CHECK(cerr);

    cerr = yaksuri_cudai_global.malloc(&ptr, size);
    YAKSURI_CUDAI_CUDA_ERR_CHECK(cerr);

    cerr = yaksuri_cudai_global.set_device(cur_device);
    YAKSURI_CUDAI_CUDA_ERR_CHECK(cerr);

    return ptr;
}

static void cuda_host_free(void *ptr)
{
    cudaError_t cerr = yaksuri_cudai_global.free_host(ptr);
    YAKSURI_CUDAI_CUDA_ERR_CHECK(cerr);
}

static void cuda_gpu_free(void *ptr)
{
    cudaError_t cerr = yaksuri_cudai_global.free(ptr);
    YAKSURI_CUDAI_CUDA_ERR_CHECK(cerr);
}

yaksuri_cudai_global_s yaksuri_cudai_global;

static int finalize_hook(void)
{
    int rc = YAKSA_SUCCESS;
    cudaError_t cerr;

    int cur_device;
    cerr = yaksuri_cudai_global.get_device(&cur_device);
    YAKSURI_CUDAI_CUDA_ERR_CHKANDJUMP(cerr, rc, fn_fail);

    for (int i = 0; i < yaksuri_cudai_global.ndevices; i++) {
        cerr = yaksuri_cudai_global.set_device(i);
        YAKSURI_CUDAI_CUDA_ERR_CHKANDJUMP(cerr, rc, fn_fail);

        cerr = yaksuri_cudai_global.stream_destroy(yaksuri_cudai_global.stream[i]);
        YAKSURI_CUDAI_CUDA_ERR_CHKANDJUMP(cerr, rc, fn_fail);

        free(yaksuri_cudai_global.p2p[i]);
    }
    free(yaksuri_cudai_global.stream);
    free(yaksuri_cudai_global.p2p);

    cerr = yaksuri_cudai_global.set_device(cur_device);
    YAKSURI_CUDAI_CUDA_ERR_CHKANDJUMP(cerr, rc, fn_fail);

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}

static int get_num_devices(int *ndevices)
{
    *ndevices = yaksuri_cudai_global.ndevices;

    return YAKSA_SUCCESS;
}

static int check_p2p_comm(int sdev, int ddev, bool * is_enabled)
{
#if CUDA_P2P == CUDA_P2P_ENABLED
    *is_enabled = yaksuri_cudai_global.p2p[sdev][ddev];
#elif CUDA_P2P == CUDA_P2P_CLIQUES
    if ((sdev + ddev) % 2)
        *is_enabled = 0;
    else
        *is_enabled = yaksuri_cudai_global.p2p[sdev][ddev];
#else
    *is_enabled = 0;
#endif

    return YAKSA_SUCCESS;
}

int yaksuri_cuda_init_hook(yaksur_gpudriver_info_s ** info)
{
    int rc = YAKSA_SUCCESS;
    cudaError_t cerr;

    rc = yaksuri_cudai_setup_cuda_fns();
    YAKSU_ERR_CHECK(rc, fn_fail);

    cerr = yaksuri_cudai_global.get_device_count(&yaksuri_cudai_global.ndevices);
    YAKSURI_CUDAI_CUDA_ERR_CHKANDJUMP(cerr, rc, fn_fail);

    yaksuri_cudai_global.stream = (cudaStream_t *)
        malloc(yaksuri_cudai_global.ndevices * sizeof(cudaStream_t));

    yaksuri_cudai_global.p2p = (bool **) malloc(yaksuri_cudai_global.ndevices * sizeof(bool *));
    for (int i = 0; i < yaksuri_cudai_global.ndevices; i++) {
        yaksuri_cudai_global.p2p[i] = (bool *) malloc(yaksuri_cudai_global.ndevices * sizeof(bool));
    }

    int cur_device;
    cerr = yaksuri_cudai_global.get_device(&cur_device);
    YAKSURI_CUDAI_CUDA_ERR_CHKANDJUMP(cerr, rc, fn_fail);

    for (int i = 0; i < yaksuri_cudai_global.ndevices; i++) {
        cerr = yaksuri_cudai_global.set_device(i);
        YAKSURI_CUDAI_CUDA_ERR_CHKANDJUMP(cerr, rc, fn_fail);

        cerr =
            yaksuri_cudai_global.stream_create_with_flags(&yaksuri_cudai_global.stream[i],
                                                          cudaStreamNonBlocking);
        YAKSURI_CUDAI_CUDA_ERR_CHKANDJUMP(cerr, rc, fn_fail);

        for (int j = 0; j < yaksuri_cudai_global.ndevices; j++) {
            if (i == j) {
                yaksuri_cudai_global.p2p[i][j] = 1;
            } else {
                int val;
                cerr = yaksuri_cudai_global.device_can_access_peer(&val, i, j);
                YAKSURI_CUDAI_CUDA_ERR_CHKANDJUMP(cerr, rc, fn_fail);

                if (val) {
                    cerr = yaksuri_cudai_global.device_enable_peer_access(j, 0);
                    if (cerr != cudaErrorPeerAccessAlreadyEnabled) {
                        YAKSURI_CUDAI_CUDA_ERR_CHKANDJUMP(cerr, rc, fn_fail);
                    }
                    yaksuri_cudai_global.p2p[i][j] = 1;
                } else {
                    yaksuri_cudai_global.p2p[i][j] = 0;
                }
            }
        }
    }

    cerr = yaksuri_cudai_global.set_device(cur_device);
    YAKSURI_CUDAI_CUDA_ERR_CHKANDJUMP(cerr, rc, fn_fail);

    *info = (yaksur_gpudriver_info_s *) malloc(sizeof(yaksur_gpudriver_info_s));
    (*info)->get_num_devices = get_num_devices;
    (*info)->check_p2p_comm = check_p2p_comm;
    (*info)->ipack = yaksuri_cudai_ipack;
    (*info)->iunpack = yaksuri_cudai_iunpack;
    (*info)->pup_is_supported = yaksuri_cudai_pup_is_supported;
    (*info)->host_malloc = cuda_host_malloc;
    (*info)->host_free = cuda_host_free;
    (*info)->gpu_malloc = cuda_gpu_malloc;
    (*info)->gpu_free = cuda_gpu_free;
    (*info)->event_destroy = yaksuri_cudai_event_destroy;
    (*info)->event_query = yaksuri_cudai_event_query;
    (*info)->event_synchronize = yaksuri_cudai_event_synchronize;
    (*info)->event_add_dependency = yaksuri_cudai_event_add_dependency;
    (*info)->type_create = yaksuri_cudai_type_create_hook;
    (*info)->type_free = yaksuri_cudai_type_free_hook;
    (*info)->get_ptr_attr = yaksuri_cudai_get_ptr_attr;
    (*info)->finalize = finalize_hook;

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}
