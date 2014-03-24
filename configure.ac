AC_PREREQ(2.62)

m4_define(totem_version_major, 3)
m4_define(totem_version_minor, 10)
m4_define(totem_version_micro, 2)

AC_INIT([totem-pl-parser],
        [totem_version_major.totem_version_minor.totem_version_micro],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=totem-pl-parser&component=General])

AC_CONFIG_SRCDIR([plparse/totem-pl-parser.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.9 dist-xz no-dist-gzip check-news subdir-objects])
AM_MAINTAINER_MODE([enable])
AC_USE_SYSTEM_EXTENSIONS

# Enable silent build when available (Automake 1.11)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

dnl Add the languages which your application supports to po/LINGUAS
GETTEXT_PACKAGE=totem-pl-parser
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, ["$GETTEXT_PACKAGE"], [Define to the Gettext package name])
AC_SUBST(GETTEXT_PACKAGE)
IT_PROG_INTLTOOL([0.35.0])

AC_PROG_CXX
AM_PROG_CC_C_O

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT([])

PKG_PROG_PKG_CONFIG

dnl Use local version of asprintf when not on the current system.
AC_CHECK_FUNCS(asprintf)
AC_CONFIG_LIBOBJ_DIR([lib])
AC_REPLACE_FUNCS([asprintf])

AC_PATH_PROG([GLIB_GENMARSHAL],[glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])

# Requirements
GLIB_REQS=2.31.0
GIO_REQS=2.24.0
QUVI_REQS=0.9.1
LIBARCHIVE_REQS=3.0
LIBSOUP_REQS=2.43.0

# Before making a release, the PLPARSER_LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A
PLPARSER_LT_VERSION=19:0:1
AC_SUBST(PLPARSER_LT_VERSION)

TOTEM_PL_PARSER_VERSION_MAJOR=totem_version_major
TOTEM_PL_PARSER_VERSION_MINOR=totem_version_minor
TOTEM_PL_PARSER_VERSION_MICRO=totem_version_micro
AC_SUBST(TOTEM_PL_PARSER_VERSION_MAJOR)
AC_SUBST(TOTEM_PL_PARSER_VERSION_MINOR)
AC_SUBST(TOTEM_PL_PARSER_VERSION_MICRO)

pkg_modules="glib-2.0 >= $GLIB_REQS libxml-2.0 gthread-2.0 gio-2.0 >= $GIO_REQS libsoup-2.4 >= $LIBSOUP_REQS"

##################################
# Checking libgmime dependency
##################################
gmime_message=""

AC_ARG_ENABLE(gmime-i-know-what-im-doing,
	      AS_HELP_STRING([--disable-gmime-i-know-what-im-doing],
			     [Disable libgmime (Unsupported, breaks Podcast support).]),
			     [enable_gmime=no],
			     [enable_gmime=yes])

if test "x$enable_gmime" = "xyes" ; then
   PKG_CHECK_MODULES(GMIME,  
		       gmime-2.6,
		       [have_gmime=yes GMIME=gmime-2.6], 
		       [have_gmime=no])

   if test "x$have_gmime" = "xyes" ; then
      AC_SUBST(GMIME, $GMIME)
      AC_SUBST(USEGMIME, yes)
      AC_DEFINE(HAVE_GMIME, 1, [GMime available in the system])
      pkg_modules="$pkg_modules $GMIME"
   else
      AC_MSG_ERROR([libgmime is required to compile totem-pl-parser.])
   fi
else
   AC_SUBST(USEGMIME, no)
   gmime_message="
 **************************************************************
 Compiling totem-pl-parser without libgmime.
 This will break podcast support and is completely unsupported.
 **************************************************************"
fi

##################################
# Checking quvi dependency
##################################

QUVI=""
AC_ARG_ENABLE(quvi,
	      AS_HELP_STRING([--enable-quvi],
			     [Enable libquvi support (default is auto).]),
			     [],
			     [enable_quvi=auto])
if test "x$enable_quvi" != "xno" ; then
	PKG_CHECK_MODULES(QUVI,
			  libquvi-0.9 >= $QUVI_REQS glib-2.0,
			  [have_quvi=yes], [have_quvi=no])
	if test "x$enable_quvi" = "xyes" -a "x$have_quvi" = "xno" ; then
		AC_MSG_ERROR([Quvi support requested but not available.])
	fi
	if test "x$have_quvi" = "xyes" ; then
		QUVI="libquvi-0.9"
		AC_DEFINE(HAVE_QUVI, 1, [libquvi available in the system])
	fi
