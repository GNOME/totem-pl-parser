SUBDIRS = lib plparse po docs

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

EXTRA_DIST =			\
	COPYING.LIB		\
	autogen.sh		\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	AUTHORS NEWS		\
	gtk-doc.make		\
	totem-plparser.pc.in	\
	totem-plparser-mini.pc	\
	totem-pl-parser.spec	\
	totem-pl-parser.doap	\
	MAINTAINERS		\
	$(NULL)

CLEANFILES = totem-plparser.pc

DISTCLEANFILES = intltool-extract intltool-merge intltool-update \
		 gtk-doc.make

MAINTAINERCLEANFILES = \
	INSTALL \
	aclocal.m4 \
	compile \
	config.guess \
	config.h.in \
	config.sub \
	depcomp \
	install-sh \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	ltmain.sh \
	missing \
	gtk-doc.make \
	ChangeLog \
	`find "$(srcdir)" -type f -name Makefile.in -print` \
	$(NULL)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = totem-plparser.pc totem-plparser-mini.pc

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper --enable-gtk-doc --enable-introspection

# Build ChangeLog from GIT history
ChangeLog:
	@if test -f $(top_srcdir)/.git/HEAD; then \
		git log --stat --after="Mon Jul 20 23:47:57 2009" > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
