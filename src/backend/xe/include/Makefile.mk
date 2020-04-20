##
## Copyright (C) by Argonne National Laboratory
##     See COPYRIGHT in top-level directory
##

AM_CPPFLAGS += -I$(top_srcdir)/src/backend/xe/include

noinst_HEADERS += \
	src/backend/xe/include/yaksuri_xe_pre.h \
	src/backend/xe/include/yaksuri_xe_post.h \
	src/backend/xe/include/yaksuri_xei.h \
	src/backend/xe/include/yaksuri_xei_md.h
