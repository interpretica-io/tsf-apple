# tsf-apple

iOS device and simulator control for the OKTET Labs Test Environment
(TE), packaged as an external TE repository (consumed with the
`TE_EXT_REPO` builder directive).

Four libraries:

- `ta_apple` — agent side. Physical devices go through
  libimobiledevice: device information from lockdownd, apps through
  the installation proxy, files through AFC, screenshots through
  screenshotr, the system log through syslog_relay, app launch through
  debugserver. Any host with usbmuxd serves as the agent, Linux
  included. Simulators go through `xcrun simctl` on a macOS agent with
  Xcode. The library registers the `/agent/apple` subtree: the devices
  and the simulators the agent sees, their names, product types and
  versions, the simulator state (set it to boot or shut down), and the
  installed apps as `read_create` instances, so that adding one
  installs a package and a Configurator rollback uninstalls it.
- `ta_apple_conf` — the `/agent/apple` subtree on top of `ta_apple`, linked
  into the agent alone.
- `rpcs_apple` — the `apple_*` RPCs for the RPC server of the agent:
  screenshot, launch, terminate, open a URL, AFC push and pull, the
  system log capture.
- `tapi_apple` — engine side. `tapi_apple.h` gives a test one handle
  type for a device or a simulator and the operations:
  `tapi_apple_install()` (Configurator), `tapi_apple_launch()` (RPC),
  `tapi_apple_screenshot()` (the file goes to the engine and into the
  log as a test artifact), `tapi_apple_syslog_expect()`,
  `tapi_apple_sim_boot()` and the rest. `cm_apple.yml` is the
  Configurator model of the subtree.

## Agent host requirements

For devices:

- `libimobiledevice`, `libplist`, `libusbmuxd` and the `usbmuxd`
  daemon (Debian: `apt install libimobiledevice-dev libplist-dev
  usbmuxd`);
- the device paired with the host and unlocked;
- for screenshots and app launch, a developer disk image mounted on
  the device (`ideviceimagemounter`, or Xcode once).

For simulators: macOS with Xcode; `xcrun simctl` and `plutil` on
`PATH`.

## Usage

Declare the repository in an external libraries catalog and pass it to
`dispatcher.sh --external=<catalog.yml>`:

```yaml
repositories:
  - name: tsf_apple
    url: https://github.com/interpretica-io/tsf-apple.git
    ref: v1.0.0
    libs:
      - ta_apple
      - ta_apple_conf
      - rpcs_apple
      - tapi_apple
```

In `builder.conf`, bind `tapi_apple` to the engine platform and the
agent libraries to the agent platform, add the RPC definitions to
`rpcxdr` on both, put `ta_apple` and `ta_apple_conf` into the agent and
`ta_apple` with `rpcs_apple` into the RPC server:

```
TE_EXT_REPO_USE([tsf_apple], [], [tapi_apple])
TE_LIB_PARMS([rpcxdr], [], [],
             [--with-rpcdefs=tarpc_job.x.m4,../ta_apple/apple_rpc.x.m4])

TE_EXT_REPO_USE([tsf_apple], [<agent platform>], [ta_apple ta_apple_conf rpcs_apple])
TE_LIB_PARMS([rpcxdr], [<agent platform>], [],
             [--with-rpcdefs=tarpc_job.x.m4,../ta_apple/apple_rpc.x.m4])
TE_TA_TYPE([<ta type>], [<agent platform>], [unix], [--with-rcf-rpc],
           [], [], [], [comm_net_agent rcfpch ta_apple ta_apple_conf])
TE_TA_APP([ta_rpcprovider], [<agent platform>], [<ta type>],
          [ta_rpcprovider], [], [],
          [... rpcs_job rpcs_apple ta_apple rpcserver agentlib rpcxdrta ...],
          [\${EXT_SOURCES}/build.sh], [ta_rpcs], [])
```

`rpcs_apple` must come before `rpcxdrta` in the list: `tarpc.c` in
`rpcxdrta` has a weak stub for every RPC and the linker keeps the first
definition it meets, so an `rpcs_*` library after it never gets linked
in and the RPCs fail with `RPC-ERPCNOTSUPP`.

Include `cm_apple.yml` in the Configurator configuration of the suite,
add `tapi_apple` to the `te_libs` of the suite and write the test:

```c
#include "tapi_apple.h"

    CHECK_RC(rcf_rpc_server_create(ta, "pco_apple", &rpcs));
    CHECK_RC(tapi_apple_create(rpcs, udid, false, &dev));

    TEST_STEP("Install and start the app");
    CHECK_RC(tapi_apple_install_from_engine(dev, "com.example.app", ipa));
    CHECK_RC(tapi_apple_syslog_start(dev, &sl));
    CHECK_RC(tapi_apple_launch(dev, "com.example.app", &pid));
    CHECK_RC(tapi_apple_syslog_expect(sl, "com.example.app", 10000, NULL));
    CHECK_RC(tapi_apple_screenshot(dev, "started", NULL));

cleanup:
    tapi_apple_destroy(dev);
```

For a simulator, create the handle with `simulator = true`, boot it
with `tapi_apple_sim_boot()` and use the same install, launch and
screenshot calls; `tapi_apple_terminate()` and
`tapi_apple_sim_openurl()` work for simulators only, the system log
capture for devices only.

## Limits

- App launch on a device goes through debugserver: the app starts
  under the debugger and the library detaches. Terminating an app on
  a device is not supported.
- There is no stand-in for a device or a simulator: without one the
  `/agent/apple` subtree lists nothing and the RPCs return errors.

Requires TE with `TE_EXT_REPO` support.
