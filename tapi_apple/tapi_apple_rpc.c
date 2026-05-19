/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Apple TAPI
 *
 * Client wrappers of the apple_* RPCs.
 */

#define TE_LGR_USER     "TAPI Apple RPC"

#include "te_config.h"

#include <string.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_string.h"
#include "logger_api.h"
#include "tapi_rpc_internal.h"
#include "tarpc.h"

#include "tapi_apple_rpc.h"

/* The RPCs return te_errno; an RPC transport failure is TE_ECORRUPTED. */
#define CHECK_RPC_ERRNO_UNCHANGED(_func, _var) \
    CHECK_RETVAL_VAR_ERR_COND(_func, _var, false,                    \
                              TE_RC(TE_TAPI, TE_ECORRUPTED), false)

/* See description in tapi_apple_rpc.h */
te_errno
rpc_apple_screenshot(rcf_rpc_server *rpcs, const char *udid, bool simulator,
                     const char *path)
{
    tarpc_apple_screenshot_in in;
    tarpc_apple_screenshot_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.udid = (char *)udid;
    in.simulator = simulator;
    in.path = (char *)path;

    rcf_rpc_call(rpcs, "apple_screenshot", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(apple_screenshot, out.retval);
    TAPI_RPC_LOG(rpcs, apple_screenshot, "%s%s, %s", "%r", udid,
                 simulator ? " (simulator)" : "", path, out.retval);
    RETVAL_TE_ERRNO(apple_screenshot, out.retval);
}

/* See description in tapi_apple_rpc.h */
te_errno
rpc_apple_launch(rcf_rpc_server *rpcs, const char *udid, bool simulator,
                 const char *bundle, unsigned int *pid)
{
    tarpc_apple_launch_in in;
    tarpc_apple_launch_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.udid = (char *)udid;
    in.simulator = simulator;
    in.bundle = (char *)bundle;

    rcf_rpc_call(rpcs, "apple_launch", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(apple_launch, out.retval);
    TAPI_RPC_LOG(rpcs, apple_launch, "%s%s, %s", "%r pid=%u", udid,
                 simulator ? " (simulator)" : "", bundle, out.retval,
                 out.pid);

    if (out.retval == 0 && pid != NULL)
        *pid = out.pid;
    RETVAL_TE_ERRNO(apple_launch, out.retval);
}

/* See description in tapi_apple_rpc.h */
te_errno
rpc_apple_terminate(rcf_rpc_server *rpcs, const char *udid, bool simulator,
                    const char *bundle)
{
    tarpc_apple_terminate_in in;
    tarpc_apple_terminate_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.udid = (char *)udid;
    in.simulator = simulator;
    in.bundle = (char *)bundle;

    rcf_rpc_call(rpcs, "apple_terminate", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(apple_terminate, out.retval);
    TAPI_RPC_LOG(rpcs, apple_terminate, "%s%s, %s", "%r", udid,
                 simulator ? " (simulator)" : "", bundle, out.retval);
    RETVAL_TE_ERRNO(apple_terminate, out.retval);
}

/* See description in tapi_apple_rpc.h */
te_errno
rpc_apple_openurl(rcf_rpc_server *rpcs, const char *udid, const char *url)
{
    tarpc_apple_openurl_in in;
    tarpc_apple_openurl_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.udid = (char *)udid;
    in.url = (char *)url;

    rcf_rpc_call(rpcs, "apple_openurl", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(apple_openurl, out.retval);
    TAPI_RPC_LOG(rpcs, apple_openurl, "%s, %s", "%r", udid, url, out.retval);
    RETVAL_TE_ERRNO(apple_openurl, out.retval);
}

/* See description in tapi_apple_rpc.h */
te_errno
rpc_apple_push(rcf_rpc_server *rpcs, const char *udid, const char *local,
               const char *remote)
{
    tarpc_apple_push_in in;
    tarpc_apple_push_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.udid = (char *)udid;
    in.local = (char *)local;
    in.remote = (char *)remote;

    rcf_rpc_call(rpcs, "apple_push", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(apple_push, out.retval);
    TAPI_RPC_LOG(rpcs, apple_push, "%s, %s -> %s", "%r", udid, local, remote,
                 out.retval);
    RETVAL_TE_ERRNO(apple_push, out.retval);
}

/* See description in tapi_apple_rpc.h */
te_errno
rpc_apple_pull(rcf_rpc_server *rpcs, const char *udid, const char *remote,
               const char *local)
{
    tarpc_apple_pull_in in;
    tarpc_apple_pull_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.udid = (char *)udid;
    in.remote = (char *)remote;
    in.local = (char *)local;

    rcf_rpc_call(rpcs, "apple_pull", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(apple_pull, out.retval);
    TAPI_RPC_LOG(rpcs, apple_pull, "%s, %s -> %s", "%r", udid, remote, local,
                 out.retval);
    RETVAL_TE_ERRNO(apple_pull, out.retval);
}

/* See description in tapi_apple_rpc.h */
te_errno
rpc_apple_syslog_start(rcf_rpc_server *rpcs, const char *udid,
                       unsigned int *id)
{
    tarpc_apple_syslog_start_in in;
    tarpc_apple_syslog_start_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.udid = (char *)udid;

    rcf_rpc_call(rpcs, "apple_syslog_start", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(apple_syslog_start, out.retval);
    TAPI_RPC_LOG(rpcs, apple_syslog_start, "%s", "%r id=%u", udid,
                 out.retval, out.id);

    if (out.retval == 0)
        *id = out.id;
    RETVAL_TE_ERRNO(apple_syslog_start, out.retval);
}

/* See description in tapi_apple_rpc.h */
te_errno
rpc_apple_syslog_read(rcf_rpc_server *rpcs, unsigned int id, uint64_t offset,
                      te_string *data, uint64_t *next)
{
    tarpc_apple_syslog_read_in in;
    tarpc_apple_syslog_read_out out;
    bool silent = rpcs->silent;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.id = id;
    in.offset = offset;

    /* Polled in a loop: one log line per poll would swamp the log */
    rpcs->silent = true;
    rcf_rpc_call(rpcs, "apple_syslog_read", &in, &out);
    rpcs->silent = silent;
    CHECK_RPC_ERRNO_UNCHANGED(apple_syslog_read, out.retval);

    if (out.retval == 0)
    {
        if (out.data != NULL)
            te_string_append(data, "%s", out.data);
        *next = out.next;
    }
    else
    {
        ERROR("apple_syslog_read(%u, %llu) failed: %r", id,
              (unsigned long long)offset, out.retval);
    }
    RETVAL_TE_ERRNO(apple_syslog_read, out.retval);
}

/* See description in tapi_apple_rpc.h */
te_errno
rpc_apple_syslog_stop(rcf_rpc_server *rpcs, unsigned int id)
{
    tarpc_apple_syslog_stop_in in;
    tarpc_apple_syslog_stop_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.id = id;

    rcf_rpc_call(rpcs, "apple_syslog_stop", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(apple_syslog_stop, out.retval);
    TAPI_RPC_LOG(rpcs, apple_syslog_stop, "%u", "%r", id, out.retval);
    RETVAL_TE_ERRNO(apple_syslog_stop, out.retval);
}
