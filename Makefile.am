SUBDIRS = lib plparse po

EXTRA_DIST =			\
	COPYING.LIB		\
	autogen.sh		\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	AUTHORS NEWS		\
	totem-plparser.pc.in	\
	totem-plparser-mini.pc

CLEANFILES = totem-plparser.pc

DISTCLEANFILES = intltool-extract intltool-merge intltool-update \
		 gnome-doc-utils.make

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = totem-plparser.pc totem-plparser-mini.pc

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper
