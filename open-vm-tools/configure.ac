################################################################################
### Copyright 2007 VMware, Inc.  All rights reserved.
###
### Configure script for building the VMware OSS Tools.
###
###
### This program is free software; you can redistribute it and/or modify
### it under the terms of version 2 of the GNU General Public License as
### published by the Free Software Foundation.
###
### This program is distributed in the hope that it will be useful,
### but WITHOUT ANY WARRANTY; without even the implied warranty of
### MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
### GNU General Public License for more details.
###
### You should have received a copy of the GNU General Public License
### along with this program; if not, write to the Free Software
### Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
################################################################################

################################################################################
# In addition to the usual environment variables and command line
# arguments that a configure script usually takes (CFLAGS, LDFLAGS,
# etc.), this script also accepts variables of the form:
#
#  CUSTOM_LIB_CPPFLAGS: compile-time flags for LIB
#  CUSTOM_LIB_LIBS: link-time flags for LIB
#  RPCGENFLAGS: extra flags to pass to rpcgen
#
# The following libraries are currently tested: DNET, FUSE, GLIB2, GMODULE,
# GOBJECT, GTHREAD, GTK, GTKMM, ICU, LIBPNG, PAM, PROCPS, URIPARSER, ZLIB
#
# For the procps library: you can provide the name of the procps library on
# your system by defining CUSTOM_PROCPS_NAME. By default the configure script
# will try both "-lproc" and "-lproc-3.2.7".
################################################################################

###
### Initialization
###

TOOLS_VERSION="2012.03.13"
AC_INIT(
   [open-vm-tools],
   [2012.03.13],
   [open-vm-tools-devel@lists.sourceforge.net])

# In order to make this configure script auto-detect situations where
# people have a 32-bit userland running with a 64-bit kernel, we try to ask
# the compiler (assumedly gcc) for its default Target:.
# We have to set up $TEST_CC manually, since AC_PROG_CC hasn't yet been run (and can't be until AC_CANONICAL_HOST & AC_CANONICAL_BUILD are run)
# The purpose of all this is to set up $host_alias/$build_alias in a more
# intelligent way than config.guess currently does.
TEST_CC="$CC_FOR_BUILD"
test -z "$TEST_CC" && TEST_CC="$HOST_CC"
test -z "$TEST_CC" && TEST_CC="$CC"
if test -n "$TEST_CC" -a -z "$host_alias"; then
   host_alias="`$TEST_CC -dumpmachine`"
   if test -z "$build_alias" -a -n "$host_alias"; then
      build_alias="$host_alias"
   fi
fi
unset TEST_CC

# checkvm/checkvm.c has no special significance - we just need to pass in a file that
# helps autoconf verify that it really has found the source tree.
AC_CONFIG_SRCDIR([checkvm/checkvm.c])

# Keep the top-level directory tidy by putting auxiliary build tools and local
# macros in separate subdirectories.
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_HOST
AC_CANONICAL_BUILD

# Quote the regular expressions
case "$host_cpu" in
   [i[3456]86])
      userSpaceBitness="32"
      ;;
   [x86_64])
      userSpaceBitness="64"
      ;;
   [*])
      AC_MSG_ERROR([Unknown architecture.])
      ;;
esac

# Operational arguments.
AC_ARG_WITH([root-privileges],
	    [AS_HELP_STRING([--without-root-privileges],
	       [does not perform any operations that require root privileges])],
	    [],
	    [with_root_privileges=yes])

# Kernel arguments.
# The kernel args have to go here otherwise the KERNEL_RELEASE won't be visible
# to getOsVersion()
AC_ARG_WITH([kernel-modules],
	    [AS_HELP_STRING([--without-kernel-modules],
		[does not compile or install the kernel modules])],
	    [],
	    [with_kernel_modules=yes])

AC_ARG_WITH([kernel-release],
	    [AS_HELP_STRING([--with-kernel-release],
		[specifies the kernel release you want to build against])],
	    [KERNEL_RELEASE="$withval"],
	    [KERNEL_RELEASE=`uname -r`])

AC_ARG_WITH([linuxdir],
	    [AS_HELP_STRING([--with-linuxdir],
		[specifies the Linux directory you want to use])],
	    [LINUXDIR="$withval"],
	    [LINUXDIR=/lib/modules/$KERNEL_RELEASE])

# Turn the uname output into something we can run comparisons on.
getOsVersion() {
   major_version="`echo $KERNEL_RELEASE | cut -f1 -d. | cut -f1 -d-`"
   minor_version="`echo $KERNEL_RELEASE | cut -f2 -d. | cut -f1 -d-`"
   micro_version="`echo $KERNEL_RELEASE | cut -f3 -d. | cut -f1 -d-`"
   printf '%02d%02d%03d' $major_version $minor_version $micro_version
}

case "$host_os" in
   [linux*])
      os="linux"
      ;;
   [freebsd*])
      os="freebsd"
      ;;
   [solaris*])
      os="solaris"
      ;;
   [*])
      AC_MSG_WARN([This is an untested and unsupported Operating System. Proceed at your own peril.])
      ;;
esac
osVersion="`getOsVersion`"

