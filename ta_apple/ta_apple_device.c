/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Apple agent library
 *
 * Physical devices through libimobiledevice.
 */

#define TE_LGR_USER     "TA Apple Device"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>

#include <plist/plist.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/installation_proxy.h>
#include <libimobiledevice/afc.h>
#include <libimobiledevice/screenshotr.h>
#include <libimobiledevice/syslog_relay.h>
#include <libimobiledevice/debugserver.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "te_sleep.h"
#include "logger_api.h"

#include "ta_apple.h"

/** Label the services see. */
#define LABEL   "ta_apple"

/** Where an app package is uploaded before installation. */
#define STAGING "PublicStaging"

/** Time to wait for an installation to complete. */
#define INSTALL_TIMEOUT_MS  300000

/** Size of one AFC transfer. */
#define AFC_CHUNK   (64 * 1024)

/** Number of system log captures that may run at once. */
#define SYSLOG_MAX  8

/** Where the captures go on the agent. */
#define SYSLOG_DIR  "/tmp"

/* Open a device and a lockdownd session on it. */
static te_errno
device_open(const char *udid, idevice_t *dev, lockdownd_client_t *ld)
{
    idevice_error_t ierr;
    lockdownd_error_t lerr;

    ierr = idevice_new_with_options(dev, udid,
                                    IDEVICE_LOOKUP_USBMUX |
                                    IDEVICE_LOOKUP_NETWORK);
    if (ierr != IDEVICE_E_SUCCESS)
    {
        ERROR("No device %s: idevice error %d", udid, ierr);
        return TE_RC(TE_TA_UNIX, TE_ENOENT);
    }

    if (ld == NULL)
        return 0;

    lerr = lockdownd_client_new_with_handshake(*dev, ld, LABEL);
    if (lerr != LOCKDOWN_E_SUCCESS)
    {
        ERROR("lockdownd handshake with %s failed: error %d (is the "
              "device paired and unlocked?)", udid, lerr);
        idevice_free(*dev);
        *dev = NULL;
        return TE_RC(TE_TA_UNIX, TE_EPERM);
    }
    return 0;
}

/* See description in ta_apple.h */
te_errno
ta_apple_devices(te_string *out)
{
    char **list = NULL;
    int n = 0;
    int i;
    idevice_error_t err;

    err = idevice_get_device_list(&list, &n);
    if (err == IDEVICE_E_NO_DEVICE)
        return 0;
    if (err != IDEVICE_E_SUCCESS)
    {
        ERROR("Cannot list devices: idevice error %d (is usbmuxd running?)",
              err);
        return TE_RC(TE_TA_UNIX, TE_EIO);
    }

    for (i = 0; i < n; i++)
        te_string_append(out, "%s\n", list[i]);
    idevice_device_list_free(list);
    return 0;
}

/* See description in ta_apple.h */
te_errno
ta_apple_info(const char *udid, const char *key, te_string *value)
{
    te_errno rc;
    idevice_t dev = NULL;
    lockdownd_client_t ld = NULL;
    plist_t node = NULL;
    lockdownd_error_t lerr;

    rc = device_open(udid, &dev, &ld);
    if (rc != 0)
        return rc;

    lerr = lockdownd_get_value(ld, NULL, key, &node);
    if (lerr != LOCKDOWN_E_SUCCESS || node == NULL)
    {
        ERROR("lockdownd has no '%s' for %s: error %d", key, udid, lerr);
        rc = TE_RC(TE_TA_UNIX, TE_ENOENT);
    }
    else if (plist_get_node_type(node) == PLIST_STRING)
    {
        char *s = NULL;

        plist_get_string_val(node, &s);
        te_string_append(value, "%s", s != NULL ? s : "");
        free(s);
    }
    else
    {
        char *xml = NULL;
        uint32_t len = 0;

        plist_to_xml(node, &xml, &len);
        te_string_append(value, "%.*s", (int)len, xml != NULL ? xml : "");
        free(xml);
    }

    if (node != NULL)
        plist_free(node);
    lockdownd_client_free(ld);
    idevice_free(dev);
    return rc;
}

