##
## Copyright (C) by Argonne National Laboratory
##     See COPYRIGHT in top-level directory
##

AM_CPPFLAGS += -I$(top_srcdir)/src/util

libyaksa_la_SOURCES += \
	src/util/yaksu_atomics.c \
	src/util/yaksu_pool.c

noinst_HEADERS += \
	src/util/yaksu.h \
	src/util/yaksu_base.h \
	src/util/yaksu_atomics.h \
	src/util/yaksu_pool.h
