/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#ifndef YAKSURI_XEI_MD_H_INCLUDED
#define YAKSURI_XEI_MD_H_INCLUDED

#include <stdint.h>

typedef struct yaksuri_xei_md_s {
    union {
        struct {
            int count;
            long stride;
            struct yaksuri_xei_md_s *child;
        } contig;
        struct {
            struct yaksuri_xei_md_s *child;
        } dup;
        struct {
            struct yaksuri_xei_md_s *child;
        } resized;
        struct {
            int count;
            int blocklength;
            long stride;
            struct yaksuri_xei_md_s *child;
        } hvector;
        struct {
            int count;
            int blocklength;
            long *array_of_displs;
            struct yaksuri_xei_md_s *child;
        } blkhindx;
        struct {
            int count;
            int *array_of_blocklengths;
            long *array_of_displs;
            struct yaksuri_xei_md_s *child;
        } hindexed;
    } u;

    unsigned long extent;
    unsigned long num_elements;
} yaksuri_xei_md_s;

#endif /* YAKSURI_XEI_H_INCLUDED */
