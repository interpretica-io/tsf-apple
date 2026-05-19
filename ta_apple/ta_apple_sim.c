/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Apple agent library
 *
 * Simulators through 'xcrun simctl'. There is no library interface
 * to CoreSimulator, so the tool runs on the agent and its JSON
 * output is parsed here.
 */

#define TE_LGR_USER     "TA Apple Sim"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <jansson.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "logger_api.h"
#include "agentlib.h"

#include "ta_apple.h"

/** Where Xcode installs the command line tool front end. */
#define XCRUN   "/usr/bin/xcrun"

/*
 * Run a simctl command and collect its standard output. The exit
 * status of the tool is not available through ta_pclose_r(), so the
 * tool itself is checked for first and an empty answer is treated
 * as a failure by the callers.
 */
static te_errno
simctl(te_string *out, const char *fmt, ...)
{
    te_errno rc;
    te_string cmd = TE_STRING_INIT;
    va_list ap;
    FILE *f = NULL;
    pid_t pid;
    char buf[4096];
    size_t n;

    if (access(XCRUN, X_OK) != 0)
    {
        VERB("No " XCRUN " on this agent: no simulators");
        return TE_RC(TE_TA_UNIX, TE_ENOENT);
    }

    te_string_append(&cmd, XCRUN " simctl ");
    va_start(ap, fmt);
    te_string_append_va(&cmd, fmt, ap);
    va_end(ap);

    rc = ta_popen_r(cmd.ptr, &pid, &f);
    if (rc != 0)
    {
        ERROR("Cannot run '%s': %r", cmd.ptr, rc);
        te_string_free(&cmd);
        return rc;
    }

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
    {
        if (out != NULL)
            te_string_append(out, "%.*s", (int)n, buf);
    }

    rc = ta_pclose_r(pid, f);
    if (rc != 0)
        ERROR("'%s' failed: %r", cmd.ptr, rc);

    te_string_free(&cmd);
    return rc;
}

/* The available simulators as simctl describes them in JSON. */
static te_errno
sim_json(json_t **root)
{
    te_errno rc;
    te_string out = TE_STRING_INIT;
    json_error_t err;

    rc = simctl(&out, "list devices available -j");
    if (rc != 0)
    {
        te_string_free(&out);
        return rc;
    }

    *root = json_loads(out.ptr != NULL ? out.ptr : "", 0, &err);
    te_string_free(&out);
    if (*root == NULL)
    {
        ERROR("Cannot parse the simctl device list: %s", err.text);
        return TE_RC(TE_TA_UNIX, TE_EPROTO);
    }
    return 0;
}

/*
 * Walk the simulators: the JSON maps a runtime to an array of
 * devices. The callback gets each device object; a non-zero result
 * stops the walk.
 */
typedef int (*sim_walk_cb)(json_t *sim, void *arg);

static te_errno
sim_walk(sim_walk_cb cb, void *arg)
{
    te_errno rc;
    json_t *root = NULL;
    json_t *devices;
    const char *runtime;
    json_t *list;

    rc = sim_json(&root);
    if (rc != 0)
        return rc;

    devices = json_object_get(root, "devices");
    json_object_foreach(devices, runtime, list)
    {
        size_t i;
        json_t *sim;

        json_array_foreach(list, i, sim)
        {
            if (cb(sim, arg) != 0)
                goto out;
        }
    }

out:
    json_decref(root);
    return 0;
}

static int
sim_list_cb(json_t *sim, void *arg)
{
    te_string *out = arg;

    te_string_append(out, "%s\t%s\t%s\n",
                     json_string_value(json_object_get(sim, "udid")),
                     json_string_value(json_object_get(sim, "state")),
                     json_string_value(json_object_get(sim, "name")));
    return 0;
}

/* See description in ta_apple.h */
te_errno
ta_apple_sim_list(te_string *out)
{
    return sim_walk(sim_list_cb, out);
}

typedef struct sim_get_arg {
    const char *udid;
    const char *field;
    te_string *value;
    bool found;
} sim_get_arg;

