totem_glibc_sources = ['stub.c']

if not cc.has_function('asprintf')
  # FIXME: this is untested
  totem_glibc_sources += ['asprintf.c']
  cdata.set('asprintf', 'totem_private_asprintf')
else
  cdata.set('HAVE_ASPRINTF', true)
endif

totem_glibc_lib = static_library('totem_glibc',
                                 totem_glibc_sources,
                                 install: false)
