include $(top_srcdir)/Makefile.decl

pwd=`pwd`
INCLUDES = \
	-I$(top_srcdir)/plparse	\
	-DTEST_FILE_DIR=\""$(pwd)/"\"

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS			+= parser
parser_SOURCES			 = parser.c
parser_LDADD			 = \
				$(progs_ldadd)		\
				$(TOTEM_PLPARSER_LIBS)	\
				../libtotem-plparser.la
parser_CPPFLAGS			 = \
				-I$(top_srcdir)			\
				-I$(top_srcdir)/lib		\
				-I$(top_builddir)/plparse	\
				-DGNOMELOCALEDIR=\""$(datadir)/locale"\"\
				$(DISABLE_DEPRECATED)		\
				$(AM_CPPFLAGS)
parser_CFLAGS			 = \
				$(TOTEM_PLPARSER_CFLAGS)\
				$(HAL_CFLAGS)		\
				$(DBUS_CFLAGS)		\
				$(WARN_CFLAGS)		\
				$(AM_CFLAGS)

TEST_PROGS			+= disc
disc_SOURCES			 = disc.c
disc_LDADD			 = \
				$(progs_ldadd)		\
				../libtotem-plparser.la	\
				$(TOTEM_PLPARSER_LIBS)
disc_CPPFLAGS			 = \
				$(common_defines)	\
				$(AM_CPPFLAGS)
disc_CFLAGS			 = \
				$(WARN_CFLAGS)		\
				$(TOTEM_PLPARSER_CFLAGS)\
				$(AM_CFLAGS)
disc_LDFLAGS			 = $(AM_LDFLAGS)
