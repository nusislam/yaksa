##
## Copyright (C) by Argonne National Laboratory
##     See COPYRIGHT in top-level directory
##

AM_CPPFLAGS += -I$(top_srcdir)/src/backend/xe/hooks

libyaksa_la_SOURCES += \
	src/backend/xe/hooks/yaksuri_xe_init_hooks.c \
	src/backend/xe/hooks/yaksuri_xei_type_hooks.c
