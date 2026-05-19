/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Apple TAPI
 *
 * State and apps go through the /agent/apple subtree of the
 * Configurator; screenshots, launches, files and the system log go
 * through the apple_* RPCs.
 */

#define TE_LGR_USER     "TAPI Apple"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "te_sleep.h"
#include "logger_api.h"
#include "conf_api.h"
#include "rcf_api.h"
#include "tapi_file.h"
#include "tapi_test_log.h"

#include "tapi_apple.h"
#include "tapi_apple_rpc.h"

/** Where the agent keeps files on their way to the engine. */
#define AGENT_TMP   "/tmp"

/** Pause between two reads of a system log capture. */
#define SYSLOG_POLL_MS  500

struct tapi_apple {
    rcf_rpc_server *rpcs;           /**< RPC server on the agent */
    const char *ta;                 /**< Agent name */
    char *udid;                     /**< Device or simulator UDID */
    bool simulator;                 /**< It is a simulator */
    tapi_apple_syslog *syslog;      /**< Running capture, if any */
};

struct tapi_apple_syslog {
    tapi_apple *dev;                /**< Device of the capture */
    unsigned int id;                /**< Capture id on the agent */
    uint64_t offset;                /**< Bytes read so far */
    te_string pending;              /**< An unfinished last line */
};