/* Start the installation proxy on a device. */
static te_errno
instproxy_open(const char *udid, idevice_t *dev, instproxy_client_t *inst)
{
    te_errno rc;
    instproxy_error_t err;

    rc = device_open(udid, dev, NULL);
    if (rc != 0)
        return rc;

    err = instproxy_client_start_service(*dev, inst, LABEL);
    if (err != INSTPROXY_E_SUCCESS)
    {
        ERROR("Cannot start the installation proxy on %s: error %d", udid,
              err);
        idevice_free(*dev);
        *dev = NULL;
        return TE_RC(TE_TA_UNIX, TE_EIO);
    }
    return 0;
}

/* See description in ta_apple.h */
te_errno
ta_apple_apps(const char *udid, te_string *out)
{
    te_errno rc;
    idevice_t dev = NULL;
    instproxy_client_t inst = NULL;
    plist_t opts;
    plist_t apps = NULL;
    instproxy_error_t err;
    uint32_t i;

    rc = instproxy_open(udid, &dev, &inst);
    if (rc != 0)
        return rc;

    opts = instproxy_client_options_new();
    instproxy_client_options_add(opts, "ApplicationType", "User", NULL);
    instproxy_client_options_set_return_attributes(opts, "CFBundleIdentifier",
                                                   NULL);
    err = instproxy_browse(inst, opts, &apps);
    instproxy_client_options_free(opts);
    if (err != INSTPROXY_E_SUCCESS || apps == NULL)
    {
        ERROR("Cannot list the apps of %s: error %d", udid, err);
        rc = TE_RC(TE_TA_UNIX, TE_EIO);
        goto out;
    }

    for (i = 0; i < plist_array_get_size(apps); i++)
    {
        plist_t id = plist_dict_get_item(plist_array_get_item(apps, i),
                                         "CFBundleIdentifier");
        char *s = NULL;

        if (id == NULL)
            continue;
        plist_get_string_val(id, &s);
        if (s != NULL)
            te_string_append(out, "%s\n", s);
        free(s);
    }

out:
    if (apps != NULL)
        plist_free(apps);
    instproxy_client_free(inst);
    idevice_free(dev);
    return rc;
}

/* Upload one file to the device through AFC. */
static te_errno
afc_upload_file(afc_client_t afc, const char *local, const char *remote)
{
    te_errno rc = 0;
    int fd;
    uint64_t handle = 0;
    char *buf;
    afc_error_t err;

    fd = open(local, O_RDONLY);
    if (fd < 0)
    {
        ERROR("Cannot open %s: %s", local, strerror(errno));
        return TE_OS_RC(TE_TA_UNIX, errno);
    }

    err = afc_file_open(afc, remote, AFC_FOPEN_WRONLY, &handle);
    if (err != AFC_E_SUCCESS)
    {
        ERROR("Cannot create %s on the device: AFC error %d", remote, err);
        close(fd);
        return TE_RC(TE_TA_UNIX, TE_EIO);
    }

    buf = TE_ALLOC(AFC_CHUNK);
    for (;;)
    {
        ssize_t n = read(fd, buf, AFC_CHUNK);
        uint32_t written = 0;

        if (n < 0)
        {
            rc = TE_OS_RC(TE_TA_UNIX, errno);
            break;
        }
        if (n == 0)
            break;
        err = afc_file_write(afc, handle, buf, n, &written);
        if (err != AFC_E_SUCCESS || written != n)
        {
            ERROR("Writing %s to the device failed: AFC error %d", remote,
                  err);
            rc = TE_RC(TE_TA_UNIX, TE_EIO);
            break;
        }
    }

    free(buf);
    afc_file_close(afc, handle);
    close(fd);
    return rc;
}

