#!/bin/bash
# This is a buildpkg build.sh script
# build.sh helper functions
. ${BUILDPKG_SCRIPTS}/build.sh.functions
#
###########################################################
# Check the following 4 variables before running the script
topdir=proftpd
#version=1.3.4d
version=1.3.9b
pkgver=1
source[0]=https://ftp2.osuosl.org/pub/blfs/conglomeration/${topdir}/${topdir}-${version}.tar.gz
# If there are no patches, simply comment this

# Source function library
. ${BUILDPKG_SCRIPTS}/buildpkg.functions


# solaris 2.5.1 does not have ftello
   #--disable-shadow
  #--sysconfdir=/etc/proftpd \
configure_args+=(
   SHELL=$prefix/bin/bash
   SED=$prefix/bin/gsed
   AWK=$prefix/bin/ggawk
   --disable-largefile
   --disable-ipv6
   --disable-auth-pam 
  --disable-xattr
  --disable-caps
  --disable-nls 
  --disable-dynamic-load
  --disable-tests
  --disable-validation
)
LIBS="$LIBS -llsecompat"
CPPFLAGS="$CPPFLAGS -D__EXTENSIONS__ -Dfutmpx=utmpx -DHAVE_STRUCT_ADDRINFO -I/usr/local/lse/include/ncurses"
make_build_opts=( CC="gcc -g -include $prefix/include/lsecompat.h -include /usr/local/lse/include/lsenet.h" )


reg prep
prep()
{
    generic_prep
    setdir source
    ${__gsed} -i 's|^#! /bin/sh|#/usr/local/lse/bin/bash|' configure
    # Patch root and sub-directory scripts to use bash
    find . -type f \( -name "configure" -o -name "glue.sh" -o -name "install-sh" \) \
      -exec ${__gsed} -i 's|^#! */bin/sh|#!/usr/local/lse/bin/bash|' {} +


    #${__gsed} -i 's|^#! /bin/sh|#/usr/local/lse/bin/bash|' modules/glue.sh

}

reg build
build()
{
    export SHELL=$prefix/bin/bash
    export CONFIG_SHELL=$prefix/bin/bash
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
    doc ChangeLog NEWS COPYING

    ${__mkdir} -p ${stagedir}/${_sysconfdir}/init.d
    ${__mkdir} -p ${stagedir}/${_sysconfdir}/rc0.d
    ${__mkdir} -p ${stagedir}/${_sysconfdir}/rc1.d
    ${__mkdir} -p ${stagedir}/${_sysconfdir}/rc2.d
    ${__mkdir} -p ${stagedir}/${_sysconfdir}/rcS.d

    # prep-config
    ${__mv} ${stagedir}${prefix}/${_sysconfdir}/proftpd.conf ${stagedir}${prefix}/${_sysconfdir}/proftpd.conf.default
    ${__gsed} -i 's|Anonymous ~ftp|Anonymous /usr/local/lse/var/proftpd|'  ${stagedir}${prefix}/${_sysconfdir}/proftpd.conf.default
   

    # Install initscript
    ${__gcp} -v $patchdir/proftpd.init ${stagedir}/${_sysconfdir}/init.d/lse_proftpd
    chmod 755 ${stagedir}/${_sysconfdir}/init.d/lse_proftpd
    (setdir ${stagedir}/${_sysconfdir}/rc0.d; ${__ln} -sf ../init.d/lse_proftpd K02lse_proftpd)
    (setdir ${stagedir}/${_sysconfdir}/rc1.d; ${__ln} -sf ../init.d/lse_proftpd K02lse_proftpd)
    (setdir ${stagedir}/${_sysconfdir}/rcS.d; ${__ln} -sf ../init.d/lse_proftpd K02lse_proftpd)
    (setdir ${stagedir}/${_sysconfdir}/rc2.d; ${__ln} -sf ../init.d/lse_proftpd S98lse_proftpd)
    setdir ${stagedir}${prefix}
    rm -r -f var libexec

}

reg pack
pack()
{
    lprefix=${prefix#/*}
    topinstalldir=/
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
