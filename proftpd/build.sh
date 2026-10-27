#!/bin/bash
# This is a buildpkg build.sh script
# build.sh helper functions
. ${BUILDPKG_SCRIPTS}/build.sh.functions
#
###########################################################
# Check the following 4 variables before running the script
topdir=proftpd
version=1.3.4d
pkgver=1
source[0]=https://ftp2.osuosl.org/pub/blfs/conglomeration/${topdir}/${topdir}-${version}.tar.gz
# If there are no patches, simply comment this

# Source function library
. ${BUILDPKG_SCRIPTS}/buildpkg.functions


# solaris 2.5.1 does not have ftello
   #--disable-shadow
configure_args+=(
   --disable-largefile
   --disable-ipv6
   --disable-auth-pam 
)
LIBS="$LIBS -llsecompat"
CPPFLAGS="$CPPFLAGS -D__EXTENSIONS__"
make_build_opts=( CC="gcc -g -include $prefix/include/lsecompat.h" )


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
    generic_check
}

reg install
install()
{
    generic_install DESTDIR
    doc ChangeLog README NEWS COPYING
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