/* Upload a directory tree to the device through AFC. */
static te_errno
afc_upload_tree(afc_client_t afc, const char *local, const char *remote)
{
    te_errno rc = 0;
    DIR *dir;
    struct dirent *ent;
    afc_error_t err;

    err = afc_make_directory(afc, remote);
    if (err != AFC_E_SUCCESS)
    {
        ERROR("Cannot create %s on the device: AFC error %d", remote, err);
        return TE_RC(TE_TA_UNIX, TE_EIO);
    }

    dir = opendir(local);
    if (dir == NULL)
    {
        ERROR("Cannot read %s: %s", local, strerror(errno));
        return TE_OS_RC(TE_TA_UNIX, errno);
    }

    while (rc == 0 && (ent = readdir(dir)) != NULL)
    {
        te_string l = TE_STRING_INIT;
        te_string r = TE_STRING_INIT;
        struct stat st;

        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        te_string_append(&l, "%s/%s", local, ent->d_name);
        te_string_append(&r, "%s/%s", remote, ent->d_name);
        if (stat(l.ptr, &st) != 0)
            rc = TE_OS_RC(TE_TA_UNIX, errno);
        else if (S_ISDIR(st.st_mode))
            rc = afc_upload_tree(afc, l.ptr, r.ptr);
        else
            rc = afc_upload_file(afc, l.ptr, r.ptr);
        te_string_free(&l);
        te_string_free(&r);
    }

    closedir(dir);
    return rc;
}

/* What the installation proxy reports through its callback. */
typedef struct install_status {
    bool done;          /**< The operation finished */
    te_errno rc;        /**< Its outcome */
} install_status;

static void
install_cb(plist_t command, plist_t status, void *user_data)
{
    install_status *st = user_data;
    char *name = NULL;
    char *err_name = NULL;
    char *err_desc = NULL;
    uint64_t code = 0;

    UNUSED(command);

    instproxy_status_get_error(status, &err_name, &err_desc, &code);
    if (err_name != NULL)
    {
        ERROR("Installation proxy: %s: %s", err_name,
              err_desc != NULL ? err_desc : "");
        st->rc = TE_RC(TE_TA_UNIX, TE_EFAIL);
        st->done = true;
    }
    else
    {
        instproxy_status_get_name(status, &name);
        if (name != NULL && strcmp(name, "Complete") == 0)
            st->done = true;
    }

    free(name);
    free(err_name);
    free(err_desc);
}

/* Wait for an installation proxy operation started with a callback. */
static te_errno
install_wait(install_status *st, const char *what)
{
    unsigned int waited = 0;

    while (!st->done)
    {
        if (waited >= INSTALL_TIMEOUT_MS)
        {
            ERROR("%s did not complete in %u s", what,
                  INSTALL_TIMEOUT_MS / 1000);
            return TE_RC(TE_TA_UNIX, TE_ETIMEDOUT);
        }
        te_msleep(200);
        waited += 200;
    }
    return st->rc;
}

/* See description in ta_apple.h */
te_errno
ta_apple_install(const char *udid, const char *path)
{
    te_errno rc;
    idevice_t dev = NULL;
    instproxy_client_t inst = NULL;
    afc_client_t afc = NULL;
    plist_t opts = NULL;
    te_string staged = TE_STRING_INIT;
    struct stat st;
    const char *base;
    install_status status = {.done = false, .rc = 0};
    instproxy_error_t err;

    if (stat(path, &st) != 0)
    {
        ERROR("Cannot stat %s: %s", path, strerror(errno));
        return TE_OS_RC(TE_TA_UNIX, errno);
    }

    rc = instproxy_open(udid, &dev, &inst);
    if (rc != 0)
        return rc;

    if (afc_client_start_service(dev, &afc, LABEL) != AFC_E_SUCCESS)
    {
        ERROR("Cannot start AFC on %s", udid);
        rc = TE_RC(TE_TA_UNIX, TE_EIO);
        goto out;
    }

    afc_make_directory(afc, STAGING);
    base = strrchr(path, '/');
    te_string_append(&staged, STAGING "/%s", base != NULL ? base + 1 : path);

    if (S_ISDIR(st.st_mode))
        rc = afc_upload_tree(afc, path, staged.ptr);
    else
        rc = afc_upload_file(afc, path, staged.ptr);
    if (rc != 0)
        goto out;

    opts = instproxy_client_options_new();
    if (S_ISDIR(st.st_mode))
        instproxy_client_options_add(opts, "PackageType", "Developer", NULL);
    err = instproxy_install(inst, staged.ptr, opts, install_cb, &status);
    if (err != INSTPROXY_E_SUCCESS)
    {
        ERROR("Cannot start installing %s on %s: error %d", path, udid, err);
        rc = TE_RC(TE_TA_UNIX, TE_EIO);
        goto out;
    }
    rc = install_wait(&status, "Installation");
    if (rc == 0)
        RING("Installed %s on %s", path, udid);

out:
    if (opts != NULL)
        instproxy_client_options_free(opts);
    if (afc != NULL)
    {
        afc_remove_path(afc, staged.ptr);
        afc_client_free(afc);
    }
    te_string_free(&staged);
    instproxy_client_free(inst);
    idevice_free(dev);
    return rc;
}

