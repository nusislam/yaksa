#! /usr/bin/env python
##
## Copyright (C) by Argonne National Laboratory
##     See COPYRIGHT in top-level directory
##

import sys
import argparse
import subprocess

sys.path.append('maint/')
import yutils

sys.path.append('src/backend/')
import gencomm


########################################################################################
##### Global variables
########################################################################################

num_paren_open = 0
blklens = [ "generic" ]
builtin_types = [ "char", "int", "short", "long", "long long", "int8_t", "int16_t", \
                  "int32_t", "int64_t", "float", "double" ]
builtin_maps = {
    "YAKSA_TYPE__UNSIGNED_CHAR": "char",
    "YAKSA_TYPE__UNSIGNED": "int",
    "YAKSA_TYPE__UNSIGNED_SHORT": "short",
    "YAKSA_TYPE__UNSIGNED_LONG": "long",
    "YAKSA_TYPE__LONG_DOUBLE": "double",
    "YAKSA_TYPE__UNSIGNED_LONG_LONG": "long_long",
    "YAKSA_TYPE__UINT8_T": "int8_t",
    "YAKSA_TYPE__UINT16_T": "int16_t",
    "YAKSA_TYPE__UINT32_T": "int32_t",
    "YAKSA_TYPE__UINT64_T": "int64_t",
    "YAKSA_TYPE__C_COMPLEX": "float",
    "YAKSA_TYPE__C_DOUBLE_COMPLEX": "double",
    "YAKSA_TYPE__C_LONG_DOUBLE_COMPLEX": "double",
    "YAKSA_TYPE__BYTE": "int8_t"
}


########################################################################################
##### Type-specific functions
########################################################################################

## hvector routines
def hvector(suffix, dtp, b, last):
    global s
    global idx
    global need_extent
    yutils.display(OUTFILE, "intptr_t stride%d = %s->u.hvector.stride / sizeof(%s);\n" % (suffix, dtp, b))
    if (need_extent == True):
        yutils.display(OUTFILE, "unsigned long extent%d = %s->extent / sizeof(%s);\n" % (suffix, dtp, b))
    if (last != 1):
        s += " + x%d * stride%d + x%d * extent%d" % (idx, suffix, idx + 1, suffix + 1)
        need_extent = True
    else:
        s += " + x%d * stride%d + x%d" % (idx, suffix, idx + 1)
        need_extent = False
    idx = idx + 2

## blkhindx routines
def blkhindx(suffix, dtp, b, last):
    global s
    global idx
    global need_extent
    yutils.display(OUTFILE, "intptr_t *array_of_displs%d = %s->u.blkhindx.array_of_displs;\n" % (suffix, dtp))
    if (need_extent == True):
        yutils.display(OUTFILE, "unsigned long extent%d = %s->extent / sizeof(%s);\n" % (suffix, dtp, b))
    if (last != 1):
        s += " + array_of_displs%d[x%d] / sizeof(%s) + x%d * extent%d" % \
             (suffix, idx, b, idx + 1, suffix + 1)
        need_extent = True
    else:
        s += " + array_of_displs%d[x%d] / sizeof(%s) + x%d" % (suffix, idx, b, idx + 1)
        need_extent = False
    idx = idx + 2

## hindexed routines
def hindexed(suffix, dtp, b, last):
    global s
    global idx
    global need_extent
    yutils.display(OUTFILE, "intptr_t *array_of_displs%d = %s->u.hindexed.array_of_displs;\n" % (suffix, dtp))
    if (need_extent == True):
        yutils.display(OUTFILE, "unsigned long extent%d = %s->extent / sizeof(%s);\n" % (suffix, dtp, b))
    if (last != 1):
        s += " + array_of_displs%d[x%d] / sizeof(%s) + x%d * extent%d" % \
             (suffix, idx, b, idx + 1, suffix + 1)
        need_extent = True
    else:
        s += " + array_of_displs%d[x%d] / sizeof(%s) + x%d" % (suffix, idx, b, idx + 1)
        need_extent = False
    idx = idx + 2

