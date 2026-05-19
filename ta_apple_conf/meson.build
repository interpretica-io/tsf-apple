# SPDX-License-Identifier: MIT
# Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved.
#
# External TE library (see TE_EXT_REPO): the /agent/apple configuration
# subtree on top of ta_apple. Only the agent links it, wholly, so that
# the constructor in ta_apple_conf.c registers the subtree without
# Test Agent modifications; the RPC server links ta_apple alone.

link_whole = true

sources += files(
    'ta_apple_conf.c',
)

te_libs += [
    'rcfpch',
    'ta_apple',
    'logger_core',
    'tools',
]