/* See description in ta_apple.h */
te_errno
ta_apple_uninstall(const char *udid, const char *bundle)
{
    te_errno rc;
    idevice_t dev = NULL;
    instproxy_client_t inst = NULL;
    install_status status = {.done = false, .rc = 0};
    instproxy_error_t err;

    rc = instproxy_open(udid, &dev, &inst);
    if (rc != 0)
        return rc;

    err = instproxy_uninstall(inst, bundle, NULL, install_cb, &status);
    if (err != INSTPROXY_E_SUCCESS)
    {
        ERROR("Cannot start uninstalling %s from %s: error %d", bundle, udid,
              err);
        rc = TE_RC(TE_TA_UNIX, TE_EIO);
    }
    else
    {
        rc = install_wait(&status, "Uninstallation");
    }

    instproxy_client_free(inst);
    idevice_free(dev);
    return rc;
}

/* See description in ta_apple.h */
te_errno
ta_apple_screenshot(const char *udid, const char *path)
{
    te_errno rc;
    idevice_t dev = NULL;
    lockdownd_client_t ld = NULL;
    screenshotr_client_t shot = NULL;
    char *data = NULL;
    uint64_t size = 0;
    screenshotr_error_t err;
    int fd;

    rc = device_open(udid, &dev, &ld);
    if (rc != 0)
        return rc;

    err = screenshotr_client_start_service(dev, &shot, LABEL);
    if (err != SCREENSHOTR_E_SUCCESS)
    {
        ERROR("Cannot start screenshotr on %s: error %d (a developer disk "
              "image must be mounted)", udid, err);
        rc = TE_RC(TE_TA_UNIX, TE_EOPNOTSUPP);
        goto out;
    }

    err = screenshotr_take_screenshot(shot, &data, &size);
    if (err != SCREENSHOTR_E_SUCCESS)
    {
        ERROR("Screenshot of %s failed: error %d", udid, err);
        rc = TE_RC(TE_TA_UNIX, TE_EIO);
        goto out;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || write(fd, data, size) != (ssize_t)size)
    {
        ERROR("Cannot write %s: %s", path, strerror(errno));
        rc = TE_OS_RC(TE_TA_UNIX, errno);
    }
    if (fd >= 0)
        close(fd);

out:
    free(data);
    if (shot != NULL)
        screenshotr_client_free(shot);
    lockdownd_client_free(ld);
    idevice_free(dev);
    return rc;
}

