##
## Copyright (C) by Argonne National Laboratory
##     See COPYRIGHT in top-level directory
##

AM_CPPFLAGS += -I$(top_srcdir)/src/backend/xe/stub

noinst_HEADERS += \
	src/backend/xe/stub/yaksuri_xe_pre.h \
	src/backend/xe/stub/yaksuri_xe_post.h
