/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief RPC for iOS device and simulator access
 *
 * The RPCs of rpcs_apple. Add this file to the rpcxdr definitions of
 * the engine platform and of the agent platform:
 *
 *   TE_LIB_PARMS([rpcxdr], [<platform>], [],
 *                [--with-rpcdefs=tarpc_job.x.m4,../ta_apple/apple_rpc.x.m4])
 *
 * Every RPC takes the UDID and a flag that says whether it is a
 * simulator, so the engine keeps one handle type for both.
 */

/* apple_screenshot(): write a screenshot to a file on the agent */
struct tarpc_apple_screenshot_in {
    struct tarpc_in_arg common;

    string udid<>;
    tarpc_bool simulator;
    string path<>;
};

struct tarpc_apple_screenshot_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
};

/* apple_launch(): launch an app */
struct tarpc_apple_launch_in {
    struct tarpc_in_arg common;

    string udid<>;
    tarpc_bool simulator;
    string bundle<>;
};

struct tarpc_apple_launch_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
    tarpc_uint pid;
};

/* apple_terminate(): terminate an app (simulators only) */
struct tarpc_apple_terminate_in {
    struct tarpc_in_arg common;

    string udid<>;
    tarpc_bool simulator;
    string bundle<>;
};

struct tarpc_apple_terminate_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
};

/* apple_openurl(): open a URL (simulators only) */
struct tarpc_apple_openurl_in {
    struct tarpc_in_arg common;

    string udid<>;
    string url<>;
};

struct tarpc_apple_openurl_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
};

/* apple_push(): copy a file to the media directory of a device */
struct tarpc_apple_push_in {
    struct tarpc_in_arg common;

    string udid<>;
    string local<>;
    string remote<>;
};

struct tarpc_apple_push_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
};

/* apple_pull(): copy a file from the media directory of a device */
struct tarpc_apple_pull_in {
    struct tarpc_in_arg common;

    string udid<>;
    string remote<>;
    string local<>;
};

struct tarpc_apple_pull_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
};

/* apple_syslog_start(): capture the system log into a file on the agent */
struct tarpc_apple_syslog_start_in {
    struct tarpc_in_arg common;

    string udid<>;
};

struct tarpc_apple_syslog_start_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
    tarpc_uint id;
};

/* apple_syslog_read(): the captured bytes from an offset */
struct tarpc_apple_syslog_read_in {
    struct tarpc_in_arg common;

    tarpc_uint id;
    tarpc_size_t offset;
};

struct tarpc_apple_syslog_read_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
    string data<>;
    tarpc_size_t next;
};

/* apple_syslog_stop() */
struct tarpc_apple_syslog_stop_in {
    struct tarpc_in_arg common;

    tarpc_uint id;
};

struct tarpc_apple_syslog_stop_out {
    struct tarpc_out_arg common;

    tarpc_int retval;
};

program apple
{
    version ver0
    {
        RPC_DEF(apple_screenshot)
        RPC_DEF(apple_launch)
        RPC_DEF(apple_terminate)
        RPC_DEF(apple_openurl)
        RPC_DEF(apple_push)
        RPC_DEF(apple_pull)
        RPC_DEF(apple_syslog_start)
        RPC_DEF(apple_syslog_read)
        RPC_DEF(apple_syslog_stop)
    } = 1;
} = 21;
