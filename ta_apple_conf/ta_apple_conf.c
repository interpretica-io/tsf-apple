/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Apple agent library
 *
 * The /agent/apple configuration subtree:
 *
 *   /agent/apple/device:<udid>            attached devices (RO)
 *   /agent/apple/device/name              DeviceName (RO)
 *   /agent/apple/device/product           ProductType (RO)
 *   /agent/apple/device/version           ProductVersion (RO)
 *   /agent/apple/device/app:<bundle>      user apps; add with a
 *                                         package path on the agent as
 *                                         the value installs, delete
 *                                         uninstalls (read_create)
 *   /agent/apple/simulator:<udid>         available simulators (RO)
 *   /agent/apple/simulator/name           (RO)
 *   /agent/apple/simulator/state          "Booted" or "Shutdown" (RW)
 *   /agent/apple/simulator/app:<bundle>   as for devices (read_create)
 *
 * The model is in cm_apple.yml of tapi_apple.
 */

#define TE_LGR_USER     "TA Apple Conf"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_str.h"
#include "te_string.h"
#include "logger_api.h"
#include "rcf_pch.h"
#include "rcf_pch_conf_ext.h"

#include "ta_apple.h"

/* Turn lines into the space-separated list the Configurator wants. */
static void
lines_to_list(te_string *lines, unsigned int field, char **list)
{
    te_string names = TE_STRING_INIT;
    char *line;
    char *save = NULL;

    /* An empty answer leaves the string unallocated */
    for (line = lines->ptr != NULL ? strtok_r(lines->ptr, "\n", &save) : NULL;
         line != NULL; line = strtok_r(NULL, "\n", &save))
    {
        char *fsave = NULL;
        char *f = strtok_r(line, "\t", &fsave);
        unsigned int i;

        for (i = 0; i < field && f != NULL; i++)
            f = strtok_r(NULL, "\t", &fsave);
        if (f != NULL)
            te_string_append(&names, "%s%s", names.len > 0 ? " " : "", f);
    }

    *list = names.ptr;
}

/* Copy a string into a Configurator value. */
static void
to_value(char *value, const te_string *str)
{
    te_strlcpy(value, str->ptr != NULL ? str->ptr : "", RCF_MAX_VAL);
}

static te_errno
device_list(unsigned int gid, const char *oid, const char *sub_id,
            char **list)
{
    te_string lines = TE_STRING_INIT;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(sub_id);

    if (ta_apple_devices(&lines) != 0)
    {
        *list = NULL;
        te_string_free(&lines);
        return 0;
    }

    lines_to_list(&lines, 0, list);
    te_string_free(&lines);
    return 0;
}

static te_errno
device_info_get(char *value, const char *udid, const char *key)
{
    te_errno rc;
    te_string str = TE_STRING_INIT;

    rc = ta_apple_info(udid, key, &str);
    if (rc == 0)
        to_value(value, &str);
    te_string_free(&str);
    return rc;
}

static te_errno
device_name_get(unsigned int gid, const char *oid, char *value,
                const char *apple, const char *udid)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(apple);

    return device_info_get(value, udid, "DeviceName");
}

static te_errno
device_product_get(unsigned int gid, const char *oid, char *value,
                   const char *apple, const char *udid)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(apple);

    return device_info_get(value, udid, "ProductType");
}

static te_errno
device_version_get(unsigned int gid, const char *oid, char *value,
                   const char *apple, const char *udid)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(apple);

    return device_info_get(value, udid, "ProductVersion");
}

static te_errno
device_app_list(unsigned int gid, const char *oid, const char *sub_id,
                char **list, const char *apple, const char *udid)
{
    te_errno rc;
    te_string lines = TE_STRING_INIT;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(sub_id);
    UNUSED(apple);

    rc = ta_apple_apps(udid, &lines);
    if (rc == 0)
        lines_to_list(&lines, 0, list);
    te_string_free(&lines);
    return rc;
}

static te_errno
app_get(unsigned int gid, const char *oid, char *value, const char *apple,
        const char *udid, const char *bundle)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(apple);
    UNUSED(udid);
    UNUSED(bundle);

    *value = '\0';
    return 0;
}

static te_errno
device_app_add(unsigned int gid, const char *oid, const char *value,
               const char *apple, const char *udid, const char *bundle)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(apple);

    if (value == NULL || *value == '\0')
    {
        ERROR("Adding app %s needs a package path on the agent as the value",
              bundle);
        return TE_RC(TE_TA_UNIX, TE_EINVAL);
    }
    return ta_apple_install(udid, value);
}

