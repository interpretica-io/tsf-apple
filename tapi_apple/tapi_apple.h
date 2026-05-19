/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Apple TAPI
 *
 * @defgroup tapi_apple iOS device and simulator control (tapi_apple)
 * @{
 *
 * Engine-side TAPI for an iOS device attached to a Test Agent, or a
 * simulator on a macOS agent. The agent runs ta_apple: devices go
 * through libimobiledevice, simulators through simctl. Their state
 * is in the /agent/apple subtree of the Configurator, the actions go
 * through the apple_* RPCs.
 *
 * @code
 * rcf_rpc_server *rpcs;
 * tapi_apple *dev;
 *
 * CHECK_RC(rcf_rpc_server_create(ta, "pco_apple", &rpcs));
 * CHECK_RC(tapi_apple_create(rpcs, udid, false, &dev));
 * CHECK_RC(tapi_apple_install(dev, "com.example.app", "/opt/app.ipa"));
 * CHECK_RC(tapi_apple_launch(dev, "com.example.app", NULL));
 * CHECK_RC(tapi_apple_screenshot(dev, "started", NULL));
 * tapi_apple_destroy(dev);
 * @endcode
 */

#ifndef __TAPI_APPLE_H__
#define __TAPI_APPLE_H__

#include "te_defs.h"
#include "te_errno.h"
#include "te_string.h"
#include "rcf_rpc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Device or simulator handle. */
typedef struct tapi_apple tapi_apple;

/** Handle of a running system log capture. */
typedef struct tapi_apple_syslog tapi_apple_syslog;

/**
 * List the devices or the simulators an agent sees.
 *
 * @param      ta          Test Agent name
 * @param      simulators  @c true for simulators, @c false for devices
 * @param[out] list        UDIDs, one per line (appended)
 *
 * @return Status code.
 */
extern te_errno tapi_apple_list(const char *ta, bool simulators,
                                te_string *list);

/**
 * Create a handle.
 *
 * @param      rpcs       RPC server on the agent
 * @param      udid       Device or simulator UDID (@c NULL: the only
 *                        one the agent lists)
 * @param      simulator  @c true for a simulator
 * @param[out] dev        Handle
 *
 * @return Status code (TE_ENOENT when the agent does not list it).
 */
extern te_errno tapi_apple_create(rcf_rpc_server *rpcs, const char *udid,
                                  bool simulator, tapi_apple **dev);

/**
 * Free a handle. A running system log capture is stopped.
 *
 * @param dev   Handle (may be @c NULL)
 */
extern void tapi_apple_destroy(tapi_apple *dev);

/**
 * Get the UDID.
 *
 * @param dev   Handle
 *
 * @return The UDID; the string belongs to the handle.
 */
extern const char *tapi_apple_udid(const tapi_apple *dev);

/**
 * Get the name of a device or a simulator.
 *
 * @param      dev   Handle
 * @param[out] name  Name (appended)
 *
 * @return Status code.
 */
extern te_errno tapi_apple_name(tapi_apple *dev, te_string *name);

/**
 * Get the product type and the iOS version of a device.
 *
 * @param      dev      Device handle
 * @param[out] product  ProductType, for example "iPhone15,5" (appended;
 *                      may be @c NULL)
 * @param[out] version  ProductVersion (appended; may be @c NULL)
 *
 * @return Status code.
 */
extern te_errno tapi_apple_info(tapi_apple *dev, te_string *product,
                                te_string *version);

/**
 * Check whether an app is installed.
 *
 * @param      dev        Handle
 * @param      bundle     Bundle identifier
 * @param[out] installed  Result
 *
 * @return Status code.
 */
extern te_errno tapi_apple_is_installed(tapi_apple *dev, const char *bundle,
                                        bool *installed);

/**
 * Install an app from a package on the agent (.ipa file or .app
 * bundle). The Configurator records the app, so a rollback
 * uninstalls it.
 *
 * @param dev     Handle
 * @param bundle  Bundle identifier the package installs
 * @param path    Package path on the agent
 *
 * @return Status code.
 */
extern te_errno tapi_apple_install(tapi_apple *dev, const char *bundle,
                                   const char *path);

/**
 * Copy an .ipa file from the engine to the agent and install it.
 *
 * @param dev     Handle
 * @param bundle  Bundle identifier the package installs
 * @param ipa     .ipa path on the engine
 *
 * @return Status code.
 */
extern te_errno tapi_apple_install_from_engine(tapi_apple *dev,
                                               const char *bundle,
                                               const char *ipa);

/**
 * Uninstall an app.
 *
 * @param dev     Handle
 * @param bundle  Bundle identifier
 *
 * @return Status code.
 */
extern te_errno tapi_apple_uninstall(tapi_apple *dev, const char *bundle);

/**
 * Launch an app.
 *
 * @param      dev     Handle
 * @param      bundle  Bundle identifier
 * @param[out] pid     Process identifier (may be @c NULL)
 *
 * @return Status code.
 */
extern te_errno tapi_apple_launch(tapi_apple *dev, const char *bundle,
                                  unsigned int *pid);

/**
 * Terminate an app (simulators only).
 *
 * @param dev     Simulator handle
 * @param bundle  Bundle identifier
 *
 * @return Status code (TE_EOPNOTSUPP for a device).
 */
extern te_errno tapi_apple_terminate(tapi_apple *dev, const char *bundle);

/**
 * Take a screenshot, copy it to the engine and log it as a test
 * artifact. The file goes to TE_LOG_DIR (or TE_TMP without it)
 * under a unique name that ends with "_<name>.png". A device needs
 * a mounted developer disk image.
 *
 * @param      dev   Handle
 * @param      name  Name for the log and the file
 * @param[out] path  Path of the copy on the engine (appended; may be
 *                   @c NULL)
 *
 * @return Status code.
 */
extern te_errno tapi_apple_screenshot(tapi_apple *dev, const char *name,
                                      te_string *path);

/**
 * Copy a file from the agent to the media directory of a device.
 *
 * @param dev     Device handle
 * @param src     Path on the agent
 * @param dst     Path on the device, relative to the media root
 *
 * @return Status code.
 */
extern te_errno tapi_apple_push(tapi_apple *dev, const char *src,
                                const char *dst);

/**
 * Copy a file from the media directory of a device to the agent.
 *
 * @param dev     Device handle
 * @param src     Path on the device, relative to the media root
 * @param dst     Path on the agent
 *
 * @return Status code.
 */
extern te_errno tapi_apple_pull(tapi_apple *dev, const char *src,
                                const char *dst);

/**
 * Boot a simulator and wait until it reports "Booted".
 *
 * @param dev         Simulator handle
 * @param timeout_ms  Time to wait
 *
 * @return Status code.
 */
extern te_errno tapi_apple_sim_boot(tapi_apple *dev, unsigned int timeout_ms);

/**
 * Shut a simulator down.
 *
 * @param dev   Simulator handle
 *
 * @return Status code.
 */
extern te_errno tapi_apple_sim_shutdown(tapi_apple *dev);

/**
 * Get the state of a simulator.
 *
 * @param      dev    Simulator handle
 * @param[out] state  "Booted", "Shutdown", ... (appended)
 *
 * @return Status code.
 */
extern te_errno tapi_apple_sim_state(tapi_apple *dev, te_string *state);

/**
 * Open a URL on a simulator.
 *
 * @param dev   Simulator handle
 * @param url   URL
 *
 * @return Status code.
 */
extern te_errno tapi_apple_sim_openurl(tapi_apple *dev, const char *url);

/**
 * Start capturing the system log of a device on the agent. The
 * handle keeps one capture at a time.
 *
 * @param      dev  Device handle
 * @param[out] sl   Capture handle
 *
 * @return Status code.
 */
extern te_errno tapi_apple_syslog_start(tapi_apple *dev,
                                        tapi_apple_syslog **sl);

/**
 * Wait for a captured line that matches an extended regular
 * expression. Lines read before are not matched again.
 *
 * @param      sl          Capture handle
 * @param      re          POSIX extended regular expression
 * @param      timeout_ms  Time to wait
 * @param[out] line        The matching line (appended; may be @c NULL)
 *
 * @return Status code (TE_ETIMEDOUT when no line matches in time).
 */
extern te_errno tapi_apple_syslog_expect(tapi_apple_syslog *sl,
                                         const char *re,
                                         unsigned int timeout_ms,
                                         te_string *line);

/**
 * Stop a capture.
 *
 * @param sl    Capture handle (may be @c NULL)
 *
 * @return Status code.
 */
extern te_errno tapi_apple_syslog_stop(tapi_apple_syslog *sl);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TAPI_APPLE_H__ */

/**@} <!-- END tapi_apple --> */
