##
## Copyright (C) by Argonne National Laboratory
##     See COPYRIGHT in top-level directory
##

AM_CPPFLAGS += -I$(top_srcdir)/src/backend/xe/pup

libyaksa_la_SOURCES += \
	src/backend/xe/pup/yaksuri_xei_pup.c \
	src/backend/xe/pup/yaksuri_xei_event.c \
	src/backend/xe/pup/yaksuri_xei_get_ptr_attr.c \
	src/backend/xe/pup/yaksuri_xei_pup_char.c \
	src/backend/xe/pup/yaksuri_xei_pup_double.c \
	src/backend/xe/pup/yaksuri_xei_pup_float.c \
	src/backend/xe/pup/yaksuri_xei_pup_int.c \
	src/backend/xe/pup/yaksuri_xei_pup_int16_t.c \
	src/backend/xe/pup/yaksuri_xei_pup_int32_t.c \
	src/backend/xe/pup/yaksuri_xei_pup_int64_t.c \
	src/backend/xe/pup/yaksuri_xei_pup_int8_t.c \
	src/backend/xe/pup/yaksuri_xei_pup_long.c \
	src/backend/xe/pup/yaksuri_xei_pup_long_long.c \
	src/backend/xe/pup/yaksuri_xei_pup_short.c

noinst_HEADERS += \
	src/backend/xe/pup/yaksuri_xei_pup.h

include src/backend/xe/pup/Makefile.pup.mk

SPIR_FLAGS = -c -x cl -emit-llvm -S -cl-std=CL2.0 -Xclang -finclude-default-header
.cl.c:
	@if $(AM_V_P) ; then \
		strname=`echo $< | sed -e 's_.*/__g' -e 's/\..*//g'` && \
		echo "const char $${strname}[] = {" > $@ && \
		clang $(AM_CPPFLAGS) $(SPIR_FLAGS) -c $< -o - | \
			sed -e "s/\(.\)/'\1', /g" | sed -e "s/''',/'\\\'',/g" >> $@ && \
		echo "0 };" >> $@ ; \
	else \
		echo "  SPIRV    $@" ; \
		strname=`echo $< | sed -e 's_.*/__g' -e 's/\..*//g'` && \
		echo "const char $${strname}[] = {" > $@ && \
		clang $(AM_CPPFLAGS) $(SPIR_FLAGS) -c $< -o - | \
			sed -e "s/\(.\)/'\1', /g" | sed -e "s/'''/'\\\''/g" >> $@ && \
		echo "0 };" >> $@ ; \
	fi
