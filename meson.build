project('totem-pl-parser', 'c',
        version: '3.26.6', # Don't forget to also change plparser_lt_version!
        default_options: [
          'buildtype=debugoptimized',
          'warning_level=1',
        ],
        meson_version : '>= 0.40.1',
        license: 'LGPLv2.0+')

# Before making a release, the plparser_lt_version string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A
plparser_lt_version='21:5:3'

plparse_version       = meson.project_version()
plparse_major_version = plparse_version.split('.')[0].to_int()
plparse_minor_version = plparse_version.split('.')[1].to_int()
plparse_micro_version = plparse_version.split('.')[2].to_int()

plparse_lt_current  = plparser_lt_version.split(':')[0].to_int()
plparse_lt_revision = plparser_lt_version.split(':')[1].to_int()
plparse_lt_age      = plparser_lt_version.split(':')[2].to_int()
plparse_soversion   = plparse_lt_current - plparse_lt_age
# Convert the lt_version to libtool library naming to maintain ABI compat with
# the old Autotools build. This can be changed with the next ABI break.
plparse_libversion  = '@0@.@1@.@2@'.format(plparse_soversion, plparse_lt_age, plparse_lt_revision)

# Requirements
glib_req    = '>= 2.56.0'
gio_req     = '>= 2.24.0'
archive_req = '>= 3.0'

# Dependencies
glib_dep = dependency('glib-2.0', version : glib_req)
gthread_dep = dependency('gthread-2.0', version : glib_req)
gio_dep = dependency('gio-2.0', version : gio_req)
xml_dep = dependency('libxml-2.0')

totem_pl_parser_deps = [glib_dep, xml_dep, gthread_dep, gio_dep]

# project-wide cflags
add_project_arguments('-D_GNU_SOURCE', language: 'c')

localedir = join_paths(get_option('prefix'), get_option('datadir'), 'locale')
add_project_arguments('-DGNOMELOCALEDIR="@0@"'.format(localedir), language: 'c')

# configuration data for config.h
cdata = configuration_data()
cdata.set_quoted('GETTEXT_PACKAGE', 'totem-pl-parser')

config_inc = include_directories('.')

# configuration data for pkg-config .pc.in files
pkgconf = configuration_data()

# common warning flags
cc = meson.get_compiler('c')

# Compiler flags
if cc.get_id() == 'msvc'
  # Compiler options taken from msvc_recommended_pragmas.h
  # in GLib, based on _Win32_Programming_ by Rector and Newcomer
  test_cflags = [
    '-we4002', # too many actual parameters for macro
    '-we4003', # not enough actual parameters for macro
    '-w14010', # single-line comment contains line-continuation character
    '-we4013', # 'function' undefined; assuming extern returning int
    '-w14016', # no function return type; using int as default
    '-we4020', # too many actual parameters
    '-we4021', # too few actual parameters
    '-we4027', # function declared without formal parameter list
    '-we4029', # declared formal parameter list different from definition
    '-we4033', # 'function' must return a value
    '-we4035', # 'function' : no return value
    '-we4045', # array bounds overflow
    '-we4047', # different levels of indirection
    '-we4049', # terminating line number emission
    '-we4053', # an expression of type void was used as an operand
    '-we4071', # no function prototype given
    '-we4819', # the file contains a character that cannot be represented in the current code page
  ]
elif cc.get_id() == 'gcc' or cc.get_id() == 'clang'
  test_cflags = [
    '-fno-strict-aliasing',
    '-Wpointer-arith',
    '-Wmissing-declarations',
    '-Wimplicit-function-declaration',
    '-Wformat=2',
    '-Wformat-nonliteral',
    '-Wformat-security',
    '-Wstrict-prototypes',
    '-Wmissing-prototypes',
    '-Wnested-externs',
    '-Wold-style-definition',
    '-Wundef',
    '-Wunused',
    '-Wcast-align',
    '-Wmissing-noreturn',
    '-Wmissing-format-attribute',
    '-Wmissing-include-dirs',
    '-Wlogical-op',
    '-Wno-uninitialized',
    '-Wno-shadow',
    '-Wno-int-conversion',
    '-Wno-discarded-qualifiers',
    '-Werror=redundant-decls',
    '-Werror=implicit',
    '-Werror=nonnull',
    '-Werror=init-self',
    '-Werror=main',
    '-Werror=missing-braces',
    '-Werror=sequence-point',
    '-Werror=return-type',
    '-Werror=trigraphs',
    '-Werror=array-bounds',
    '-Werror=write-strings',
    '-Werror=address',
    '-Werror=int-to-pointer-cast',
    '-Werror=pointer-to-int-cast',
    '-Werror=empty-body',
    '-Werror=write-strings',
    '-Werror=maybe-uninitialized',
  ]
