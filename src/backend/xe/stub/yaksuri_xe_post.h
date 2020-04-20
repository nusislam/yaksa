/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#ifndef YAKSURI_XE_POST_H_INCLUDED
#define YAKSURI_XE_POST_H_INCLUDED

static int yaksuri_xe_init_hook(yaksur_gpudriver_info_s ** info) ATTRIBUTE((unused));
static int yaksuri_xe_init_hook(yaksur_gpudriver_info_s ** info)
{
    *info = NULL;

    return YAKSA_SUCCESS;
}

#endif /* YAKSURI_XE_POST_H_INCLUDED */
