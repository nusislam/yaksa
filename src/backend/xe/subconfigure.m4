##
## Copyright (C) by Argonne National Laboratory
##     See COPYRIGHT in top-level directory
##


##########################################################################
##### capture user arguments
##########################################################################

# --with=xe
PAC_SET_HEADER_LIB_PATH([xe])
PAC_CHECK_HEADER_LIB([level_zero/ze_api.h],[ze_loader],[zeCommandQueueCreate],[have_xe=yes],[have_xe=no])
if test "${have_xe}" = "yes" ; then
    AC_DEFINE([HAVE_XE],[1],[Define is XE is available])
fi
AM_CONDITIONAL([BUILD_XE_BACKEND], [test x${have_xe} = xyes])
AM_CONDITIONAL([BUILD_XE_TESTS], [test x${have_xe} = xyes])


# --with-xe-p2p
AC_ARG_ENABLE([xe-p2p],AS_HELP_STRING([--enable-xe-p2p={yes|no|cliques}],[controls XE P2P capability]),,
              [enable_xe_p2p=yes])
if test "${enable_xe_p2p}" = "yes" ; then
    AC_DEFINE([XE_P2P],[XE_P2P_ENABLED],[Define if XE P2P is enabled])
elif test "${enable_xe_p2p}" = "cliques" ; then
    AC_DEFINE([XE_P2P],[XE_P2P_CLIQUES],[Define if XE P2P is enabled in clique mode])
else
    AC_DEFINE([XE_P2P],[XE_P2P_DISABLED],[Define if XE P2P is disabled])
fi


##########################################################################
##### analyze the user arguments and setup internal infrastructure
##########################################################################

if test ${have_xe} = "yes" ; then
    supported_backends="${supported_backends},xe"
fi
