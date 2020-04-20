/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#include <stdlib.h>
#include <assert.h>
#include "yaksi.h"
#include "yaksu.h"
#include "yaksuri_xei.h"

static uintptr_t get_num_elements(yaksi_type_s * type)
{
    switch (type->kind) {
        case YAKSI_TYPE_KIND__BUILTIN:
            return type->num_contig;

        case YAKSI_TYPE_KIND__CONTIG:
            return type->u.contig.count * get_num_elements(type->u.contig.child);

        case YAKSI_TYPE_KIND__DUP:
            return get_num_elements(type->u.dup.child);

        case YAKSI_TYPE_KIND__RESIZED:
            return get_num_elements(type->u.resized.child);

        case YAKSI_TYPE_KIND__HVECTOR:
            return type->u.hvector.count * type->u.hvector.blocklength *
                get_num_elements(type->u.hvector.child);

        case YAKSI_TYPE_KIND__BLKHINDX:
            return type->u.blkhindx.count * type->u.blkhindx.blocklength *
                get_num_elements(type->u.blkhindx.child);

        case YAKSI_TYPE_KIND__HINDEXED:
            {
                uintptr_t nelems = 0;
                for (int i = 0; i < type->u.hindexed.count; i++)
                    nelems += type->u.hindexed.array_of_blocklengths[i];
                nelems *= get_num_elements(type->u.hindexed.child);
                return nelems;
            }

        default:
            return 0;
    }
}

int yaksuri_xei_type_create_hook(yaksi_type_s * type)
{
    int rc = YAKSA_SUCCESS;

    type->backend.xe.priv = malloc(sizeof(yaksuri_xei_type_s));
    yaksuri_xei_type_s *xe = (yaksuri_xei_type_s *) type->backend.xe.priv;

    xe->num_elements = get_num_elements(type);
    xe->md = NULL;
    pthread_mutex_init(&xe->mdmutex, NULL);

    rc = yaksuri_xei_populate_pupfns(type);
    YAKSU_ERR_CHECK(rc, fn_fail);

    /* FIXME: the below code is very broken; L0 seems to require the
     * kernel to be compiled as many times as the number of
     * devices. */
    /* if (xe->pack) { */
    /*     ze_module_desc_t desc; */
    /*     desc.format = ZE_MODULE_FORMAT_IL_SPIRV; */
    /*     desc.inputSize = 0; */
    /*     desc.pInputModule = NULL; */
    /*     ze_result_t zerr = zeModuleCreate(device, &desc, &xe->module, NULL); */
    /*     YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */

    /*     ze_kernel_desc_t kdesc; */

    /*     kdesc.pKernelName = xe->pack; */
    /*     zerr = zeKernelCreate(xe->module, &kdesc, &xe->pack_kernel); */
    /*     YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */

    /*     kdesc.pKernelName = xe->unpack; */
    /*     zerr = zeKernelCreate(xe->module, &kdesc, &xe->unpack_kernel); */
    /*     YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */
    /* } */

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}

int yaksuri_xei_type_free_hook(yaksi_type_s * type)
{
    int rc = YAKSA_SUCCESS;
    yaksuri_xei_type_s *xe = (yaksuri_xei_type_s *) type->backend.xe.priv;
    ze_result_t zerr;

    /* if (xe->pack) { */
    /*     zerr = zeKernelDestroy(xe->pack_kernel); */
    /*     YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */

    /*     zerr = zeKernelDestroy(xe->unpack_kernel); */
    /*     YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */

    /*     zerr = zeModuleDestroy(xe->module); */
    /*     YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail); */
    /* } */

    pthread_mutex_destroy(&xe->mdmutex);
    if (xe->md) {
        if (type->kind == YAKSI_TYPE_KIND__BLKHINDX) {
            assert(xe->md->u.blkhindx.array_of_displs);
            zerr =
                zeDriverFreeMem(yaksuri_xei_global.driver,
                                (void *) xe->md->u.blkhindx.array_of_displs);
            YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);
        } else if (type->kind == YAKSI_TYPE_KIND__HINDEXED) {
            assert(xe->md->u.hindexed.array_of_displs);
            zerr =
                zeDriverFreeMem(yaksuri_xei_global.driver,
                                (void *) xe->md->u.hindexed.array_of_displs);
            YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);

            assert(xe->md->u.hindexed.array_of_blocklengths);
            zerr =
                zeDriverFreeMem(yaksuri_xei_global.driver,
                                (void *) xe->md->u.hindexed.array_of_blocklengths);
            YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);
        }

        zerr = zeDriverFreeMem(yaksuri_xei_global.driver, xe->md);
        YAKSURI_XEI_XE_ERR_CHKANDJUMP(zerr, rc, fn_fail);
    }

    free(xe);

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}
