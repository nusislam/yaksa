##
## Copyright (C) by Argonne National Laboratory
##     See COPYRIGHT in top-level directory
##

if BUILD_XE_BACKEND
include $(top_srcdir)/src/backend/xe/include/Makefile.mk
include $(top_srcdir)/src/backend/xe/hooks/Makefile.mk
include $(top_srcdir)/src/backend/xe/md/Makefile.mk
include $(top_srcdir)/src/backend/xe/pup/Makefile.mk
else
include $(top_srcdir)/src/backend/xe/stub/Makefile.mk
endif !BUILD_XE_BACKEND
