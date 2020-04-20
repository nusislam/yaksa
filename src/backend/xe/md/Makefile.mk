##
## Copyright (C) by Argonne National Laboratory
##     See COPYRIGHT in top-level directory
##

AM_CPPFLAGS += -I$(top_srcdir)/src/backend/xe/md

libyaksa_la_SOURCES += \
	src/backend/xe/md/yaksuri_xei_md.c