fi
AC_SUBST(QUVI, $QUVI)
AM_CONDITIONAL([HAVE_QUVI], [test "$have_quvi" = "yes"])

##################################
# Checking libarchive dependency
##################################

ARCHIVE=""
AC_ARG_ENABLE(libarchive,
	      AS_HELP_STRING([--enable-libarchive],
			     [Enable libarchive support (default is auto).]),
			     [],
			     [enable_libarchive=auto])
if test "x$enable_libarchive" != "xno" ; then
	PKG_CHECK_MODULES(ARCHIVE,
			  libarchive >= $LIBARCHIVE_REQS,
			  [have_libarchive=yes], [have_libarchive=no])
	if test "x$enable_libarchive" = "xyes" -a "x$have_libarchive" = "xno" ; then
		AC_MSG_ERROR([Libarchive support requested but not available.])
	fi
	if test "x$have_libarchive" = "xyes" ; then
		pkg_modules="$pkg_modules libarchive"
		ARCHIVE="libarchive"
		AC_DEFINE(HAVE_LIBARCHIVE, 1, [libarchive available in the system])
	fi
fi
AC_SUBST(ARCHIVE, $ARCHIVE)

################################
# Checking libgcrypt dependency
################################

AM_PATH_LIBGCRYPT([],[have_libgcrypt=yes],[have_libgcrypt=no])

ARCHIVE=""
AC_ARG_ENABLE(libgcrypt,
	      AS_HELP_STRING([--enable-libgcrypt],
			     [Enable libgcrypt support (default is auto).]),
			     [],
			     [enable_libgcrypt=auto])
if test "x$enable_libgcrypt" != "xno" ; then
	if test "x$enable_libgcrypt" = "xyes" -a "x$have_libgcrypt" = "xno" ; then
		AC_MSG_ERROR([Libgcrypt support requested but not available.])
	fi
	if test "x$have_libgcrypt" = "xyes" ; then
		AC_DEFINE(HAVE_LIBGCRYPT, 1, [libgcrypt available in the system])
		use_libgcrypt=yes
	fi
fi

dnl Check for packages for building libtotem-plparser.la
PKG_CHECK_MODULES(TOTEM_PLPARSER, [$pkg_modules])
AC_SUBST(TOTEM_PLPARSER_CFLAGS)
AC_SUBST(TOTEM_PLPARSER_LIBS)

PKG_CHECK_MODULES([TOTEM_PLPARSER_MINI], [gio-2.0])

GNOME_DEBUG_CHECK
GNOME_COMPILE_WARNINGS([maximum])
GNOME_CXX_WARNINGS
GNOME_MAINTAINER_MODE_DEFINES

if test "$GCC" = "yes" ; then
	CFLAGS="\
	-Wall \
	-Wchar-subscripts -Wmissing-declarations -Wmissing-prototypes \
	-Wnested-externs -Wpointer-arith \
	-Wcast-align -Wsign-compare \
	-fno-strict-aliasing \
	$CFLAGS"
fi

AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CXXFLAGS])
AC_SUBST([AM_LDFLAGS])

GTK_DOC_CHECK([1.14],[--flavour no-tmpl])
GOBJECT_INTROSPECTION_CHECK([0.9.5])
GNOME_CODE_COVERAGE

AC_CONFIG_FILES([
Makefile
totem-plparser.pc
totem-plparser-uninstalled.pc
totem-plparser-mini.pc
totem-plparser-mini-uninstalled.pc
totem-pl-parser.spec
lib/Makefile
plparse/Makefile
plparse/tests/Makefile
plparse/totem-pl-parser-features.h
po/Makefile.in
docs/Makefile
docs/reference/Makefile
docs/reference/version.xml
])


AC_OUTPUT

if test "x$have_quvi" = "xyes"; then
	AC_MSG_NOTICE([** Quvi video link parsing enabled])
else
	AC_MSG_NOTICE([   Quvi video link parsing disabled])
fi
if test "x$have_libarchive" = "xyes"; then
	AC_MSG_NOTICE([** ISO detection with libarchive enabled])
else
	AC_MSG_NOTICE([   ISO detection with libarchive disabled])
fi
if test "x$use_libgcrypt" = "xyes"; then
	AC_MSG_NOTICE([** AmazonAMZ decoding with libgcrypt enabled])
else
	AC_MSG_NOTICE([   AmazonAMZ decoding with libgcrypt disabled])
fi

echo "
$gmime_message
"