/* Send a debugserver command and check the response starts with "OK". */
static te_errno
debugserver_ok(debugserver_client_t ds, const char *name, int argc,
               char **argv, char **response)
{
    debugserver_command_t cmd = NULL;
    char *resp = NULL;
    size_t resp_len = 0;
    debugserver_error_t err;

    debugserver_command_new(name, argc, argv, &cmd);
    err = debugserver_client_send_command(ds, cmd, &resp, &resp_len);
    debugserver_command_free(cmd);
    if (err != DEBUGSERVER_E_SUCCESS)
    {
        ERROR("debugserver command %s failed: error %d", name, err);
        free(resp);
        return TE_RC(TE_TA_UNIX, TE_EIO);
    }
    if (resp == NULL || strncmp(resp, "OK", 2) != 0)
    {
        ERROR("debugserver answered %s with '%s'", name,
              resp != NULL ? resp : "");
        free(resp);
        return TE_RC(TE_TA_UNIX, TE_EFAIL);
    }
    if (response != NULL)
        *response = resp;
    else
        free(resp);
    return 0;
}

/* See description in ta_apple.h */
te_errno
ta_apple_launch(const char *udid, const char *bundle, unsigned int *pid)
{
    te_errno rc;
    idevice_t dev = NULL;
    instproxy_client_t inst = NULL;
    debugserver_client_t ds = NULL;
    char *app_path = NULL;
    char *resp = NULL;
    char *argv[2];
    debugserver_command_t cmd = NULL;
    size_t resp_len = 0;
    instproxy_error_t ierr;
    debugserver_error_t derr;

    *pid = 0;

    rc = instproxy_open(udid, &dev, &inst);
    if (rc != 0)
        return rc;

    ierr = instproxy_client_get_path_for_bundle_identifier(inst, bundle,
                                                           &app_path);
    if (ierr != INSTPROXY_E_SUCCESS || app_path == NULL)
    {
        ERROR("%s is not installed on %s", bundle, udid);
        rc = TE_RC(TE_TA_UNIX, TE_ENOENT);
        goto out;
    }

    derr = debugserver_client_start_service(dev, &ds, LABEL);
    if (derr != DEBUGSERVER_E_SUCCESS)
    {
        ERROR("Cannot start debugserver on %s: error %d (a developer disk "
              "image must be mounted)", udid, derr);
        rc = TE_RC(TE_TA_UNIX, TE_EOPNOTSUPP);
        goto out;
    }

    /* The A packet launches the app stopped; D detaches and lets it run */
    argv[0] = app_path;
    argv[1] = NULL;
    derr = debugserver_client_set_argv(ds, 1, argv, &resp);
    if (derr != DEBUGSERVER_E_SUCCESS || resp == NULL ||
        strncmp(resp, "OK", 2) != 0)
    {
        ERROR("debugserver did not accept the launch of %s: %s", bundle,
              resp != NULL ? resp : "no response");
        rc = TE_RC(TE_TA_UNIX, TE_EFAIL);
        goto out;
    }
    free(resp);
    resp = NULL;

    rc = debugserver_ok(ds, "qLaunchSuccess", 0, NULL, NULL);
    if (rc != 0)
        goto out;

    debugserver_command_new("qProcessInfo", 0, NULL, &cmd);
    if (debugserver_client_send_command(ds, cmd, &resp, &resp_len) ==
        DEBUGSERVER_E_SUCCESS && resp != NULL)
    {
        const char *p = strstr(resp, "pid:");

        if (p != NULL)
            *pid = strtoul(p + 4, NULL, 16);
    }
    debugserver_command_free(cmd);
    free(resp);
    resp = NULL;

    rc = debugserver_ok(ds, "D", 0, NULL, NULL);
    if (rc == 0)
        RING("Launched %s on %s, pid %u", bundle, udid, *pid);

out:
    free(app_path);
    if (ds != NULL)
        debugserver_client_free(ds);
    instproxy_client_free(inst);
    idevice_free(dev);
    return rc;
}

/* See description in ta_apple.h */
te_errno
ta_apple_push(const char *udid, const char *local, const char *remote)
{
    te_errno rc;
    idevice_t dev = NULL;
    afc_client_t afc = NULL;

    rc = device_open(udid, &dev, NULL);
    if (rc != 0)
        return rc;

    if (afc_client_start_service(dev, &afc, LABEL) != AFC_E_SUCCESS)
    {
        ERROR("Cannot start AFC on %s", udid);
        rc = TE_RC(TE_TA_UNIX, TE_EIO);
    }
    else
    {
        rc = afc_upload_file(afc, local, remote);
        afc_client_free(afc);
    }

    idevice_free(dev);
    return rc;
}

