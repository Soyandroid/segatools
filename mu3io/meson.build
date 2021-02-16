mu3io_dll = shared_library(
    'mu3io',
    name_prefix : '',
    include_directories : inc,
    implicit_include_directories : false,
    vs_module_defs : 'mu3io.def',
    c_pch : '../precompiled.h',
    dependencies : [
        xinput_lib,
    ],
    link_with : [
        util_lib
    ],
    sources : [
        'config.c',
        'config.h',
        'leddata.h',
        'ledoutput.c',
        'ledoutput.h',
        'mu3io.c',
        'mu3io.h',
        'pipeimpl.c',
        'pipeimpl.h',
        'serialimpl.c',
        'serialimpl.h'
    ],
)