## dup routines
def dup(suffix, dtp, b, last):
    global need_extent
    if (need_extent == True):
        yutils.display(OUTFILE, "unsigned long extent%d = %s->extent / sizeof(%s);\n" % (suffix, dtp, b))
    need_extent = False

## contig routines
def contig(suffix, dtp, b, last):
    global s
    global idx
    global need_extent
    yutils.display(OUTFILE, "intptr_t stride%d = %s->u.contig.child->extent / sizeof(%s);\n" % (suffix, dtp, b))
    if (need_extent == True):
        yutils.display(OUTFILE, "unsigned long extent%d = %s->extent / sizeof(%s);\n" % (suffix, dtp, b))
    need_extent = False
    s += " + x%d * stride%d" % (idx, suffix)
    idx = idx + 1

# resized routines
def resized(suffix, dtp, b, last):
    global need_extent
    if (need_extent == True):
        yutils.display(OUTFILE, "unsigned long extent%d = %s->extent / sizeof(%s);\n" % (suffix, dtp, b))
    need_extent = False


########################################################################################
##### Core kernels
########################################################################################
def generate_kernels(b, darray):
    global need_extent
    global s
    global idx

    ##### figure out the function name to use
    for func in "pack","unpack":
        funcprefix = "%s_" % func
        for d in darray:
            funcprefix = funcprefix + "%s_" % d
        funcprefix = funcprefix + b.replace(" ", "_")

        ##### generate the XE kernel
        if (len(darray)):
            yutils.display(OUTFILE, "__kernel void yaksuri_xei_%s(__global const void *inbuf, __global void *outbuf, unsigned long count, __global const yaksuri_xei_md_s *restrict md)\n" % funcprefix)
            yutils.display(OUTFILE, "{\n")
            yutils.display(OUTFILE, "const %s *restrict sbuf = (const %s *) inbuf;\n" % (b, b));
            yutils.display(OUTFILE, "%s *restrict dbuf = (%s *) outbuf;\n" % (b, b));
            yutils.display(OUTFILE, "unsigned long extent = md->extent / sizeof(%s);\n" % b)
            yutils.display(OUTFILE, "unsigned long idx = get_global_id(0);\n")
            yutils.display(OUTFILE, "unsigned long res = idx;\n")
            yutils.display(OUTFILE, "unsigned long inner_elements = md->num_elements;\n")
            yutils.display(OUTFILE, "\n")

            yutils.display(OUTFILE, "if (idx >= (count * inner_elements))\n")
            yutils.display(OUTFILE, "    return;\n")
            yutils.display(OUTFILE, "\n")

            # copy loop
            idx = 0
            md = "md"
            for d in darray:
                if (d == "hvector" or d == "blkhindx" or d == "hindexed" or \
                    d == "contig"):
                    yutils.display(OUTFILE, "unsigned long x%d = res / inner_elements;\n" % idx)
                    idx = idx + 1
                    yutils.display(OUTFILE, "res %= inner_elements;\n")
                    yutils.display(OUTFILE, "inner_elements /= %s->u.%s.count;\n" % (md, d))
                    yutils.display(OUTFILE, "\n")

                if (d == "hvector" or d == "blkhindx"):
                    yutils.display(OUTFILE, "unsigned long x%d = res / inner_elements;\n" % idx)
                    idx = idx + 1
                    yutils.display(OUTFILE, "res %= inner_elements;\n")
                    yutils.display(OUTFILE, "inner_elements /= %s->u.%s.blocklength;\n" % (md, d))
                elif (d == "hindexed"):
                    yutils.display(OUTFILE, "unsigned long x%d;\n" % idx)
                    yutils.display(OUTFILE, "for (int i = 0; i < %s->u.%s.count; i++) {\n" % (md, d))
                    yutils.display(OUTFILE, "    unsigned long in_elems = %s->u.%s.array_of_blocklengths[i] *\n" % (md, d))
                    yutils.display(OUTFILE, "                         %s->u.%s.child->num_elements;\n" % (md, d))
                    yutils.display(OUTFILE, "    if (res < in_elems) {\n")
                    yutils.display(OUTFILE, "        x%d = i;\n" % idx)
                    yutils.display(OUTFILE, "        res %= in_elems;\n")
                    yutils.display(OUTFILE, "        inner_elements = %s->u.%s.child->num_elements;\n" % (md, d))
                    yutils.display(OUTFILE, "        break;\n")
                    yutils.display(OUTFILE, "    } else {\n")
                    yutils.display(OUTFILE, "        res -= in_elems;\n")
                    yutils.display(OUTFILE, "    }\n")
                    yutils.display(OUTFILE, "}\n")
                    idx = idx + 1
                    yutils.display(OUTFILE, "\n")

                md = "%s->u.%s.child" % (md, d)

            yutils.display(OUTFILE, "unsigned long x%d = res;\n" % idx)
            yutils.display(OUTFILE, "\n")

            dtp = "md"
            s = "x0 * extent"
            idx = 1
            x = 1
            need_extent = False
            for d in darray:
                if (x == len(darray)):
                    last = 1
                else:
                    last = 0
                getattr(sys.modules[__name__], d)(x, dtp, b, last)
                x = x + 1
                dtp = dtp + "->u.%s.child" % d

            if (func == "pack"):
                yutils.display(OUTFILE, "dbuf[idx] = sbuf[%s];\n" % s)
            else:
                yutils.display(OUTFILE, "dbuf[%s] = sbuf[idx];\n" % s)

            yutils.display(OUTFILE, "}\n\n")