/* See description in ta_apple.h */
te_errno
ta_apple_pull(const char *udid, const char *remote, const char *local)
{
    te_errno rc;
    idevice_t dev = NULL;
    afc_client_t afc = NULL;
    uint64_t handle = 0;
    char *buf = NULL;
    int fd = -1;
    afc_error_t err;

    rc = device_open(udid, &dev, NULL);
    if (rc != 0)
        return rc;

    if (afc_client_start_service(dev, &afc, LABEL) != AFC_E_SUCCESS)
    {
        ERROR("Cannot start AFC on %s", udid);
        rc = TE_RC(TE_TA_UNIX, TE_EIO);
        goto out;
    }

    err = afc_file_open(afc, remote, AFC_FOPEN_RDONLY, &handle);
    if (err != AFC_E_SUCCESS)
    {
        ERROR("Cannot open %s on the device: AFC error %d", remote, err);
        rc = TE_RC(TE_TA_UNIX, TE_ENOENT);
        goto out;
    }

    fd = open(local, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        ERROR("Cannot create %s: %s", local, strerror(errno));
        rc = TE_OS_RC(TE_TA_UNIX, errno);
        goto out;
    }

    buf = TE_ALLOC(AFC_CHUNK);
    for (;;)
    {
        uint32_t got = 0;

        err = afc_file_read(afc, handle, buf, AFC_CHUNK, &got);
        if (err != AFC_E_SUCCESS)
        {
            ERROR("Reading %s from the device failed: AFC error %d", remote,
                  err);
            rc = TE_RC(TE_TA_UNIX, TE_EIO);
            break;
        }
        if (got == 0)
            break;
        if (write(fd, buf, got) != (ssize_t)got)
        {
            rc = TE_OS_RC(TE_TA_UNIX, errno);
            break;
        }
    }

out:
    free(buf);
    if (fd >= 0)
        close(fd);
    if (handle != 0)
        afc_file_close(afc, handle);
    if (afc != NULL)
        afc_client_free(afc);
    idevice_free(dev);
    return rc;
}

/* A system log capture: syslog_relay delivers characters to a file. */
typedef struct syslog_capture {
    bool used;                      /**< Slot in use */
    idevice_t dev;                  /**< Device */
    syslog_relay_client_t relay;    /**< Service client */
    int fd;                         /**< Capture file */
    char path[64];                  /**< Its path */
    char buf[4096];                 /**< Characters not written yet */
    size_t buf_len;                 /**< Their number */
    pthread_mutex_t lock;           /**< Guards the buffer and the file */
} syslog_capture;

static syslog_capture captures[SYSLOG_MAX];
static pthread_mutex_t captures_lock = PTHREAD_MUTEX_INITIALIZER;

/* Write the buffered characters out; called with the lock held. */
static void
capture_flush(syslog_capture *cap)
{
    if (cap->buf_len > 0)
    {
        if (write(cap->fd, cap->buf, cap->buf_len) < 0)
            WARN("Cannot write %s: %s", cap->path, strerror(errno));
        cap->buf_len = 0;
    }
}

static void
syslog_cb(char c, void *user_data)
{
    syslog_capture *cap = user_data;

    pthread_mutex_lock(&cap->lock);
    cap->buf[cap->buf_len++] = c;
    if (cap->buf_len == sizeof(cap->buf) || c == '\n')
        capture_flush(cap);
    pthread_mutex_unlock(&cap->lock);
}

