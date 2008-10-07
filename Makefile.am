SUBDIRS = lib plparse po docs

EXTRA_DIST =			\
	COPYING.LIB		\
	autogen.sh		\
	AUTHORS NEWS		\
	gtk-doc.make		\
	totem-plparser.pc.in	\
	totem-plparser-mini.pc	\
	totem-pl-parser.spec

CLEANFILES = totem-plparser.pc

DISTCLEANFILES = intltool-extract intltool-merge intltool-update \
		 gnome-doc-utils.make gtk-doc.make

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = totem-plparser.pc totem-plparser-mini.pc

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper --enable-gtk-doc
