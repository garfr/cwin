project('cwin', 'c')

cwin_lib = static_library('cwin',
                          'cwin.c',
                          c_args : ['-DCWIN_BACKEND_WIN32',
                                    '-DUNICODE'],
)

test = executable('cwin_example',
                   'example.c',
                   link_with : cwin_lib
)