########################################################################################
##### main function
########################################################################################
if __name__ == '__main__':
    ##### parse user arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('--pup-max-nesting', type=int, default=3, help='maximum nesting levels to generate')
    args = parser.parse_args()
    if (args.pup_max_nesting < 0):
        parser.print_help()
        print
        print("===> ERROR: pup-max-nesting must be positive")
        sys.exit(1)

    ##### generate the list of derived datatype arrays
    darraylist = [ ]
    yutils.generate_darrays(gencomm.derived_types, darraylist, args.pup_max_nesting)

    ##### generate the core pack/unpack kernels
    for b in builtin_types:
        filename = "src/backend/xe/pup/yaksuri_xei_pup_%s.cl" % b.replace(" ", "_")
        yutils.copyright_c(filename)
        OUTFILE = open(filename, "a")
        yutils.display(OUTFILE, "#include <stdint.h>\n")
        yutils.display(OUTFILE, "#include \"../include/yaksuri_xei_md.h\"\n")
        yutils.display(OUTFILE, "\n")

        for darray in darraylist:
            if (len(darray) == 0):
                continue
            generate_kernels(b, darray)

        yutils.display(OUTFILE, "\n")
        OUTFILE.close()

    ##### generate the core pack/unpack kernel declarations
    filename = "src/backend/xe/pup/yaksuri_xei_pup.h"
    yutils.copyright_c(filename)
    OUTFILE = open(filename, "a")
    yutils.display(OUTFILE, "#ifndef YAKSURI_XEI_PUP_H_INCLUDED\n")
    yutils.display(OUTFILE, "#define YAKSURI_XEI_PUP_H_INCLUDED\n")
    yutils.display(OUTFILE, "\n")

    # bit of an unnecessary header file, but creating it so we can
    # reuse the gencomm module
    for b in builtin_types:
        for darray in darraylist:
            for func in "pack","unpack":
                ##### figure out the function name to use
                s = "yaksuri_xei_%s_" % func
                for d in darray:
                    s = s + "%s_" % d
                s = s + b.replace(" ", "_")
                OUTFILE.write("#define %s \"%s\"\n" % (s, s))

    yutils.display(OUTFILE, "\n")
    yutils.display(OUTFILE, "#endif  /* YAKSURI_XEI_PUP_H_INCLUDED */\n")
    OUTFILE.close()

    ##### generate the switching logic to select pup functions
    gencomm.populate_pupfns(args.pup_max_nesting, "xe", blklens, builtin_types, builtin_maps)