/* See description in ta_apple.h */
te_errno
ta_apple_syslog_start(const char *udid, unsigned int *id)
{
    te_errno rc;
    syslog_capture *cap = NULL;
    unsigned int i;
    syslog_relay_error_t err;

    pthread_mutex_lock(&captures_lock);
    for (i = 0; i < SYSLOG_MAX; i++)
    {
        if (!captures[i].used)
        {
            cap = &captures[i];
            cap->used = true;
            break;
        }
    }
    pthread_mutex_unlock(&captures_lock);
    if (cap == NULL)
    {
        ERROR("All %u system log capture slots are in use", SYSLOG_MAX);
        return TE_RC(TE_TA_UNIX, TE_EBUSY);
    }

    rc = device_open(udid, &cap->dev, NULL);
    if (rc != 0)
        goto fail;

    err = syslog_relay_client_start_service(cap->dev, &cap->relay, LABEL);
    if (err != SYSLOG_RELAY_E_SUCCESS)
    {
        ERROR("Cannot start syslog_relay on %s: error %d", udid, err);
        rc = TE_RC(TE_TA_UNIX, TE_EIO);
        goto fail;
    }

    snprintf(cap->path, sizeof(cap->path), SYSLOG_DIR "/te_syslog_%u_%u.log",
             (unsigned int)getpid(), i);
    cap->fd = open(cap->path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (cap->fd < 0)
    {
        ERROR("Cannot create %s: %s", cap->path, strerror(errno));
        rc = TE_OS_RC(TE_TA_UNIX, errno);
        goto fail;
    }
    cap->buf_len = 0;
    pthread_mutex_init(&cap->lock, NULL);

    err = syslog_relay_start_capture(cap->relay, syslog_cb, cap);
    if (err != SYSLOG_RELAY_E_SUCCESS)
    {
        ERROR("Cannot start the system log capture on %s: error %d", udid,
              err);
        rc = TE_RC(TE_TA_UNIX, TE_EIO);
        goto fail;
    }

    *id = i;
    return 0;

fail:
    if (cap->fd >= 0)
    {
        close(cap->fd);
        unlink(cap->path);
    }
    cap->fd = -1;
    if (cap->relay != NULL)
        syslog_relay_client_free(cap->relay);
    cap->relay = NULL;
    if (cap->dev != NULL)
        idevice_free(cap->dev);
    cap->dev = NULL;
    cap->used = false;
    return rc;
}

/* See description in ta_apple.h */
te_errno
ta_apple_syslog_read(unsigned int id, uint64_t offset, te_string *out,
                     uint64_t *next)
{
    syslog_capture *cap;
    int fd;
    char buf[4096];
    ssize_t n;

    if (id >= SYSLOG_MAX || !captures[id].used)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);
    cap = &captures[id];

    pthread_mutex_lock(&cap->lock);
    capture_flush(cap);
    pthread_mutex_unlock(&cap->lock);

    fd = open(cap->path, O_RDONLY);
    if (fd < 0)
        return TE_OS_RC(TE_TA_UNIX, errno);
    if (lseek(fd, offset, SEEK_SET) < 0)
    {
        close(fd);
        return TE_OS_RC(TE_TA_UNIX, errno);
    }
    while ((n = read(fd, buf, sizeof(buf))) > 0)
    {
        te_string_append(out, "%.*s", (int)n, buf);
        offset += n;
    }
    close(fd);

    *next = offset;
    return 0;
}

/* See description in ta_apple.h */
te_errno
ta_apple_syslog_stop(unsigned int id)
{
    syslog_capture *cap;

    if (id >= SYSLOG_MAX || !captures[id].used)
        return TE_RC(TE_TA_UNIX, TE_ENOENT);
    cap = &captures[id];

    syslog_relay_stop_capture(cap->relay);
    syslog_relay_client_free(cap->relay);
    cap->relay = NULL;
    idevice_free(cap->dev);
    cap->dev = NULL;
    pthread_mutex_lock(&cap->lock);
    capture_flush(cap);
    close(cap->fd);
    cap->fd = -1;
    pthread_mutex_unlock(&cap->lock);
    pthread_mutex_destroy(&cap->lock);
    unlink(cap->path);
    cap->used = false;
    return 0;
}
