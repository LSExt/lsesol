#!/bin/bash
# This is a buildpkg build.sh script
# build.sh helper functions
. ${BUILDPKG_SCRIPTS}/build.sh.functions
#
###########################################################
# Check the following 4 variables before running the script
topdir=openssl
version=3.0.15
pkgver=2
source[0]=https://github.com/openssl/openssl/releases/download/${topdir}-${version}/${topdir}-${version}.tar.gz
# If there are no patches, simply comment this
patch[0]=0002-Include-sys-atomic.h-directly-on-Solaris.patch
patch[1]=0007-Use-target-CPU-choice-on-SPARC.patch

# Source function library
. ${BUILDPKG_SCRIPTS}/buildpkg.functions

# For cpu settings
. ${BUILDPKG_BASE}/gcc/build.sh.gcc.cpu

# Global settings
make_check_target="test"
__configure="./Configure"

# Define compatibility headers and enforce -O2
export GCC_MCPU="-mcpu=${TARGET_CPU:-v7}"
compat_cflags="-O2 $GCC_MCPU -I$prefix/include \
-include $prefix/include/lsecompat.h -include $prefix/include/lsenet.h"
configure_args=(
    --prefix=$prefix 
    --openssldir=${prefix}/${_sharedir}/ssl 
    --with-rand-seed=devrandom,egd 
    zlib 
    shared 
    no-dso 
    enable-egd 
    threads
    no-asm
    CFLAGS="$CFLAGS $compat_cflags"
    CPPFLAGS="$CPPFLAGS $compat_cflags"
    # OpenSSL 3.0 uses LDLIBS for extra library linkages
    LDFLAGS="$LDFLAGS"
    LDLIBS="$LIBS -lposix4 -llsecompat -llsenet -lpthread -lthread -lc"
    EX_LIBS="-llsecompat -lc"
    # Clears the version-script mapfile generator variable
    shared_mapfile=""
)
# 2. Target architecture MUST be appended AT THE VERY END
if [ "$arch" = "sparc" ]; then
    if [ "${gcc_arch}" = "ultrasparc" ]; then
        configure_args+=(solaris-sparcv9-gcc)
    elif [ "${gcc_arch}" = "v9" ]; then
        configure_args+=(solaris64-sparcv9-gcc)
    else
        configure_args+=(solaris-sparc${gcc_arch}-gcc)
    fi
else
    configure_args+=(386 solaris-x86-gcc)
fi

cpus_online=$(psrinfo | nawk '/on-line/ {count++} END { print count + 1 }')

reg prep
prep()
{
    generic_prep
    setdir source
    # gcc < 4.6.0 will not link -lpthread when -pthread is combined
    # with -shared (see PR target/18788)
    ${__gsed} -i '/ex_libs/ s/-pthread/-lpthread/' Configurations/10-main.conf
}

reg build
build()
{
    setdir source

    echo $__configure "${configure_args[@]}"
    $__configure "${configure_args[@]}"

    ${__make} -j${cpus_online} depend
    ${__make} -j${cpus_online}
}

reg check
check()
{
    generic_check
}

reg install
install()
{
    clean stage
    setdir source
    ${__make} DESTDIR=${stagedir} install_sw install_docs
    # Make .sos writable
    chmod 755 ${stagedir}${prefix}/${_libdir}/*.so.*
    chmod 755 ${stagedir}${prefix}/${_libdir}/engines-3/*.so
    # Nuke static libraries - they just take up space
    ${__rm} -f ${stagedir}${prefix}/${_libdir}/*.a

    doc README* CHANGES.md LICENSE.txt NEWS.md

    custom_install=1
    generic_install INSTALL_PREFIX

    # Compatible with previous releases
    compat openssl 3.0.2 1 1
    compat openssl 3.0.3 1 1
    compat openssl 3.0.5 1 1
    compat openssl 3.0.7 1 1
    compat openssl 3.0.9 1 1
    compat openssl 3.0.10 1 1
    compat openssl 3.0.11 1 1
    compat openssl 3.0.12 1 1
    compat openssl 3.0.13 1 1
    compat openssl 3.0.14 1 1
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
