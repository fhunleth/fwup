/*
 * Copyright 2016-2017 Frank Hunleth
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UBOOT_ENV_H
#define UBOOT_ENV_H

#include <stdint.h>
#include <confuse.h>

struct uboot_name_value {
    char *name;
    char *value;
    struct uboot_name_value *next;
};

struct uboot_env {
    uint32_t block_offset;
    uint32_t block_count;
    size_t env_size;

    // Ignored if the environment is not redundant
    uint8_t flags;

    struct uboot_name_value *vars;
};

int uboot_env_verify_cfg(cfg_t *cfg);
int uboot_env_create_cfg(cfg_t *cfg, struct uboot_env *output, struct uboot_env *output_redund);

int uboot_env_read(struct uboot_env *env, const char *buffer, int redundant);
int uboot_env_setenv(struct uboot_env *env, const char *name, const char *value);
int uboot_env_unsetenv(struct uboot_env *env, const char *name);
int uboot_env_getenv(struct uboot_env *env, const char *name, char **value);
int uboot_env_write(struct uboot_env *env, char *buffer, int redundant);
void uboot_env_free(struct uboot_env *env);

#endif // UBOOT_ENV_H