/* The collection a handle lives in. */
static const char *
kind(const tapi_apple *dev)
{
    return dev->simulator ? "simulator" : "device";
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_list(const char *ta, bool simulators, te_string *list)
{
    te_errno rc;
    cfg_handle *handles = NULL;
    unsigned int n = 0;
    unsigned int i;

    rc = cfg_synchronize_fmt(true, "/agent:%s/apple:", ta);
    if (rc != 0)
    {
        ERROR("Cannot synchronize /agent:%s/apple: %r", ta, rc);
        return rc;
    }

    rc = cfg_find_pattern_fmt(&n, &handles, "/agent:%s/apple:/%s:*", ta,
                              simulators ? "simulator" : "device");
    if (rc != 0)
        return rc;

    for (i = 0; i < n; i++)
    {
        char *name = NULL;

        rc = cfg_get_inst_name(handles[i], &name);
        if (rc != 0)
            break;
        te_string_append(list, "%s\n", name);
        free(name);
    }

    free(handles);
    return rc;
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_create(rcf_rpc_server *rpcs, const char *udid, bool simulator,
                  tapi_apple **dev)
{
    te_errno rc;
    te_string list = TE_STRING_INIT;
    tapi_apple *d;
    char *nl;

    rc = tapi_apple_list(rpcs->ta, simulator, &list);
    if (rc != 0)
        return rc;

    if (udid == NULL)
    {
        nl = list.ptr != NULL ? strchr(list.ptr, '\n') : NULL;
        if (nl == NULL || nl[1] != '\0')
        {
            ERROR("%s lists %s %s, name the UDID", rpcs->ta,
                  nl == NULL ? "no" : "more than one",
                  simulator ? "simulator" : "device");
            te_string_free(&list);
            return TE_RC(TE_TAPI, TE_ENOENT);
        }
        *nl = '\0';
        udid = list.ptr;
    }
    else
    {
        te_string needle = TE_STRING_INIT;

        te_string_append(&needle, "%s\n", udid);
        if (list.ptr == NULL || strstr(list.ptr, needle.ptr) == NULL)
        {
            ERROR("%s does not list the %s %s", rpcs->ta,
                  simulator ? "simulator" : "device", udid);
            te_string_free(&needle);
            te_string_free(&list);
            return TE_RC(TE_TAPI, TE_ENOENT);
        }
        te_string_free(&needle);
    }

    d = TE_ALLOC(sizeof(*d));
    d->rpcs = rpcs;
    d->ta = rpcs->ta;
    d->udid = TE_STRDUP(udid);
    d->simulator = simulator;

    te_string_free(&list);
    *dev = d;
    return 0;
}

/* See description in tapi_apple.h */
void
tapi_apple_destroy(tapi_apple *dev)
{
    if (dev == NULL)
        return;

    if (dev->syslog != NULL)
        tapi_apple_syslog_stop(dev->syslog);
    free(dev->udid);
    free(dev);
}

/* See description in tapi_apple.h */
const char *
tapi_apple_udid(const tapi_apple *dev)
{
    return dev->udid;
}

/* Get a string leaf of the handle. */
static te_errno
get_leaf(tapi_apple *dev, const char *leaf, te_string *value)
{
    te_errno rc;
    char *val = NULL;

    rc = cfg_get_string(&val, "/agent:%s/apple:/%s:%s/%s:", dev->ta,
                        kind(dev), dev->udid, leaf);
    if (rc == 0)
        te_string_append(value, "%s", val);
    free(val);
    return rc;
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_name(tapi_apple *dev, te_string *name)
{
    return get_leaf(dev, "name", name);
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_info(tapi_apple *dev, te_string *product, te_string *version)
{
    te_errno rc = 0;

    if (dev->simulator)
    {
        ERROR("A simulator has no product type and version leaves");
        return TE_RC(TE_TAPI, TE_EOPNOTSUPP);
    }
    if (product != NULL)
        rc = get_leaf(dev, "product", product);
    if (rc == 0 && version != NULL)
        rc = get_leaf(dev, "version", version);
    return rc;
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_is_installed(tapi_apple *dev, const char *bundle, bool *installed)
{
    te_errno rc;
    cfg_handle handle;

    rc = cfg_synchronize_fmt(true, "/agent:%s/apple:/%s:%s/app:", dev->ta,
                             kind(dev), dev->udid);
    if (rc != 0)
        return rc;

    rc = cfg_find_fmt(&handle, "/agent:%s/apple:/%s:%s/app:%s", dev->ta,
                      kind(dev), dev->udid, bundle);
    if (rc == 0)
        *installed = true;
    else if (TE_RC_GET_ERROR(rc) == TE_ENOENT)
    {
        *installed = false;
        rc = 0;
    }
    return rc;
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_install(tapi_apple *dev, const char *bundle, const char *path)
{
    te_errno rc;

    rc = cfg_add_instance_fmt(NULL, CVT_STRING, path,
                              "/agent:%s/apple:/%s:%s/app:%s", dev->ta,
                              kind(dev), dev->udid, bundle);
    if (rc == 0)
        RING("Installed %s from %s on %s", bundle, path, dev->udid);
    return rc;
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_install_from_engine(tapi_apple *dev, const char *bundle,
                               const char *ipa)
{
    te_errno rc;
    te_string remote = TE_STRING_INIT;
    te_string suffix = TE_STRING_INIT;
    const char *base = strrchr(ipa, '/');

    te_string_append(&suffix, "_%s", base != NULL ? base + 1 : ipa);
    tapi_file_make_custom_pathname(&remote, AGENT_TMP, suffix.ptr);

    rc = rcf_ta_put_file(dev->ta, 0, ipa, remote.ptr);
    if (rc != 0)
    {
        ERROR("Failed to copy %s to %s:%s: %r", ipa, dev->ta, remote.ptr, rc);
        goto out;
    }

    rc = tapi_apple_install(dev, bundle, remote.ptr);
    tapi_file_ta_unlink_fmt(dev->ta, "%s", remote.ptr);

out:
    te_string_free(&remote);
    te_string_free(&suffix);
    return rc;
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_uninstall(tapi_apple *dev, const char *bundle)
{
    return cfg_del_instance_fmt(false, "/agent:%s/apple:/%s:%s/app:%s",
                                dev->ta, kind(dev), dev->udid, bundle);
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_launch(tapi_apple *dev, const char *bundle, unsigned int *pid)
{
    return rpc_apple_launch(dev->rpcs, dev->udid, dev->simulator, bundle,
                            pid);
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_terminate(tapi_apple *dev, const char *bundle)
{
    return rpc_apple_terminate(dev->rpcs, dev->udid, dev->simulator, bundle);
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_screenshot(tapi_apple *dev, const char *name, te_string *path)
{
    te_errno rc;
    te_string agent_path = TE_STRING_INIT;
    te_string local = TE_STRING_INIT;
    te_string suffix = TE_STRING_INIT;
    const char *dir = getenv("TE_LOG_DIR");

    if (dir == NULL)
        dir = getenv("TE_TMP");
    if (dir == NULL)
    {
        ERROR("Neither TE_LOG_DIR nor TE_TMP is set, nowhere to put "
              "the screenshot");
        return TE_RC(TE_TAPI, TE_ENOENT);
    }

    te_string_append(&suffix, "_%s.png", name);
    tapi_file_make_custom_pathname(&agent_path, AGENT_TMP, suffix.ptr);
    tapi_file_make_custom_pathname(&local, dir, suffix.ptr);

    rc = rpc_apple_screenshot(dev->rpcs, dev->udid, dev->simulator,
                              agent_path.ptr);
    if (rc != 0)
        goto out;

    rc = rcf_ta_get_file(dev->ta, 0, agent_path.ptr, local.ptr);
    if (rc != 0)
    {
        ERROR("Failed to copy the screenshot %s:%s to %s: %r", dev->ta,
              agent_path.ptr, local.ptr, rc);
        goto out;
    }

    RING_ARTIFACT("Screenshot '%s': %s", name, local.ptr);
    if (path != NULL)
        te_string_append(path, "%s", local.ptr);

out:
    tapi_file_ta_unlink_fmt(dev->ta, "%s", agent_path.ptr);
    te_string_free(&agent_path);
    te_string_free(&local);
    te_string_free(&suffix);
    return rc;
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_push(tapi_apple *dev, const char *src, const char *dst)
{
    return rpc_apple_push(dev->rpcs, dev->udid, src, dst);
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_pull(tapi_apple *dev, const char *src, const char *dst)
{
    return rpc_apple_pull(dev->rpcs, dev->udid, src, dst);
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_sim_state(tapi_apple *dev, te_string *state)
{
    te_errno rc;

    rc = cfg_synchronize_fmt(true, "/agent:%s/apple:/simulator:%s", dev->ta,
                             dev->udid);
    if (rc != 0)
        return rc;
    return get_leaf(dev, "state", state);
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_sim_boot(tapi_apple *dev, unsigned int timeout_ms)
{
    te_errno rc;
    te_string state = TE_STRING_INIT;
    unsigned int waited = 0;

    rc = cfg_set_instance_fmt(CVT_STRING, "Booted",
                              "/agent:%s/apple:/simulator:%s/state:",
                              dev->ta, dev->udid);
    if (rc != 0)
        return rc;

    for (;;)
    {
        te_string_reset(&state);
        rc = tapi_apple_sim_state(dev, &state);
        if (rc == 0 && strcmp(state.ptr, "Booted") == 0)
            break;
        if (waited >= timeout_ms)
        {
            ERROR("Simulator %s is '%s' after %u ms", dev->udid,
                  rc == 0 ? state.ptr : "unknown", timeout_ms);
            rc = TE_RC(TE_TAPI, TE_ETIMEDOUT);
            break;
        }
        te_msleep(1000);
        waited += 1000;
    }

    te_string_free(&state);
    return rc;
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_sim_shutdown(tapi_apple *dev)
{
    return cfg_set_instance_fmt(CVT_STRING, "Shutdown",
                                "/agent:%s/apple:/simulator:%s/state:",
                                dev->ta, dev->udid);
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_sim_openurl(tapi_apple *dev, const char *url)
{
    return rpc_apple_openurl(dev->rpcs, dev->udid, url);
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_syslog_start(tapi_apple *dev, tapi_apple_syslog **sl)
{
    te_errno rc;
    tapi_apple_syslog *s;

    if (dev->simulator)
    {
        ERROR("The system log capture works for devices only");
        return TE_RC(TE_TAPI, TE_EOPNOTSUPP);
    }
    if (dev->syslog != NULL)
    {
        ERROR("A system log capture of %s is running already", dev->udid);
        return TE_RC(TE_TAPI, TE_EALREADY);
    }

    s = TE_ALLOC(sizeof(*s));
    s->dev = dev;
    s->pending = (te_string)TE_STRING_INIT;

    rc = rpc_apple_syslog_start(dev->rpcs, dev->udid, &s->id);
    if (rc != 0)
    {
        free(s);
        return rc;
    }

    dev->syslog = s;
    *sl = s;
    return 0;
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_syslog_expect(tapi_apple_syslog *sl, const char *re,
                         unsigned int timeout_ms, te_string *line)
{
    te_errno rc = 0;
    regex_t regex;
    te_string chunk = TE_STRING_INIT;
    unsigned int waited = 0;
    bool found = false;
    int err;

    err = regcomp(&regex, re, REG_EXTENDED | REG_NOSUB);
    if (err != 0)
    {
        char msg[128];

        regerror(err, &regex, msg, sizeof(msg));
        ERROR("Bad regular expression '%s': %s", re, msg);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    while (!found)
    {
        char *start;
        char *nl;

        te_string_reset(&chunk);
        rc = rpc_apple_syslog_read(sl->dev->rpcs, sl->id, sl->offset, &chunk,
                                   &sl->offset);
        if (rc != 0)
            break;
        if (chunk.ptr != NULL)
            te_string_append(&sl->pending, "%s", chunk.ptr);

        /* Match whole lines only; keep an unfinished tail for later */
        start = sl->pending.ptr;
        while (start != NULL && (nl = strchr(start, '\n')) != NULL)
        {
            *nl = '\0';
            if (regexec(&regex, start, 0, NULL, 0) == 0)
            {
                RING("syslog: %s", start);
                if (line != NULL)
                    te_string_append(line, "%s", start);
                found = true;
                start = nl + 1;
                break;
            }
            start = nl + 1;
        }
        if (start != NULL && start != sl->pending.ptr)
        {
            te_string rest = TE_STRING_INIT;

            te_string_append(&rest, "%s", start);
            te_string_free(&sl->pending);
            sl->pending = rest;
        }

        if (!found)
        {
            if (waited >= timeout_ms)
            {
                ERROR("No system log line matching '%s' within %u ms", re,
                      timeout_ms);
                rc = TE_RC(TE_TAPI, TE_ETIMEDOUT);
                break;
            }
            te_msleep(SYSLOG_POLL_MS);
            waited += SYSLOG_POLL_MS;
        }
    }

    regfree(&regex);
    te_string_free(&chunk);
    return rc;
}

/* See description in tapi_apple.h */
te_errno
tapi_apple_syslog_stop(tapi_apple_syslog *sl)
{
    te_errno rc;

    if (sl == NULL)
        return 0;

    rc = rpc_apple_syslog_stop(sl->dev->rpcs, sl->id);
    sl->dev->syslog = NULL;
    te_string_free(&sl->pending);
    free(sl);
    return rc;
}
