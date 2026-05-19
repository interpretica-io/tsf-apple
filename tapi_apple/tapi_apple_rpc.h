/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Apple TAPI
 *
 * Client wrappers of the apple_* RPCs, see apple_rpc.x.m4. Tests use
 * tapi_apple.h; these are the calls behind it.
 */

#ifndef __TAPI_APPLE_RPC_H__
#define __TAPI_APPLE_RPC_H__

#include "te_errno.h"
#include "te_string.h"
#include "rcf_rpc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Write a screenshot to a file on the agent. */
extern te_errno rpc_apple_screenshot(rcf_rpc_server *rpcs, const char *udid,
                                     bool simulator, const char *path);

/** Launch an app. */
extern te_errno rpc_apple_launch(rcf_rpc_server *rpcs, const char *udid,
                                 bool simulator, const char *bundle,
                                 unsigned int *pid);

/** Terminate an app. */
extern te_errno rpc_apple_terminate(rcf_rpc_server *rpcs, const char *udid,
                                    bool simulator, const char *bundle);

/** Open a URL on a simulator. */
extern te_errno rpc_apple_openurl(rcf_rpc_server *rpcs, const char *udid,
                                  const char *url);

/** Copy a file from the agent to the media directory of a device. */
extern te_errno rpc_apple_push(rcf_rpc_server *rpcs, const char *udid,
                               const char *local, const char *remote);

/** Copy a file from the media directory of a device to the agent. */
extern te_errno rpc_apple_pull(rcf_rpc_server *rpcs, const char *udid,
                               const char *remote, const char *local);

/** Start a system log capture. */
extern te_errno rpc_apple_syslog_start(rcf_rpc_server *rpcs,
                                       const char *udid, unsigned int *id);

/** Read captured system log bytes from an offset. */
extern te_errno rpc_apple_syslog_read(rcf_rpc_server *rpcs, unsigned int id,
                                      uint64_t offset, te_string *data,
                                      uint64_t *next);

/** Stop a system log capture. */
extern te_errno rpc_apple_syslog_stop(rcf_rpc_server *rpcs, unsigned int id);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TAPI_APPLE_RPC_H__ */
