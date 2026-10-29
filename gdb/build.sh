#!/bin/bash
# This is a buildpkg build.sh script
# build.sh helper functions
. ${BUILDPKG_SCRIPTS}/build.sh.functions
#
###########################################################
# Check the following 4 variables before running the script
topdir=gdb
version=7.8
pkgver=1
source[0]=ftp://ftp.sunet.se/pub/gnu/gdb/$topdir-$version.tar.xz
# If there are no patches, simply comment this
#patch[0]=sol-2.5.1-mocks.patch

# Source function library
. ${BUILDPKG_SCRIPTS}/buildpkg.functions

# Global overrides / additions
#export CPPFLAGS="$CPPFLAGS -I$prefix/include -lpthread -lthread"
export LDFLAGS="$LDFLAGS -L$prefix/lib -R$prefix/lib -lgcc_s"
#export CPPFLAGS="$CPPFLAGS -lpthread -lthread -D__EXTENSIONS__ -D_LONG_LONG -D_INT64_TYPE -D_SYSCALL32 -DPROC_SERVICE_IS_OLD"
#export CPPFLAGS="$CPPFLAGS -lpthread -lthread -D__EXTENSIONS__ -D_LONG_LONG -D_INT64_TYPE -DPROC_SERVICE_IS_OLD"
export CPPFLAGS="$CPPFLAGS -lpthread -lthread -D__EXTENSIONS__ -D_LONG_LONG -D_INT64_TYPE"
#export LIBS="-llsecompat"

reg prep
prep()
{
    generic_prep
}

reg build
build()
{
    generic_build
}

reg check
check()
{
    export LD_OPTIONS="-Wl,-rpath,$prefix/lib"
    generic_check
}

reg install
install()
{
    generic_install DESTDIR
    # Cleanup
    ${__rm} -rf ${stagedir}${prefix}/$_libdir
    ${__rm} -rf ${stagedir}${prefix}/$_includedir
    ${__rm} -rf ${stagedir}${prefix}/$_sharedir/locale
    ${__rm} -f ${stagedir}${prefix}/${_infodir}/bfd*
    ${__rm} -f ${stagedir}${prefix}/${_infodir}/standard*
    ${__rm} -f ${stagedir}${prefix}/${_infodir}/configure*
    # Install docs
    ${__mkdir} -p ${stagedir}${prefix}/${_vdocdir}
    setdir source
    ${__cp} -p COPYING COPYING.LIB COPYING3 gdb/{NEWS,README} \
	${stagedir}${prefix}/${_vdocdir}
}

reg pack
pack()
{
    generic_pack
}

reg distclean
distclean()
{
    clean distclean
}

###################################################
# No need to look below here
###################################################
build_sh $*
