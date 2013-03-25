{
  'targets': [
    {
      'target_name': 'libpomelo',
      'type': '<(library)',
      'dependencies': [
        'deps/uv/uv.gyp:libuv',
        'deps/jansson/jansson.gyp:jansson',
      ],
      'include_dirs': [
        './include',
        './deps/uv/include',
        './deps/jansson/src',
      ],
      'sources': [
        'include/pomelo-private/common.h',
        'include/pomelo-private/internal.h',
        'include/pomelo-private/listener.h',
        'include/pomelo-private/map.h',
        'include/pomelo-private/ngx-queue.h',
        'include/pomelo-private/transport.h',
        'include/pomelo-protobuf/pb-util.h',
        'include/pomelo-protobuf/pb.h',
        'include/pomelo-protocol/message.h',
        'include/pomelo-protocol/package.h',
        'include/pomelo.h',
        'src/client.c',
        'src/common.c',
        'src/listener.c',
        'src/map.c',
        'src/message.c',
        'src/msg-json.c',
        'src/msg-pb.c',
        'src/network.c',
        'src/package.c',
        'src/pb-decode.c',
        'src/pb-encode.c',
        'src/pb-util.c',
        'src/pkg-handshake.c',
        'src/pkg-heartbeat.c',
        'src/transport.c',
        'src/protocol.c',
        'src/thread.c',
      ],
      'conditions': [
          ['OS == "win"', {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'AdditionalOptions': [ '/TP' ],
                }
              },
              'defines': [
                '_WIN32',
                'WIN32',
                '_CRT_NONSTDC_NO_DEPRECATE',
                '_DEBUG',
                '_WINDOWS',
                '_USRDLL',
                'JANSSON_DLL_EXPORTS',
                '_WINDLL',
                '_UNICODE',
                'UNICODE'
              ],
              'link_settings': {
                'libraries': [
                  '-ladvapi32.lib',
                  '-liphlpapi.lib',
                  '-lpsapi.lib',
                  '-lshell32.lib',
                  '-lws2_32.lib'
                ],
              },
            }
          ],
          ['OS != "win"',{
            'defines':[
              '_LARGEFILE_SOURCE',
              '_FILE_OFFSET_BITS=64',
              '_GNU_SOURCE',
            ],
            'ldflags': [
              '-no-undefined',
              '-export-symbols-regex \'^json_\'',
              '-version-info 8:0:4',
            ]
          }
          ]
      ],
    },
    {
      'target_name': 'destroy',
      'type': 'executable',
      'dependencies': [
        'libpomelo',
      ],
      'include_dirs': [
        'include/',
        './deps/uv/include',
        './deps/jansson/src',
      ],
      'sources': [
        'example/destroy.c'
      ],
      'conditions' : [
        ['OS == "win"', {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'AdditionalOptions': [ '/TP' ],
              }
            },
            'defines': [
              'WIN32',
              '_CRT_NONSTDC_NO_DEPRECATE',
              '_DEBUG',
              '_WINDOWS',
              '_USRDLL',
              'JANSSON_DLL_EXPORTS',
              '_WINDLL',
              '_UNICODE',
              'UNICODE'
            ],
            'link_settings': {
              'libraries': [
                '-ladvapi32.lib',
                '-liphlpapi.lib',
                '-lpsapi.lib',
                '-lshell32.lib',
                '-lws2_32.lib'
              ],
            },
          }
        ],
        ['OS != "win" ',{
          'defines':[
              '_LARGEFILE_SOURCE',
              '_FILE_OFFSET_BITS=64',
              '_GNU_SOURCE',
          ]
        }
        ]
      ],
    },
    {
      'target_name': 'notify',
      'type': 'executable',
      'dependencies': [
        'libpomelo',
      ],
      'include_dirs': [
        'include/',
        './deps/uv/include',
        './deps/jansson/src',
      ],
      'sources': [
        'example/notify.c'
      ],
      'conditions':[
        ['OS == "win"', {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'AdditionalOptions': [ '/TP' ],
              }
            },
            'defines': [
              'WIN32',
              '_CRT_NONSTDC_NO_DEPRECATE',
              '_DEBUG',
              '_WINDOWS',
              '_USRDLL',
              'JANSSON_DLL_EXPORTS',
              '_WINDLL',
              '_UNICODE',
              'UNICODE'
            ],
            'link_settings': {
              'libraries': [
                '-ladvapi32.lib',
                '-liphlpapi.lib',
                '-lpsapi.lib',
                '-lshell32.lib',
                '-lws2_32.lib'
              ],
            },
          }
        ],
        ['OS != "win" ',{
          'defines':[
              '_LARGEFILE_SOURCE',
              '_FILE_OFFSET_BITS=64',
              '_GNU_SOURCE',
          ]
        }
        ]
      ]
    },
    {
      'target_name': 'request',
      'type': 'executable',
      'dependencies': [
        'libpomelo',
      ],
      'include_dirs': [
        'include/',
        './deps/uv/include',
        './deps/jansson/src',
      ],
      'sources': [
        'example/request.c'
      ],
      'conditions': [
        ['OS == "win"', {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'AdditionalOptions': [ '/TP' ],
              }
            },
            'defines': [
              'WIN32',
              '_CRT_NONSTDC_NO_DEPRECATE',
              '_DEBUG',
              '_WINDOWS',
              '_USRDLL',
              'JANSSON_DLL_EXPORTS',
              '_WINDLL',
              '_UNICODE',
              'UNICODE'
            ],
            'link_settings': {
              'libraries': [
                '-ladvapi32.lib',
                '-liphlpapi.lib',
                '-lpsapi.lib',
                '-lshell32.lib',
                '-lws2_32.lib'
              ],
            },
          }
        ],
        ['OS != "win" ',{
          'defines':[
              '_LARGEFILE_SOURCE',
              '_FILE_OFFSET_BITS=64',
              '_GNU_SOURCE',
          ]
        }
        ]
      ]
    }
  ],
}