if test "$with_kernel_modules" = "yes"; then
   case "$os" in
      linux)
         if test "$osVersion" -lt 206009; then
            AC_MSG_ERROR([Kernels prior to 2.6.9 are not supported in this release of open-vm-tools. Configure using --without-kernel-modules to suppress building kernel drivers.])
         fi
         if test ! -d "$LINUXDIR/kernel/"; then
             AC_MSG_ERROR([$LINUXDIR/kernel does not exist])
         fi
         LINUXINCLUDE="$LINUXDIR/build/include"
         if test ! -d "$LINUXINCLUDE"; then
            AC_MSG_ERROR([Can't find include dir under $LINUXDIR])
         fi
         ;;

      freebsd)
         freebsd_sysdir=/usr/src/sys
         if test -n "$SYSDIR"; then
            freebsd_sysdir="$SYSDIR"
         fi
         if test ! -f "$freebsd_sysdir/conf/kmod.mk"; then
            AC_MSG_ERROR([FreeBSD kernel tree not found. Please install the kernel sources (or provide the location using SYSDIR) or configure using --without-kernel-modules.])
         fi
         ;;
   esac
fi

# Arguments for disabling individual open-vm-tools features or libraries.
AC_ARG_ENABLE(
   multimon,
   AS_HELP_STRING(
      [--disable-multimon],
      [disables multimon, enabled by default]),
   [enable_multimon="$enableval"],
   [enable_multimon="yes"])

AC_ARG_WITH(
   gtk2,
   AS_HELP_STRING(
      [--without-gtk2],
      [compiles without Gtk 2.0]),
   [with_gtk2="$withval"],
   [with_gtk2="yes"])

AC_ARG_WITH(
   gtkmm,
   AS_HELP_STRING(
      [--without-gtkmm],
      [compiles without Gtkmm, sigc++, and related libs]),
   [with_gtkmm="$withval"],
   [with_gtkmm="yes"])

AC_ARG_ENABLE(
   docs,
   AS_HELP_STRING(
      [--disable-docs],
      [disables generation of API documentation; by default, docs are built if doxygen is available.]),
   [enable_docs="$enableval"],
   [enable_docs="yes"])

AC_ARG_ENABLE(
   tests,
   AS_HELP_STRING(
      [--disable-tests],
      [disable compilation of test code.]),
   [enable_tests="$enableval"],
   [enable_tests="auto"])

AM_INIT_AUTOMAKE

###
### Constants
###
# These need to be declared after initialization.

# Some of our macro call-sites require changes to
# CPPFLAGS/CFLAGS/LDFLAGS. In such places, we save the original value
# of CPPFLAGS/CFLAGS/LDFLAGS before the macro call and restore it when
# the call is done. We must perform this save at each macro site,
# because CPPFLAGS/CFLAGS/LDFLAGS may change over the course of
# configuration.
#
# CPPFLAGS is intended for preprocessor options (-D and -I mainly)
# CFLAGS is intended for compiler options (-O, -f, -W, and so forth)

CPPFLAGS="$CPPFLAGS -DUSING_AUTOCONF=1 -DOPEN_VM_TOOLS"

###
### Programs
###
# C preprocessor and compiler.
AC_PROG_CPP
AC_PROG_CC

# C++ compiler. Note that unlike AC_PROG_CC, this call does not trigger an
# error if no C++ compiler was found; it'll just set the variable CXX to 'g++'.
AC_PROG_CXX

# This allows features like per-target compiler flags.  I.e., you can compile
# one copy of the same sources twice with different flags.  (See lib/guestApp
# for an example.)
AM_PROG_CC_C_O

# Needed for the various install and uninstall hooks.
AC_PROG_INSTALL
AC_PROG_SED
AC_PROG_LN_S
AC_PROG_MKDIR_P

# Needed for creating the archives in lib/ and the shared libraries.
AC_PROG_LIBTOOL
if test "$ac_cv_prog_AR" = false; then
   AC_MSG_ERROR([The 'ar' utility was not found. Please put ar on the path.])
fi

# We use pkg-config to set up the cflags and libs for gtk.
AC_CHECK_PROG(
   [HAVE_PKG_CONFIG],
   [pkg-config],
   [yes],
   [no])

if test "$GCC" != "yes"; then
   AC_MSG_ERROR([Only GCC is currently supported. Please put gcc in the path.])
fi

###
### Libraries
###

AC_PATH_XTRA

#
# Check for libintl.h. When configuring using "--without-x", /usr/local/include
# may not be added to the include path, so code that use glib's i18n functions
# would fail to compile because it can't find libintl.h.
#
AC_CHECK_HEADER([libintl.h],
                [],
                [have_libintl=no],
                [])
if test "$have_libintl" = "no"; then
   unset ac_cv_header_libintl_h
   CPPFLAGS="$CPPFLAGS -I/usr/local/include"
   AC_CHECK_HEADER([libintl.h],
                   [],
                   [AC_MSG_ERROR([libintl.h not found. Make sure you have the gettext headers installed.])],
                   [])
fi

#
# Check for glib 2.6.0 or greater.
#
AC_VMW_CHECK_LIB([glib-2.0],
                 [GLIB2],
                 [glib-2.0],
                 [],
                 [2.6.0],
                 [glib.h],
                 [g_key_file_new],
                 [],
                 [AC_MSG_ERROR([glib >= 2.6.0 is required.])])
AC_VMW_CHECK_LIB([gmodule-2.0],
                 [GMODULE],
                 [gmodule-2.0],
                 [],
                 [2.6.0],
                 [],
                 [],
                 [],
                 [AC_MSG_ERROR([gmodule >= 2.6.0 is required.])])
AC_VMW_CHECK_LIB([gobject-2.0],
                 [GOBJECT],
                 [gobject-2.0],
                 [],
                 [2.6.0],
                 [glib-object.h],
                 [],
                 [],
                 [AC_MSG_ERROR([gobject >= 2.6.0 is required.])])
AC_VMW_CHECK_LIB([gthread-2.0],
                 [GTHREAD],
                 [gthread-2.0],
                 [],
                 [2.6.0],
                 [],
                 [],
                 [],
                 [AC_MSG_ERROR([glib >= 2.6.0 is required.])])
AC_CHECK_PROG([have_genmarshal],
              [glib-genmarshal],
              [yes],
              [no])

if test "$have_genmarshal" != "yes"; then
   AC_MSG_ERROR([glib-genmarshal is required; make sure it's available in your path.])
fi

#
# Parts of our Linux code require more recent version of glib
#
if test "$os" = "linux"; then
   AC_VMW_CHECK_LIB([glib-2.0],
                    [GLIB2],
                    [glib-2.0],
                    [],
                    [2.14.0],
                    [glib.h],
                    [g_regex_new],
                    [have_glib_2_14=yes],
                    [AC_MSG_WARN([glib is not recent enough, some features will be disabled.])])
fi

#
# Check for fuse.
#
AC_VMW_CHECK_LIB([fuse],
                 [FUSE],
                 [fuse],
                 [],
                 [],
                 [fuse.h],
                 [fuse_main],
                 [have_fuse=yes],
                 [have_fuse=no;
                  AC_MSG_WARN([Fuse is missing, vmblock-fuse will be disabled.])])

#
# Check for PAM.
#
AC_ARG_WITH([pam],
   [AS_HELP_STRING([--without-pam],
   [compiles without PAM support.])],
   [],
   [with_pam=yes])

if test "$with_pam" = "yes"; then
   AC_VMW_DEFAULT_FLAGS([PAM])
   AC_VMW_CHECK_LIB([pam],
                    [PAM],
                    [],
                    [],
                    [],
                    [security/pam_appl.h],
                    [pam_start],
                    [PAM_CPPFLAGS="$PAM_CPPFLAGS -DUSE_PAM"],
                    [AC_VMW_LIB_ERROR([PAM], [pam])])
fi

#
# Check for CUnit and disable test code if not available.
#
if test "$enable_tests" = "auto" -o "$enable_tests" = "yes"; then
   AC_VMW_DEFAULT_FLAGS([CUNIT])
   AC_VMW_CHECK_LIB([cunit],
                    [CUNIT],
                    [],
                    [],
                    [],
                    [CUnit/CUnit.h],
                    [CU_initialize_registry],
                    [have_cunit=yes],
                    [have_cunit=no])
   if test "$have_cunit" = "no"; then
      if test "$enable_tests" = "yes"; then
         AC_VMW_LIB_ERROR([CUNIT], [cunit])
      else
         AC_MSG_WARN([CUnit not found, tests won't be compiled.])
      fi
   fi
fi

# If the user explicitly disables X11, then don't try to detect the X-related libraries
if test "$have_x" = "disabled"; then
   enable_multimon="no"
elif test "$have_x" != "yes"; then
   AC_MSG_ERROR(
      [The X11 libraries were not found. Please configure without X11 (using --without-x), or install the libX11 devel package(s).])
else
   CPPFLAGS="$CPPFLAGS $X_CFLAGS"
   COMMON_XLIBS="$X_PRE_LIBS $X_LIBS -lX11 $X_EXTRA_LIBS"

   AC_CHECK_LIB(
      [Xext],
      [XeviQueryVersion],
      [COMMON_XLIBS="-lXext $COMMON_XLIBS"],
      [AC_MSG_ERROR(
         [libXext not found. Please configure without X11 (using --without-x), or install the libXext devel package(s).])],
      [$COMMON_XLIBS])

   AC_CHECK_HEADER(
      [X11/extensions/extutil.h],
      [],
      [AC_MSG_ERROR(
         [X11/extensions/extutil.h header not found - you're probably on Solaris 10 or older. Please copy that header file onto your system manually, or configure without X11 (using --without-x).])],
      [#include <X11/Xlib.h>
       #include <X11/Xproto.h>])

   if test "$enable_multimon" != "no"; then
      AC_CHECK_LIB(
	 [Xinerama],
	 [XineramaQueryVersion],
	 [COMMON_XLIBS="-lXinerama $COMMON_XLIBS"],
	 [AC_MSG_ERROR(
	    [libXinerama not found. Please configure without multimon (using --disable-multimon), configure without X11 (using --without-x), or install the libXinerama devel package(s).])],
	 [$COMMON_XLIBS])
   fi

   AC_CHECK_LIB(
      [Xi],
      [XOpenDevice],
      [COMMON_XLIBS="-lXi $COMMON_XLIBS"],
      [AC_MSG_ERROR(
         [libXi not found. Please configure without X11 (using --without-x), or install the libXi devel package(s).])],
      [$COMMON_XLIBS])

   AC_CHECK_LIB(
      [Xrender],
      [XRenderQueryVersion],
      [COMMON_XLIBS="-lXrender $COMMON_XLIBS"],
      [AC_MSG_ERROR(
         [libXrender not found. Please configure without X11 (using --without-x), or install the libXrender devel package(s).])],
      [$COMMON_XLIBS])

   AC_CHECK_LIB(
      [Xrandr],
      [XRRQueryVersion],
      [COMMON_XLIBS="-lXrandr $COMMON_XLIBS"],
      [AC_MSG_ERROR(
	 [libXrandr not found. Please configure without X11 (using --without-x) or install the libXrandr devel package(s).])],
      [$COMMON_XLIBS])

   AC_CHECK_LIB(
      [Xtst],
      [XTestQueryExtension],
      [COMMON_XLIBS="-lXtst $COMMON_XLIBS"],
      [AC_MSG_ERROR(
	 [libXtst not found. Please configure without X11 (using --without-x) or install the libXtst devel package(s).])],
      [$COMMON_XLIBS])

   AC_CHECK_LIB(
      [SM],
      [SmcOpenConnection],
      [XSM_LIBS="-lSM -lICE" && have_xsm_lib="yes"],
      []
      [-lICE])

   AC_CHECK_HEADERS([X11/SM/SMlib.h X11/ICE/ICElib.h],
                    [have_xsm_header="yes"],
                    [],
                    [])
   if test "$have_xsm_lib" = "yes" -a "$have_xsm_header" = "yes"; then
      have_xsm="yes"
   fi

   AC_CHECK_LIB(
      [Xcomposite],
      [XCompositeQueryExtension],
      [XCOMPOSITE_LIBS="-lXcomposite"],
      [have_xcomposite="no"]
      [])
   AC_CHECK_HEADERS([X11/extensions/Xcomposite.h],
                    [],
                    [have_xcomposite="no"],
                    [])
   if test "$have_xcomposite" != "no"; then
      have_xcomposite="yes"
   fi

   # Check whether we have gtk+ 2.0.
   if test "$with_gtk2" != "no"; then
      # gdk_display_get_default_group (added in gtk+ 2.4.0) is function currently
      # needed by vmware-user.
      AC_VMW_CHECK_LIB([gtk-x11-2.0],
                       [GTK],
                       [gtk+-2.0],
                       [],
                       [2.4.0],
                       [gtk/gtk.h],
                       [gdk_display_get_default_group],
                       [GTK_CPPFLAGS="$GTK_CPPFLAGS -DGTK2"],
                       [AC_MSG_ERROR([Gtk+ 2.0 library not found or too old. Please configure without Gtk+ support (using --without-gtk2) or install the Gtk+ 2.0 devel package.])])
   fi

   #
   # Check for gtkmm 2.4.0 or greater.
   #

   if test "$with_gtkmm" != "no"; then
      CUSTOM_GTKMM_CPPFLAGS="$CUSTOM_GTKMM_CPPFLAGS $GTK_CPPFLAGS"
      AC_VMW_CHECK_LIBXX([gtkmm-2.4],
                         [GTKMM],
                         [gtkmm-2.4],
                         [],
                         [2.4.0],
                         [gtkmm.h],
                         [],
                         [GTKMM_CPPFLAGS="$GTKMM_CPPFLAGS -DHAVE_GTKMM"],
                         [AC_MSG_ERROR([gtkmm library not found. Please install the libgtkmm devel package(s), or re-configure using --without-gtkmm.])])
   fi
fi # End of checks for X libraries

AC_CHECK_LIB(
   [crypt],
   [crypt],
   [HAVE_CRYPT="yes"],
   [AC_MSG_ERROR(
      [libcrypt not found. Please install the libc/libcrypt devel package(s).])])

AC_CHECK_FUNCS(
   dlopen,
   ,
   [AC_CHECK_LIB(
      dl,
      dlopen,
      [VIX_LIBADD="$VIX_LIBADD -ldl"
       LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD -ldl"],
      [AC_MSG_ERROR(
         [dlopen was not found, but is required for open-vm-tools to function properly. Please contact your OS vendor.])])])

AC_CHECK_FUNCS([ecvt])
AC_CHECK_FUNCS([fcvt])

if test "$os" = "freebsd" -a "$osVersion" -ge 600000; then
   AC_CHECK_LIB(
      [thr],
      [pthread_mutex_init],
      [THREAD_LIB=-lthr],
      [AC_MSG_ERROR(
         [Unable to locate required threading library libthr.])])
else
   AC_CHECK_LIB(
      [pthread],
      [pthread_mutex_init],
      [THREAD_LIB=-lpthread],
      [AC_MSG_ERROR(
         [libpthread not found. Please install the libc/libpthread devel package(s).])])
fi

# PAM prefix
AC_ARG_WITH([pam-prefix],
	    [AS_HELP_STRING([--with-pam-prefix],
		[specifies where pam files go. Default is $(sysconfdir)])],
	    [PAM_PREFIX="$withval"],
	    [PAM_PREFIX='$(sysconfdir)'])

if test "$os" = "linux"; then
        AC_ARG_WITH([procps],
	    [AS_HELP_STRING([--without-procps],
		[compiles without libproc (disables support for meminfo)])],
	    [],
	    [with_procps=yes])
else
	with_procps="no"
fi

if test "$with_procps" = "yes"; then
   if test -z "$CUSTOM_PROCPS_NAME"; then
      CUSTOM_PROCPS_NAME=proc
   fi

   # XXX: no pkg-config and no procps-config means we need to
   # hard-code a sensible default.
   if test -z "$CUSTOM_PROCPS_LIBS"; then
      CUSTOM_PROCPS_LIBS="-L/lib"
   fi

   # Some distros provide libproc-${version}.so only, others provide the
   # libproc.so symlink. Try both to see what sticks (but only try the 3.2.7
   # and 3.2.8 versions - adding every possible version here would be a mess).
   #
   # Users can help by providing CUSTOM_PROCPS_NAME / CUSTOM_PROCPS_LIBS if
   # necessary.
   have_procps=no

   AC_VMW_CHECK_LIB([$CUSTOM_PROCPS_NAME],
                    [PROCPS],
                    [],
                    [],
                    [],
                    [],
                    [getstat],
                    [
                     have_procps=yes;
                    ],
                    [])

   if test "$have_procps" = "no"; then
      AC_VMW_CHECK_LIB([proc-3.2.8],
                       [PROCPS],
                       [],
                       [],
                       [],
                       [],
                       [getstat],
                       [
                        have_procps=yes;
                       ],
                       [])
   fi

   if test "$have_procps" = "no"; then
      AC_VMW_CHECK_LIB([proc-3.2.7],
                       [PROCPS],
                       [],
                       [],
                       [],
                       [],
                       [getstat],
                       [],
                       [AC_MSG_ERROR([libproc not found. Please configure without procps (using --without-procps) or install procps - http://procps.sourceforge.net])])
   fi
fi

if test "$with_procps" != "yes"; then
AC_DEFINE([NO_PROCPS], 1, [Define to 1 if building without procps.])
fi

AC_ARG_WITH([dnet],
	    [AS_HELP_STRING([--without-dnet],
	    [compiles without libdnet (disables support for nicinfo)])],
	    [],
	    [with_dnet=yes])

have_dnet="no"
if test "$with_dnet" = "yes"; then
	# On Debian, dnet is installed via the libdumbnet package. We need to
	# detect this so that our source files include dumbnet.h instead of
	# dnet.h, which is part of a different package altogether.
   AC_VMW_CHECK_LIB([dumbnet],
                    [DNET],
                    [],
                    [dumbnet-config],
                    [],
                    [dumbnet.h],
                    [intf_open],
                    [have_dnet="yes";
                     AC_DEFINE([DNET_IS_DUMBNET], 1, [Define to 1 if substituting Debian's libdumbnet for libdnet.])],
                    [])

   if test $have_dnet = "no"; then
      AC_VMW_CHECK_LIB([dnet],
                       [DNET],
                       [],
                       [dnet-config],
                       [],
                       [dnet.h],
                       [intf_open],
                       [have_dnet="yes"],
                       [])
   fi

   if test $have_dnet = "no"; then
		AC_MSG_ERROR(
		   [dnet-config was not found on your PATH. Please configure without dnet (using --without-dnet) or install dnet - http://libdnet.sourceforge.net])
	fi
fi

if test "$with_dnet" != "yes"; then
AC_DEFINE([NO_DNET], 1, [Define to 1 if building without libdnet.])
fi

AC_ARG_WITH([icu],
            [AS_HELP_STRING([--without-icu],
              [disables support for ICU])],
            [],
            [with_icu=yes])

if test "$have_x" = "yes" -o "$with_icu" = "yes"; then
   AC_CHECK_TOOL([have_cxx], [$CXX], [no])
   if test "$have_cxx" = "no"; then
      AC_MSG_ERROR([C++ compiler not found. Make sure you have a C++ compiler installed or configure without X11 (using --without-x) and without ICU (using --without-icu).])
   fi
fi

if test "$with_icu" = "yes"; then
   AC_VMW_CHECK_LIBXX([icuuc],
                      [ICU],
                      [],
                      [icu-config],
                      [],
                      [unicode/utf.h],
                      [],
                      [ICU_CPPFLAGS="$ICU_CPPFLAGS -DUSE_ICU"],
                      [AC_MSG_ERROR([ICU library not found. Please configure without ICU (using --without-icu) or install ICU - http://www.icu-project.org])])
   # Check whether we have ICU >= 3.8.
   AC_LANG_PUSH([C++])
   AC_MSG_CHECKING([for ucasemap_utf8ToTitle in ICU])
   ORIGINAL_CPPFLAGS="$CPPFLAGS"
   CPPFLAGS="$CPPFLAGS $ICU_CPPFLAGS"
   AC_TRY_COMPILE([#include <unicode/ucasemap.h>],
                  [
                     (void) &ucasemap_utf8ToTitle;
                     return 0;
                  ],
                  [
                     ICU_CPPFLAGS="$ICU_CPPFLAGS -DHAVE_ICU_38"
                     AC_MSG_RESULT([yes])
                  ],
                  [AC_MSG_RESULT([no])])
   CPPFLAGS="$ORIGINAL_CPPFLAGS"
   AC_LANG_POP([C++])

   # Easier to give all modules the ICU defines/includes...
   CPPFLAGS="$CPPFLAGS $ICU_CPPFLAGS"
else
   CPPFLAGS="$CPPFLAGS -DNO_ICU"
fi

AC_PATH_PROG(
   [RPCGEN],
   [rpcgen],
   [ AC_MSG_ERROR([rpcgen not found. Please install the libc devel package.]) ])

###
### Headers
###

AC_CHECK_HEADERS([crypt.h])
AC_CHECK_HEADERS([inttypes.h])
AC_CHECK_HEADERS([stdint.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([wchar.h])
AC_CHECK_HEADERS([sys/inttypes.h])
AC_CHECK_HEADERS([sys/io.h])
AC_CHECK_HEADERS([sys/param.h]) # Required to make the sys/user.h check work correctly on FreeBSD
AC_CHECK_HEADERS([sys/sysinfo.h])
AC_CHECK_HEADERS([sys/types.h])
AC_CHECK_HEADERS([sys/user.h],
   [],
   [],
   [
#ifdef HAVE_SYS_PARAM_H
#   include <sys/param.h>
#endif
   ])
AC_CHECK_HEADERS([sys/vfs.h])
AC_CHECK_HEADERS([syslimits.h])
AC_CHECK_HEADERS([unwind.h])

AC_CHECK_HEADER(
   [wchar.h],
   [HAVE_WCHAR_H="yes"],
   [HAVE_WCHAR_H="no"])

if test "$os" = "linux"; then
   # Make sure kernel-headers package is installed.
   AC_CHECK_HEADER(
      [linux/unistd.h],
      [],
      [AC_MSG_ERROR(linux/unistd.h is not found. Please install kernel-headers/linux-userspace-headers/linux-libc-dev package.)])
fi

if test "$enable_multimon" != "no"; then
   AC_CHECK_HEADER(
      [X11/extensions/panoramiXproto.h],
      [],
      [AC_MSG_ERROR(
         [panoramiXproto.h not found. Please configure without multimon (using --disable-multimon) or install the libXinerama devel package(s).])],
      [#include <X11/X.h>
       #include <X11/Xmd.h>])
fi

bsdPrintfWrappers=no
if test "$os" = "linux"; then
   AC_CHECK_LIB([c],
                [ecvt],
                [bsdPrintfWrappers=yes],
                [])
fi

###
### Typdefs, structs, and compiler quarks.
###
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_VOLATILE

###
### Specific features and OS/arch flags / actions
###

### General flags / actions
CFLAGS="$CFLAGS -Wall"
CFLAGS="$CFLAGS -Werror"

# -Wno-unknown-pragmas is due to gcc not understanding '#pragma ident' in Xlib.h on OpenSolaris.
for TEST_CFLAG in -Wno-pointer-sign -Wno-unused-value -fno-strict-aliasing \
    -Wno-unknown-pragmas -Wno-uninitialized; do
    AC_MSG_CHECKING([for GCC flag $TEST_CFLAG])
    ORIGINAL_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS $TEST_CFLAG"
    NEW_CFLAG=""
    AC_TRY_COMPILE(
       [],
       [
	return 0;
       ],
   [NEW_CFLAG=" $TEST_CFLAG"
    AC_MSG_RESULT(yes)],
   [AC_MSG_RESULT(no)])
   CFLAGS="$ORIGINAL_CFLAGS$NEW_CFLAG"
done
CPPFLAGS="$CPPFLAGS -DVMX86_TOOLS"
CPPFLAGS="$CPPFLAGS"

# -fvisibility is used by "core service" plugins, but not required.
ORIGINAL_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -fvisibility=hidden"
AC_MSG_CHECKING([for GCC flag -fvisibility])
AC_TRY_COMPILE([],
               [return 0;],
               [PLUGIN_CPPFLAGS="-fvisibility=hidden -DGCC_EXPLICIT_EXPORT";
                AC_MSG_RESULT(yes)],
               [AC_MSG_RESULT(no)])
CFLAGS="$ORIGINAL_CFLAGS"

# Detect "unused-but-set-variable" gcc warning and disable it.
ORIGINAL_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wno-unused-but-set-variable"
AC_MSG_CHECKING([for GCC flag -Wno-unused-but-set-variable])
AC_TRY_COMPILE([],
               [return 0;],
               [ORIGINAL_CFLAGS="$ORIGINAL_CFLAGS -Wno-unused-but-set-variable";
                AC_MSG_RESULT(yes)],
               [AC_MSG_RESULT(no)])
CFLAGS="$ORIGINAL_CFLAGS"


BUILDDIR="`pwd`"

INCLUDE_DIR="`cd $srcdir; pwd`/lib/include"
BLD_INCLUDE_DIR="$BUILDDIR/lib/include"
CPPFLAGS="-I$INCLUDE_DIR -I$BLD_INCLUDE_DIR $CPPFLAGS"

###
### Documentation.
###

if test "$enable_docs" = "yes"; then
   AC_CHECK_PROG([have_doxygen],
                 [doxygen],
                 [yes],
                 [no])
   if test "$have_doxygen" = "no"; then
      AC_MSG_WARN([doxygen not found; API documentation will not be generated.])
   else
      AC_PATH_PROG([DOT], [dot], [])
      if test "$DOT" = ""; then
         HAVE_DOT=NO
      else
         DOT=`dirname $DOT`
         HAVE_DOT=YES
      fi
      AC_SUBST([DOT])
      AC_SUBST([HAVE_DOT])

      AC_PATH_PROG([MSCGEN],
                   [mscgen],
                   [no])
      if test "$MSCGEN" != "no"; then
         MSCGEN_DIR="`dirname $MSCGEN`"
      else
         MSCGEN_DIR=
      fi
      AC_SUBST([MSCGEN_DIR])
   fi
fi

###
### OS/arch-specific flags / actions
###

MODULES=""
MODULES_OS="$os"
TARGET_OS="$os"
MODULES_DIR=""
buildHgfsmounter=no

if test "$have_glib_2_14" = "yes"; then
   CPPFLAGS="$CPPFLAGS -DHAVE_GLIB_REGEX"
fi

if test "$os" = "linux"; then
   MODULES_DIR="$LINUXDIR/kernel/"

   CPPFLAGS="$CPPFLAGS -D_FILE_OFFSET_BITS=64"
   CPPFLAGS="$CPPFLAGS -D_LARGEFILE64_SOURCE"
   CPPFLAGS="$CPPFLAGS -D_XOPEN_SOURCE=500"
   CPPFLAGS="$CPPFLAGS -D_BSD_SOURCE"
   CPPFLAGS="$CPPFLAGS -D_SVID_SOURCE"

   LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD -lrt"

   MODULES="$MODULES vmsync vmci vsock"
   MODULES="$MODULES vmxnet vmblock vmhgfs"
   buildHgfsmounter=yes
fi

if test "$os" = "freebsd"; then
   LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD -lkvm"
   MODULES_DIR="/boot/modules"

   if test "$osVersion" -ge 302000; then
      MODULES="$MODULES vmmemctl"
   fi
   if test "$osVersion" -ge 409000; then
      MODULES="$MODULES vmxnet"
   fi
   if test "$osVersion" -ge 600000; then
      MODULES="$MODULES vmblock vmhgfs"
      buildHgfsmounter=yes
   fi

   if test "$with_kernel_modules" = "yes"; then
      echo "****************************************************************"
      echo "   You are building FreeBSD kernel modules. Make sure you use   "
      echo "   'make' to build open-vm-tools, and not GNU make ('gmake').   "
      echo "****************************************************************"
   fi
fi

if test "$os" = "solaris"; then
   LIB_IMPERSONATE_CPPFLAGS="$LIB_IMPERSONATE_CPPFLAGS -D_POSIX_PTHREAD_SEMANTICS"
   LIB_USER_CPPFLAGS="$LIB_USER_CPPFLAGS -D_POSIX_PTHREAD_SEMANTICS"

   LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD -lsocket"
   LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD -lnsl"
   LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD -lresolv"
   LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD -lrpcsvc"

   # Setup defines to identify the OS version.
   if test "$osVersion" -eq 509000; then
      CPPFLAGS="$CPPFLAGS -DSOL9"
   fi
   if test "$osVersion" -eq 510000; then
      CPPFLAGS="$CPPFLAGS -DSOL10"
   fi
   if test "$osVersion" -eq 511000; then
      CPPFLAGS="$CPPFLAGS -DSOL11"
   fi

   MODULES="$MODULES vmxnet vmmemctl"

   # HGFS and vmblock need Solaris 10 at least.
   if test "$osVersion" -ge 510000; then
      MODULES="$MODULES vmhgfs vmblock"
   fi

   # vmxnet3 is built on Solaris 10 / 11 only if GLDv3 is installed.
   if test "$osVersion" -gt 510000; then
      AC_CHECK_HEADER(
         [sys/mac.h],
         [MODULES="$MODULES vmxnet3"],
         [AC_MSG_WARN([GLDv3 (sys/mac.h) is not installed, vmxnet3 will not be compiled.])])
   fi

   if test "$with_kernel_modules" = "yes"; then
      echo "****************************************************************"
      echo "   You are building Solaris kernel modules. Make sure you use   "
      echo "   GNU make to build open-vm-tools.                             "
      echo "****************************************************************"
   fi
fi

AM_CONDITIONAL(BUILD_HGFSMOUNTER, test "$buildHgfsmounter" = "yes")
AM_CONDITIONAL(LINUX, test "$os" = "linux")
AM_CONDITIONAL(SOLARIS, test "$os" = "solaris")
AM_CONDITIONAL(FREEBSD, test "$os" = "freebsd")
AM_CONDITIONAL(FREEBSD_CUSTOM_SYSDIR, test "$os" = "freebsd" -a -n "$SYSDIR")
AM_CONDITIONAL(THIRTY_TWO_BIT_USERSPACE, test "$userSpaceBitness" = "32")
AM_CONDITIONAL(HAVE_X11, test "$have_x" = "yes")
AM_CONDITIONAL(HAVE_ICU, test "$with_icu" = "yes")
AM_CONDITIONAL(WITH_KERNEL_MODULES, test "$with_kernel_modules" = "yes")
AM_CONDITIONAL(HAVE_XSM, test "$have_xsm" = "yes")
AM_CONDITIONAL(HAVE_XCOMPOSITE, test "$have_xcomposite" = "yes")
AM_CONDITIONAL(ENABLE_TESTS, test "$have_cunit" = "yes")
AM_CONDITIONAL(WITH_ROOT_PRIVILEGES, test "$with_root_privileges" = "yes")
AM_CONDITIONAL(HAVE_DNET, test "$have_dnet" = "yes")
AM_CONDITIONAL(HAVE_DOXYGEN, test "$have_doxygen" = "yes")
AM_CONDITIONAL(HAVE_FUSE, test "$have_fuse" = "yes")
AM_CONDITIONAL(HAVE_GNU_LD, test "$with_gnu_ld" = "yes")
AM_CONDITIONAL(HAVE_GTKMM, test "$have_x" = "yes" -a "$with_gtkmm" = "yes")
AM_CONDITIONAL(HAVE_PAM, test "$with_pam" = "yes")
AM_CONDITIONAL(USE_SLASH_PROC, test "os" = "linux" -a "$have_glib_2_14" = "yes")
AM_CONDITIONAL(USE_PRINTF_WRAPPERS, test "$bsdPrintfWrappers" = "yes")

if test "$have_xsm" != "yes"; then
AC_DEFINE([NO_XSM], 1, [])
fi

if test "$have_xcomposite" != "yes"; then
   AC_DEFINE([NO_XCOMPOSITE])
fi

### Feature-specific flags / actions
# Combine where possible

# If control reaches this point and multimon is still enabled, then we know
# all of the tests for required components have passed and it's safe to allow
# multimon. Otherwise, it should be disabled.
if test "$enable_multimon" = "no"; then
   # XXX: For consistency, change this to ENABLE_MULTIMON. This will require
   # some additional code cleanup.
   AC_DEFINE([NO_MULTIMON], 1, [Define to 1 if building without multimon support.])
fi

LIB_AUTH_CPPFLAGS="$LIB_AUTH_CPPFLAGS $PAM_CPPFLAGS"
if test "$HAVE_CRYPT" = "yes"; then
   LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD -lcrypt"
   VIX_LIBADD="$VIX_LIBADD -lcrypt"
fi


LIBVMTOOLS_LIBADD="$LIBVMTOOLS_LIBADD $THREAD_LIB"
VIX_LIBADD="$VIX_LIBADD $THREAD_LIB"

### Core Services definitions.

HGFS_LIBS="$BUILDDIR/libhgfs/libhgfs.la"

VMTOOLS_LIBS="$BUILDDIR/libvmtools/libvmtools.la $GLIB2_LIBS"
VMTOOLS_CPPFLAGS="-DVMTOOLS_USE_GLIB $GLIB2_CPPFLAGS"

PLUGIN_CPPFLAGS="$VMTOOLS_CPPFLAGS $PLUGIN_CPPFLAGS"
PLUGIN_LDFLAGS="-Wl,-z,defs -Wl,-lc -shared -module -avoid-version"

# In Solaris, the XDR-related functions are not in libc like in Linux and
# FreeBSD, so binaries need to be linked to some extra libraries.
XDR_LIBS=
if test "$os" = "solaris"; then
   XDR_LIBS="-lnsl -lrpcsvc"
fi

# Installation directories for core services plugins.
TEST_PLUGIN_INSTALLDIR=$datadir/open-vm-tools/tests
COMMON_PLUGIN_INSTALLDIR=$libdir/open-vm-tools/plugins/common
VMSVC_PLUGIN_INSTALLDIR=$libdir/open-vm-tools/plugins/vmsvc
VMUSR_PLUGIN_INSTALLDIR=$libdir/open-vm-tools/plugins/vmusr

# General definitions
INSTVMSG='$(SHELL) $(top_srcdir)/scripts/build/instvmsg.sh'
RPCGEN_WRAPPER='$(SHELL) $(top_builddir)/scripts/build/rpcgen_wrapper.sh'

### General substs

AC_SUBST([HGFS_LIBS])
AC_SUBST([TOOLS_VERSION])
AC_SUBST([TARGET_OS])
AC_SUBST([KERNEL_RELEASE])
AC_SUBST([LINUXINCLUDE])
AC_SUBST([MODULES_OS])
AC_SUBST([MODULES_DIR])
AC_SUBST([MODULES])
AC_SUBST([COMMON_XLIBS])
AC_SUBST([XSM_LIBS])
AC_SUBST([XCOMPOSITE_LIBS])
AC_SUBST([PAM_PREFIX])
AC_SUBST([PLUGIN_CPPFLAGS])
AC_SUBST([PLUGIN_LDFLAGS])
AC_SUBST([VMTOOLS_CPPFLAGS])
AC_SUBST([VMTOOLS_LIBS])
AC_SUBST([RPCGENFLAGS])
AC_SUBST([XDR_LIBS])
AC_SUBST([TEST_PLUGIN_INSTALLDIR])
AC_SUBST([COMMON_PLUGIN_INSTALLDIR])
AC_SUBST([VMSVC_PLUGIN_INSTALLDIR])
AC_SUBST([VMUSR_PLUGIN_INSTALLDIR])
if test "$os" = "freebsd" -a -n "$SYSDIR"; then
   # If SYSDIR is not defined, AC_SUBST expands to nothing, so we need something
   # inside this block.
   true
   AC_SUBST([SYSDIR])
fi
AC_SUBST([INSTVMSG])
AC_SUBST([RPCGEN_WRAPPER])

### Lib substs

AC_SUBST([LIB_AUTH_CPPFLAGS])
AC_SUBST([LIB_IMPERSONATE_CPPFLAGS])
AC_SUBST([LIB_USER_CPPFLAGS])
AC_SUBST([LIBVMTOOLS_LIBADD])

### Program substs

AC_SUBST([VIX_LIBADD])

###
### Create the Makefiles
###
AC_CONFIG_FILES([                      \
   Makefile                            \
   lib/Makefile                        \
   lib/appUtil/Makefile                \
   lib/auth/Makefile                   \
   lib/backdoor/Makefile               \
   lib/dict/Makefile                   \
   lib/dynxdr/Makefile                 \
   lib/err/Makefile                    \
   lib/file/Makefile                   \
   lib/foundryMsg/Makefile             \
   lib/glibUtils/Makefile              \
   lib/guestApp/Makefile               \
   lib/guestRpc/Makefile               \
   lib/hgfs/Makefile                   \
   lib/hgfsBd/Makefile                 \
   lib/hgfsHelper/Makefile             \
   lib/hgfsServer/Makefile             \
   lib/hgfsServerManagerGuest/Makefile \
   lib/hgfsServerPolicyGuest/Makefile  \
   lib/impersonate/Makefile            \
   lib/lock/Makefile                   \
   lib/message/Makefile                \
   lib/misc/Makefile                   \
   lib/netUtil/Makefile                \
   lib/panic/Makefile                  \
   lib/panicDefault/Makefile           \
   lib/printer/Makefile                \
   lib/procMgr/Makefile                \
   lib/rpcChannel/Makefile             \
   lib/rpcIn/Makefile                  \
   lib/rpcOut/Makefile                 \
   lib/rpcVmx/Makefile                 \
   lib/slashProc/Makefile              \
   lib/string/Makefile                 \
   lib/stubs/Makefile                  \
   lib/syncDriver/Makefile             \
   lib/system/Makefile                 \
   lib/unicode/Makefile                \
   lib/user/Makefile                   \
   lib/vmCheck/Makefile                \
   lib/vmSignal/Makefile               \
   lib/wiper/Makefile                  \
   lib/xdg/Makefile                    \
   services/Makefile                   \
   services/vmtoolsd/Makefile          \
   services/plugins/Makefile           \
   services/plugins/desktopEvents/Makefile \
   services/plugins/dndcp/Makefile     \
   services/plugins/guestInfo/Makefile \
   services/plugins/guestInfo/getlib/Makefile \
   services/plugins/hgfsServer/Makefile \
   services/plugins/powerOps/Makefile  \
   services/plugins/resolutionSet/Makefile \
   services/plugins/timeSync/Makefile  \
   services/plugins/vix/Makefile       \
   services/plugins/vmbackup/Makefile  \
   vmware-user-suid-wrapper/Makefile   \
   toolbox/Makefile                    \
   hgfsclient/Makefile                 \
   hgfsmounter/Makefile                \
   checkvm/Makefile                    \
   rpctool/Makefile                    \
   libguestlib/Makefile                \
   libguestlib/vmguestlib.pc           \
   libhgfs/Makefile                    \
   libvmtools/Makefile                 \
   xferlogs/Makefile                   \
   modules/Makefile                    \
   vmblock-fuse/Makefile               \
   vmblockmounter/Makefile             \
   tests/Makefile                      \
   tests/vmrpcdbg/Makefile             \
   tests/testDebug/Makefile            \
   tests/testPlugin/Makefile           \
   tests/testVmblock/Makefile          \
   docs/Makefile                       \
   docs/api/Makefile                   \
   scripts/Makefile		               \
   scripts/build/rpcgen_wrapper.sh     \
])

###
### Output
###
AC_OUTPUT
