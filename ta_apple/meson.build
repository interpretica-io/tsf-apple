# SPDX-License-Identifier: MIT
# Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved.
#
# External TE library (see TE_EXT_REPO): agent-side access to iOS
# devices through libimobiledevice and to simulators through simctl.
# The agent and its RPC server both link it; the /agent/apple subtree
# is ta_apple_conf, for the agent alone.

headers += files(
    'ta_apple.h',
)

sources += files(
    'ta_apple_device.c',
    'ta_apple_sim.c',
)

te_libs += [
    'agentlib',
    'logger_core',
    'tools',
]

deps += [
    dependency('libimobiledevice-1.0'),
    dependency('libplist-2.0'),
    dependency('jansson'),
    dependency('threads'),
]
