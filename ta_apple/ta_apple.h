/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Apple agent library
 *
 * @defgroup ta_apple iOS device and simulator access on a Test Agent (ta_apple)
 * @{
 *
 * Two backends behind one library:
 *
 * - physical devices through libimobiledevice (usbmuxd): device
 *   info from lockdownd, apps through the installation proxy, files
 *   through AFC, screenshots through screenshotr, the system log
 *   through syslog_relay, app launch through debugserver. Works on
 *   any host with usbmuxd, Linux included;
 * - simulators through 'xcrun simctl', on a macOS agent with Xcode.
 *
 * The library registers the /agent/apple configuration subtree, see
 * ta_apple_conf.c, and backs the apple_* RPCs of rpcs_apple.
 */

#ifndef __TA_APPLE_H__
#define __TA_APPLE_H__

#include "te_defs.h"
#include "te_errno.h"
#include "te_string.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Physical devices
 */

/**
 * List the attached devices: one UDID per line.
 *
 * @param[out] out  Lines (appended)
 *
 * @return Status code.
 */
extern te_errno ta_apple_devices(te_string *out);

/**
 * Get a lockdownd value of a device.
 *
 * @param      udid   Device UDID
 * @param      key    Key, for example "DeviceName", "ProductType",
 *                    "ProductVersion"
 * @param[out] value  Value (appended); a non-string value comes as
 *                    its XML plist
 *
 * @return Status code.
 */
extern te_errno ta_apple_info(const char *udid, const char *key,
                              te_string *value);

/**
 * List the user apps of a device: one bundle identifier per line.
 *
 * @param      udid  Device UDID
 * @param[out] out   Lines (appended)
 *
 * @return Status code.
 */
extern te_errno ta_apple_apps(const char *udid, te_string *out);

/**
 * Install an app from an .ipa file or an .app bundle directory on
 * the agent. The package is uploaded to the device staging area
 * first.
 *
 * @param udid  Device UDID
 * @param path  Package path on the agent
 *
 * @return Status code.
 */
extern te_errno ta_apple_install(const char *udid, const char *path);

/**
 * Uninstall an app.
 *
 * @param udid    Device UDID
 * @param bundle  Bundle identifier
 *
 * @return Status code.
 */
extern te_errno ta_apple_uninstall(const char *udid, const char *bundle);

/**
 * Take a screenshot into a file on the agent. The device needs a
 * mounted developer disk image.
 *
 * @param udid  Device UDID
 * @param path  File to write; the format is what the device gives
 *              (TIFF on old versions, PNG on new ones)
 *
 * @return Status code.
 */
extern te_errno ta_apple_screenshot(const char *udid, const char *path);

/**
 * Launch an app through debugserver and detach from it.
 *
 * @param      udid    Device UDID
 * @param      bundle  Bundle identifier
 * @param[out] pid     Process identifier (0 when the device does not
 *                     report it)
 *
 * @return Status code.
 */
extern te_errno ta_apple_launch(const char *udid, const char *bundle,
                                unsigned int *pid);

/**
 * Copy a file from the agent to the media directory of a device
 * (the AFC root, what iTunes file sharing sees).
 *
 * @param udid    Device UDID
 * @param local   Path on the agent
 * @param remote  Path on the device, relative to the media root
 *
 * @return Status code.
 */
extern te_errno ta_apple_push(const char *udid, const char *local,
                              const char *remote);

/**
 * Copy a file from the media directory of a device to the agent.
 *
 * @param udid    Device UDID
 * @param remote  Path on the device, relative to the media root
 * @param local   Path on the agent
 *
 * @return Status code.
 */
extern te_errno ta_apple_pull(const char *udid, const char *remote,
                              const char *local);

/**
 * Start capturing the system log of a device into a file on the
 * agent.
 *
 * @param      udid  Device UDID
 * @param[out] id    Identifier of the capture
 *
 * @return Status code.
 */
extern te_errno ta_apple_syslog_start(const char *udid, unsigned int *id);

/**
 * Read captured system log bytes from an offset.
 *
 * @param      id      Capture identifier
 * @param      offset  Byte offset in the capture
 * @param[out] out     Bytes from the offset (appended)
 * @param[out] next    Offset after the returned bytes
 *
 * @return Status code.
 */
extern te_errno ta_apple_syslog_read(unsigned int id, uint64_t offset,
                                     te_string *out, uint64_t *next);

/**
 * Stop a system log capture and remove its file.
 *
 * @param id    Capture identifier
 *
 * @return Status code.
 */
extern te_errno ta_apple_syslog_stop(unsigned int id);

/*
 * Simulators (macOS agent with Xcode)
 */

/**
 * List the available simulators: one "udid\tstate\tname" line each.
 *
 * @param[out] out  Lines (appended)
 *
 * @return Status code; an agent without simctl lists nothing.
 */
extern te_errno ta_apple_sim_list(te_string *out);

/**
 * Get the name or the state of a simulator.
 *
 * @param      udid   Simulator UDID
 * @param      field  "name" or "state"
 * @param[out] value  Value (appended)
 *
 * @return Status code (TE_ENOENT when no such simulator).
 */
extern te_errno ta_apple_sim_get(const char *udid, const char *field,
                                 te_string *value);

/**
 * Boot or shut down a simulator.
 *
 * @param udid  Simulator UDID
 * @param boot  @c true to boot, @c false to shut down
 *
 * @return Status code.
 */
extern te_errno ta_apple_sim_set_booted(const char *udid, bool boot);

/**
 * List the user apps of a simulator: one bundle identifier per line.
 *
 * @param      udid  Simulator UDID
 * @param[out] out   Lines (appended)
 *
 * @return Status code.
 */
extern te_errno ta_apple_sim_apps(const char *udid, te_string *out);

/**
 * Install an app on a simulator from a path on the agent.
 *
 * @param udid  Simulator UDID
 * @param path  .app bundle directory or .ipa file
 *
 * @return Status code.
 */
extern te_errno ta_apple_sim_install(const char *udid, const char *path);

/**
 * Uninstall an app from a simulator.
 *
 * @param udid    Simulator UDID
 * @param bundle  Bundle identifier
 *
 * @return Status code.
 */
extern te_errno ta_apple_sim_uninstall(const char *udid, const char *bundle);

/**
 * Launch an app on a simulator.
 *
 * @param      udid    Simulator UDID
 * @param      bundle  Bundle identifier
 * @param[out] pid     Process identifier
 *
 * @return Status code.
 */
extern te_errno ta_apple_sim_launch(const char *udid, const char *bundle,
                                    unsigned int *pid);

/**
 * Terminate an app on a simulator.
 *
 * @param udid    Simulator UDID
 * @param bundle  Bundle identifier
 *
 * @return Status code.
 */
extern te_errno ta_apple_sim_terminate(const char *udid,
                                       const char *bundle);

/**
 * Take a screenshot of a simulator into a PNG file on the agent.
 *
 * @param udid  Simulator UDID
 * @param path  File to write
 *
 * @return Status code.
 */
extern te_errno ta_apple_sim_screenshot(const char *udid, const char *path);

/**
 * Open a URL on a simulator.
 *
 * @param udid  Simulator UDID
 * @param url   URL
 *
 * @return Status code.
 */
extern te_errno ta_apple_sim_openurl(const char *udid, const char *url);

/**
 * Initializer of the /agent/apple subtree, registered with
 * TE_RCF_PCH_CONF_EXT().
 *
 * @return Status code.
 */
extern te_errno ta_apple_conf_init(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TA_APPLE_H__ */

/**@} <!-- END ta_apple --> */
