{
  'targets': [
    {
      'target_name': 'jansson',
      'type': '<(library)',
      'include_dirs': [
        './src',
      ],
      'sources': [
        'src/dump.c',
        'src/error.c',
        'src/hashtable.c',
        'src/hashtable.h',
        'src/jansson_private.h',
        'src/load.c',
        'src/memory.c',
        'src/pack_unpack.c',
        'src/strbuffer.c',
        'src/strbuffer.h',
        'src/strconv.c',
        'src/utf.c',
        'src/utf.h',
        'src/value.c',
      ],
      'ldflags': [
        '-no-undefined',
        '-export-symbols-regex \'^json_\'',
        '-version-info 8:0:4',
      ]
    },
  ],
}