static int
sim_get_cb(json_t *sim, void *arg)
{
    sim_get_arg *a = arg;
    const char *udid = json_string_value(json_object_get(sim, "udid"));

    if (udid == NULL || strcmp(udid, a->udid) != 0)
        return 0;

    te_string_append(a->value, "%s",
                     json_string_value(json_object_get(sim, a->field)));
    a->found = true;
    return 1;
}

/* See description in ta_apple.h */
te_errno
ta_apple_sim_get(const char *udid, const char *field, te_string *value)
{
    te_errno rc;
    sim_get_arg arg = {.udid = udid, .field = field, .value = value,
                       .found = false};

    rc = sim_walk(sim_get_cb, &arg);
    if (rc == 0 && !arg.found)
        rc = TE_RC(TE_TA_UNIX, TE_ENOENT);
    return rc;
}

/* See description in ta_apple.h */
te_errno
ta_apple_sim_set_booted(const char *udid, bool boot)
{
    return simctl(NULL, "%s %s", boot ? "boot" : "shutdown", udid);
}

/* See description in ta_apple.h */
te_errno
ta_apple_sim_apps(const char *udid, te_string *out)
{
    te_errno rc;
    te_string text = TE_STRING_INIT;
    json_t *root;
    json_error_t err;
    const char *bundle;
    json_t *app;

    /* listapps prints an old-style plist; plutil turns it into JSON */
    rc = simctl(&text, "listapps %s | plutil -convert json -o - -", udid);
    if (rc != 0)
        goto out;

    root = json_loads(text.ptr != NULL ? text.ptr : "", 0, &err);
    if (root == NULL)
    {
        ERROR("Cannot parse the app list of simulator %s: %s", udid,
              err.text);
        rc = TE_RC(TE_TA_UNIX, TE_EPROTO);
        goto out;
    }

    json_object_foreach(root, bundle, app)
    {
        const char *type = json_string_value(json_object_get(app,
                                                             "ApplicationType"));

        if (type == NULL || strcmp(type, "User") == 0)
            te_string_append(out, "%s\n", bundle);
    }
    json_decref(root);

out:
    te_string_free(&text);
    return rc;
}

/* See description in ta_apple.h */
te_errno
ta_apple_sim_install(const char *udid, const char *path)
{
    return simctl(NULL, "install %s '%s'", udid, path);
}

/* See description in ta_apple.h */
te_errno
ta_apple_sim_uninstall(const char *udid, const char *bundle)
{
    return simctl(NULL, "uninstall %s %s", udid, bundle);
}

/* See description in ta_apple.h */
te_errno
ta_apple_sim_launch(const char *udid, const char *bundle, unsigned int *pid)
{
    te_errno rc;
    te_string out = TE_STRING_INIT;
    const char *colon;

    rc = simctl(&out, "launch %s %s", udid, bundle);
    if (rc != 0)
        goto out;

    /* "com.example.app: 12345" */
    colon = out.ptr != NULL ? strrchr(out.ptr, ':') : NULL;
    if (colon == NULL)
    {
        ERROR("simctl launch did not report a pid: %s",
              out.ptr != NULL ? out.ptr : "");
        rc = TE_RC(TE_TA_UNIX, TE_EPROTO);
        goto out;
    }
    *pid = strtoul(colon + 1, NULL, 10);

out:
    te_string_free(&out);
    return rc;
}

/* See description in ta_apple.h */
te_errno
ta_apple_sim_terminate(const char *udid, const char *bundle)
{
    return simctl(NULL, "terminate %s %s", udid, bundle);
}

/* See description in ta_apple.h */
te_errno
ta_apple_sim_screenshot(const char *udid, const char *path)
{
    return simctl(NULL, "io %s screenshot '%s'", udid, path);
}

/* See description in ta_apple.h */
te_errno
ta_apple_sim_openurl(const char *udid, const char *url)
{
    return simctl(NULL, "openurl %s '%s'", udid, url);
}
