# SPDX-License-Identifier: MIT
# Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved.
#
# External TE library (see TE_EXT_REPO): engine-side TAPI for iOS
# devices and simulators on agents. Built as a shared library on the
# engine platform. It needs the apple_* RPCs in rpcxdr, so add
# ../ta_apple/apple_rpc.x.m4 to the rpcxdr definitions of the engine
# platform.

if get_option('engine')
    build_lib_shared = true
endif
build_lib_static = not get_option('engine')
install_lib = install_dev

headers += files(
    'tapi_apple.h',
    'tapi_apple_rpc.h',
)

sources += files(
    'tapi_apple.c',
    'tapi_apple_rpc.c',
)

te_libs += [
    'tapi',
    'tapi_rpc',
    'rcfrpc',
    'confapi',
    'rcfapi',
    'rpcxdr',
    'logger_core',
    'tools',
]

# Configurator model of the /agent/apple subtree
if get_option('engine')
    install_data(
        files('cm_apple.yml'),
        install_dir: join_paths(get_option('datadir'), 'cm'),
    )
endif