else
  test_cflags = []
endif

extra_warning_cflags = []
foreach cflag: test_cflags
  if cc.has_argument(cflag)
    extra_warning_cflags += [ cflag ]
  endif
endforeach

# uchardet dependency
enable_uchardet = get_option('enable-uchardet')
have_uchardet = false
if enable_uchardet != 'no'
  uchardet_dep = dependency('uchardet', required: false)
  if enable_uchardet == 'yes' and not uchardet_dep.found()
    error('uchardet support requested but not available.')
  endif
  if uchardet_dep.found()
    cdata.set('HAVE_UCHARDET', true,
      description: 'uchardet available in the system')
    have_uchardet = true
    totem_pl_parser_deps += [uchardet_dep]
  endif
endif

# libarchive dependency
enable_libarchive = get_option('enable-libarchive')
have_libarchive = false
pkgconf.set('ARCHIVE', '')
if enable_libarchive != 'no'
  libarchive_dep = dependency('libarchive', version: archive_req, required: false)
  if enable_libarchive == 'yes' and not libarchive_dep.found()
    error('libarchive support requested but not available.')
  endif
  if libarchive_dep.found()
    pkgconf.set('ARCHIVE', 'libarchive')
    cdata.set('HAVE_LIBARCHIVE', true,
      description: 'libarchive available in the system')
    have_libarchive = true
    totem_pl_parser_deps += [libarchive_dep]
  endif
endif

# libgcrypt dependency
enable_libgcrypt = get_option('enable-libgcrypt')
have_libgcrypt = false
pkgconf.set('GCRYPT', '')
if enable_libgcrypt != 'no'
  # presented without further comment: https://dev.gnupg.org/T2037
  libgcrypt_dep = cc.find_library('gcrypt', required : false)

  if enable_libgcrypt == 'yes' and not libgcrypt_dep.found()
    error('libgcrypt support requested but not available.')
  endif
  if libgcrypt_dep.found()
    # FIXME: we just assume it's in the standard path. We can't really parse
    # the output of libgcrypt-config in any meaningful way, since if any of
    # the paths have spaces in them it's all going to break.
    pkgconf.set('LIBGCRYPT_LIBS', '-lgcrypt')
    pkgconf.set('LIBGCRYPT_CFLAGS', '')
    cdata.set('HAVE_LIBGCRYPT', true,
      description: 'libgcrypt available in the system')
    have_libgcrypt = true
    totem_pl_parser_deps += [libgcrypt_dep]
  endif
endif

# subdirs

plparser_inc = include_directories('plparse')

subdir('lib')
subdir('plparse')
subdir('po')
if get_option('enable-gtk-doc')
  subdir('docs/reference')
endif

# config.h
configure_file(output: 'config.h', configuration: cdata)

# pkg-config files
pkgconf.set('prefix', get_option('prefix'))
pkgconf.set('exec_prefix', '${prefix}')
pkgconf.set('libdir', '${prefix}/@0@'.format(get_option('libdir')))
pkgconf.set('includedir', '${prefix}/@0@'.format(get_option('includedir')))
pkgconf.set('VERSION', meson.project_version())

pkg_install_dir = join_paths(get_option('libdir'), 'pkgconfig')

pkg_files = [
  'totem-plparser.pc',
  'totem-plparser-mini.pc',
]

foreach pkg: pkg_files
  configure_file(input: pkg + '.in',
                 output: pkg,
                 configuration: pkgconf,
                 install_dir: pkg_install_dir)
endforeach

# configuration summary
message('''

    Configuration summary:

      ISO detection with libarchive     : @0@
      AmazonAMZ decoding with libgcrypt : @1@
      uchardet encoding detection       : @2@
'''.format(have_libarchive.to_string('yes', 'no'),
           have_libgcrypt.to_string('yes', 'no'),
           have_uchardet.to_string('yes', 'no')))