static te_errno
device_app_del(unsigned int gid, const char *oid, const char *apple,
               const char *udid, const char *bundle)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(apple);

    return ta_apple_uninstall(udid, bundle);
}

static te_errno
sim_list(unsigned int gid, const char *oid, const char *sub_id, char **list)
{
    te_string lines = TE_STRING_INIT;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(sub_id);

    if (ta_apple_sim_list(&lines) != 0)
    {
        /* No simctl on this agent: no simulators */
        *list = NULL;
        te_string_free(&lines);
        return 0;
    }

    lines_to_list(&lines, 0, list);
    te_string_free(&lines);
    return 0;
}

static te_errno
sim_field_get(char *value, const char *udid, const char *field)
{
    te_errno rc;
    te_string str = TE_STRING_INIT;

    rc = ta_apple_sim_get(udid, field, &str);
    if (rc == 0)
        to_value(value, &str);
    te_string_free(&str);
    return rc;
}

static te_errno
sim_name_get(unsigned int gid, const char *oid, char *value,
             const char *apple, const char *udid)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(apple);

    return sim_field_get(value, udid, "name");
}

static te_errno
sim_state_get(unsigned int gid, const char *oid, char *value,
              const char *apple, const char *udid)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(apple);

    return sim_field_get(value, udid, "state");
}

static te_errno
sim_state_set(unsigned int gid, const char *oid, const char *value,
              const char *apple, const char *udid)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(apple);

    if (strcmp(value, "Booted") == 0)
        return ta_apple_sim_set_booted(udid, true);
    if (strcmp(value, "Shutdown") == 0)
        return ta_apple_sim_set_booted(udid, false);

    ERROR("Simulator state must be Booted or Shutdown, not '%s'", value);
    return TE_RC(TE_TA_UNIX, TE_EINVAL);
}

static te_errno
sim_app_list(unsigned int gid, const char *oid, const char *sub_id,
             char **list, const char *apple, const char *udid)
{
    te_errno rc;
    te_string lines = TE_STRING_INIT;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(sub_id);
    UNUSED(apple);

    rc = ta_apple_sim_apps(udid, &lines);
    if (rc == 0)
        lines_to_list(&lines, 0, list);
    te_string_free(&lines);
    return rc;
}

static te_errno
sim_app_add(unsigned int gid, const char *oid, const char *value,
            const char *apple, const char *udid, const char *bundle)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(apple);

    if (value == NULL || *value == '\0')
    {
        ERROR("Adding app %s needs a package path on the agent as the value",
              bundle);
        return TE_RC(TE_TA_UNIX, TE_EINVAL);
    }
    return ta_apple_sim_install(udid, value);
}

static te_errno
sim_app_del(unsigned int gid, const char *oid, const char *apple,
            const char *udid, const char *bundle)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(apple);

    return ta_apple_sim_uninstall(udid, bundle);
}

RCF_PCH_CFG_NODE_COLLECTION(node_sim_app, "app", NULL, NULL,
                            sim_app_add, sim_app_del, sim_app_list, NULL);

RCF_PCH_CFG_NODE_RW(node_sim_state, "state", NULL, &node_sim_app,
                    sim_state_get, sim_state_set);

RCF_PCH_CFG_NODE_RO(node_sim_name, "name", NULL, &node_sim_state,
                    sim_name_get);

RCF_PCH_CFG_NODE_RO_COLLECTION(node_simulator, "simulator", &node_sim_name,
                               NULL, NULL, sim_list);

RCF_PCH_CFG_NODE_COLLECTION(node_device_app, "app", NULL, NULL,
                            device_app_add, device_app_del, device_app_list,
                            NULL);

RCF_PCH_CFG_NODE_RO(node_device_version, "version", NULL, &node_device_app,
                    device_version_get);

RCF_PCH_CFG_NODE_RO(node_device_product, "product", NULL,
                    &node_device_version, device_product_get);

RCF_PCH_CFG_NODE_RO(node_device_name, "name", NULL, &node_device_product,
                    device_name_get);

RCF_PCH_CFG_NODE_RO_COLLECTION(node_device, "device", &node_device_name,
                               &node_simulator, NULL, device_list);

RCF_PCH_CFG_NODE_NA(node_apple, "apple", &node_device, NULL);

/* See description in ta_apple.h */
te_errno
ta_apple_conf_init(void)
{
    /* The app collections read their value through a getter */
    node_device_app.get = (rcf_ch_cfg_get)app_get;
    node_sim_app.get = (rcf_ch_cfg_get)app_get;

    return rcf_pch_add_node("/agent", &node_apple);
}

TE_RCF_PCH_CONF_EXT(ta_apple_conf_init);
