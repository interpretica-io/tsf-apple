/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Apple RPC server library
 *
 * The apple_* RPCs (see apple_rpc.x.m4) on top of ta_apple.
 * TARPC_FUNC_STATIC() binds an RPC to the function of the same name,
 * so each RPC has a plain C function first and the wrapper after it.
 */

#define TE_LGR_USER     "RPC Apple"

#include "te_config.h"

#include <string.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "rpc_server.h"

#include "ta_apple.h"

static te_errno
apple_screenshot(const char *udid, bool simulator, const char *path)
{
    return simulator ? ta_apple_sim_screenshot(udid, path) :
                       ta_apple_screenshot(udid, path);
}

TARPC_FUNC_STATIC(apple_screenshot, {},
{
    MAKE_CALL(out->retval = func(in->udid, in->simulator, in->path));
    out->common.errno_changed = false;
})

static te_errno
apple_launch(const char *udid, bool simulator, const char *bundle,
             unsigned int *pid)
{
    return simulator ? ta_apple_sim_launch(udid, bundle, pid) :
                       ta_apple_launch(udid, bundle, pid);
}

TARPC_FUNC_STATIC(apple_launch, {},
{
    MAKE_CALL(out->retval = func(in->udid, in->simulator, in->bundle,
                                 &out->pid));
    out->common.errno_changed = false;
})

static te_errno
apple_terminate(const char *udid, bool simulator, const char *bundle)
{
    if (!simulator)
    {
        ERROR("Terminating %s on the device %s is not supported", bundle,
              udid);
        return TE_RC(TE_TA_UNIX, TE_EOPNOTSUPP);
    }
    return ta_apple_sim_terminate(udid, bundle);
}

TARPC_FUNC_STATIC(apple_terminate, {},
{
    MAKE_CALL(out->retval = func(in->udid, in->simulator, in->bundle));
    out->common.errno_changed = false;
})

static te_errno
apple_openurl(const char *udid, const char *url)
{
    return ta_apple_sim_openurl(udid, url);
}

TARPC_FUNC_STATIC(apple_openurl, {},
{
    MAKE_CALL(out->retval = func(in->udid, in->url));
    out->common.errno_changed = false;
})

static te_errno
apple_push(const char *udid, const char *local, const char *remote)
{
    return ta_apple_push(udid, local, remote);
}

TARPC_FUNC_STATIC(apple_push, {},
{
    MAKE_CALL(out->retval = func(in->udid, in->local, in->remote));
    out->common.errno_changed = false;
})

static te_errno
apple_pull(const char *udid, const char *remote, const char *local)
{
    return ta_apple_pull(udid, remote, local);
}

TARPC_FUNC_STATIC(apple_pull, {},
{
    MAKE_CALL(out->retval = func(in->udid, in->remote, in->local));
    out->common.errno_changed = false;
})

static te_errno
apple_syslog_start(const char *udid, unsigned int *id)
{
    return ta_apple_syslog_start(udid, id);
}

TARPC_FUNC_STATIC(apple_syslog_start, {},
{
    MAKE_CALL(out->retval = func(in->udid, &out->id));
    out->common.errno_changed = false;
})

static te_errno
apple_syslog_read(unsigned int id, uint64_t offset, char **data,
                  uint64_t *next)
{
    te_errno rc;
    te_string out = TE_STRING_INIT;

    rc = ta_apple_syslog_read(id, offset, &out, next);
    *data = out.ptr != NULL ? out.ptr : TE_STRDUP("");
    return rc;
}

TARPC_FUNC_STATIC(apple_syslog_read, {},
{
    MAKE_CALL(out->retval = func(in->id, in->offset, &out->data, &out->next));
    out->common.errno_changed = false;
})

static te_errno
apple_syslog_stop(unsigned int id)
{
    return ta_apple_syslog_stop(id);
}

TARPC_FUNC_STATIC(apple_syslog_stop, {},
{
    MAKE_CALL(out->retval = func(in->id));
    out->common.errno_changed = false;
})
