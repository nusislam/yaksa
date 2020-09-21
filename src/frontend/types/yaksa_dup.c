/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#include "yaksi.h"
#include "yaksu.h"
#include <stdlib.h>
#include <assert.h>

int yaksi_type_create_dup(yaksi_type_s * intype, yaksi_type_s ** newtype)
{
    int rc = YAKSA_SUCCESS;

    yaksu_atomic_incr(&intype->refcount);
    *newtype = intype;

    return rc;
}

int yaksa_type_create_dup(yaksa_type_t oldtype, yaksa_info_t info, yaksa_type_t * newtype)
{
    int rc = YAKSA_SUCCESS;

    assert(yaksu_atomic_load(&yaksi_is_initialized));

    yaksi_type_s *intype;
    rc = yaksi_type_get(oldtype, &intype);
    YAKSU_ERR_CHECK(rc, fn_fail);

    yaksi_type_s *outtype;
    rc = yaksi_type_create_dup(intype, &outtype);
    YAKSU_ERR_CHECK(rc, fn_fail);

    yaksu_handle_t obj_id;
    yaksa_context_t ctx_id;
    YAKSI_TYPE_DECODE(oldtype, ctx_id, obj_id);

    yaksi_context_s *ctx;
    rc = yaksu_handle_pool_elem_get(yaksi_global.context_handle_pool, ctx_id, (const void **) &ctx);
    YAKSU_ERR_CHECK(rc, fn_fail);

    rc = yaksi_type_handle_alloc(ctx, outtype, newtype);
    YAKSU_ERR_CHECK(rc, fn_fail);

  fn_exit:
    return rc;
  fn_fail:
    goto fn_exit;
}
