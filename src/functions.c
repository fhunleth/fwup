/*
 * Copyright 2014 LKC Technologies, Inc.
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

#include "functions.h"
#include "util.h"
#include "fatfs.h"
#include "mbr.h"
#include "fwfile.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "3rdparty/sha2.h"

static int raw_write_validate(struct fun_context *fctx);
static int raw_write_run(struct fun_context *fctx);
static int fat_attrib_validate(struct fun_context *fctx);
static int fat_attrib_run(struct fun_context *fctx);
static int fat_mkfs_validate(struct fun_context *fctx);
static int fat_mkfs_run(struct fun_context *fctx);
static int fat_write_validate(struct fun_context *fctx);
static int fat_write_run(struct fun_context *fctx);
static int fat_mv_validate(struct fun_context *fctx);
static int fat_mv_run(struct fun_context *fctx);
static int fat_rm_validate(struct fun_context *fctx);
static int fat_rm_run(struct fun_context *fctx);
static int fat_mkdir_validate(struct fun_context *fctx);
static int fat_mkdir_run(struct fun_context *fctx);
static int fat_setlabel_validate(struct fun_context *fctx);
static int fat_setlabel_run(struct fun_context *fctx);
static int fw_create_validate(struct fun_context *fctx);
static int fw_create_run(struct fun_context *fctx);
static int fw_add_local_file_validate(struct fun_context *fctx);
static int fw_add_local_file_run(struct fun_context *fctx);
static int mbr_write_validate(struct fun_context *fctx);
static int mbr_write_run(struct fun_context *fctx);

struct fun_info {
    const char *name;
    int (*validate)(struct fun_context *fctx);
    int (*run)(struct fun_context *fctx);
};

static struct fun_info fun_table[] = {
    {"raw_write", raw_write_validate, raw_write_run },
    {"fat_attrib", fat_attrib_validate, fat_attrib_run },
    {"fat_mkfs", fat_mkfs_validate, fat_mkfs_run },
    {"fat_write", fat_write_validate, fat_write_run },
    {"fat_mv", fat_mv_validate, fat_mv_run },
    {"fat_rm", fat_rm_validate, fat_rm_run },
    {"fat_mkdir", fat_mkdir_validate, fat_mkdir_run },
    {"fat_setlabel", fat_setlabel_validate, fat_setlabel_run },
    {"fw_create", fw_create_validate, fw_create_run },
    {"fw_add_local_file", fw_add_local_file_validate, fw_add_local_file_run },
    {"mbr_write", mbr_write_validate, mbr_write_run }
};

#define CHECK_ARG_UINT(ARG, MSG) do { errno=0; strtoul(ARG, NULL, 0); if (errno != 0) ERR_RETURN(MSG); } while (0)

static struct fun_info *lookup(int argc, const char **argv)
{
    if (argc < 1) {
        set_last_error("Not enough parameters");
        return 0;
    }

    size_t i;
    for (i = 0; i < NUM_ELEMENTS(fun_table); i++) {
        if (strcmp(argv[0], fun_table[i].name) == 0) {
            return &fun_table[i];
        }
    }

    set_last_error("Unknown function");
    return 0;
}

/**
 * @brief Validate the parameters passed to the function
 * @param fctx the function context
 * @return 0 if ok
 */
int fun_validate(struct fun_context *fctx)
{
    struct fun_info *fun = lookup(fctx->argc, fctx->argv);
    if (!fun)
        return -1;

    return fun->validate(fctx);
}

/**
 * @brief Run a function
 * @param fctx the function context
 * @return 0 if ok
 */
int fun_run(struct fun_context *fctx)
{
    struct fun_info *fun = lookup(fctx->argc, fctx->argv);
    if (!fun)
        return -1;

    return fun->run(fctx);
}


/**
 * @brief Run all of the functions in a funlist
 * @param fctx the context to use (argc and argv will be updated in it)
 * @param funlist the list
 * @return 0 if ok
 */
int fun_run_funlist(struct fun_context *fctx, cfg_opt_t *funlist)
{
    int ix = 0;
    char *aritystr;
    while ((aritystr = cfg_opt_getnstr(funlist, ix++)) != NULL) {
        fctx->argc = strtoul(aritystr, NULL, 0);
        if (fctx->argc <= 0 || fctx->argc > FUN_MAX_ARGS) {
            set_last_error("Unexpected argc value in funlist");
            return -1;
        }
        int i;
        for (i = 0; i < fctx->argc; i++) {
            fctx->argv[i] = cfg_opt_getnstr(funlist, ix++);
            if (fctx->argv[i] == NULL) {
                set_last_error("Unexpected error with funlist");
                return -1;
            }
        }
        // Clear out the rest of the argv entries to avoid confusion when debugging.
        for (; i < FUN_MAX_ARGS; i++)
            fctx->argv[i] = 0;

        if (fun_run(fctx) < 0)
            return -1;
    }
    return 0;
}

int raw_write_validate(struct fun_context *fctx)
{
    if (fctx->type != FUN_CONTEXT_FILE)
        ERR_RETURN("raw_write only usable in on-resource");

    if (fctx->argc != 2)
        ERR_RETURN("raw_write requires a block offset");

    CHECK_ARG_UINT(fctx->argv[1], "raw_write requires a non-negative integer block offset");

    return 0;
}

int raw_write_run(struct fun_context *fctx)
{
    assert(fctx->type == FUN_CONTEXT_FILE);
    assert(fctx->on_event);

    cfg_t *resource = cfg_gettsec(fctx->cfg, "file-resource", fctx->on_event->title);
    if (!resource)
        ERR_RETURN("raw_write can't find matching file-resource");
    size_t expected_length = cfg_getint(resource, "length");
    char *expected_sha256 = cfg_getstr(resource, "sha256");
    if (strlen(expected_sha256) != SHA256_DIGEST_STRING_LENGTH - 1)
        ERR_RETURN("raw_write detected sha256 with the wrong length");

    // Just in case we're raw writing to the FAT partition, make sure
    // that we flush any cached data.
    fctx->fatfs_ptr(fctx, -1, NULL, NULL);

    int dest_offset = strtoul(fctx->argv[1], NULL, 0) * 512;
    size_t len_written = 0;

    SHA256_CTX ctx256;
    SHA256_Init(&ctx256);
    for (;;) {
        int64_t offset;
        size_t len;
        const void *buffer;

        if (fctx->read(fctx, &buffer, &len, &offset) < 0)
            return -1;

        // Check if done.
        if (len == 0)
            break;

        SHA256_Update(&ctx256, (unsigned char*) buffer, len);

        ssize_t written = pwrite(fctx->output_fd, buffer, len, dest_offset + offset);
        if (written != (ssize_t) len)
            ERR_RETURN("unexpected error writing to destination");

        len_written += len;
    }

    if (len_written != expected_length) {
        if (len_written == 0)
            ERR_RETURN("raw_write didn't write anything. Was it called twice in one on-resource?");
        else
            ERR_RETURN("raw_write didn't write the expected amount");
    }

    char digest[SHA256_DIGEST_STRING_LENGTH];
    SHA256_End(&ctx256, digest);
    if (memcmp(digest, expected_sha256, SHA256_DIGEST_STRING_LENGTH) != 0)
        ERR_RETURN("raw_write detected SHA256 mismatch");

    return 0;
}

int fat_mkfs_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        ERR_RETURN("fat_mkfs requires a block offset and block count");

    CHECK_ARG_UINT(fctx->argv[1], "fat_mkfs requires a non-negative integer block offset");
    CHECK_ARG_UINT(fctx->argv[2], "fat_mkfs requires a non-negative integer block count");

    return 0;
}

int fat_mkfs_run(struct fun_context *fctx)
{
    FILE *fatfp;
    size_t offset;
    if (fctx->fatfs_ptr(fctx, strtoul(fctx->argv[1], NULL, 0), &fatfp, &offset) < 0)
        return -1;

    return fatfs_mkfs(fatfp, offset, strtoul(fctx->argv[2], NULL, 0));
}

int fat_attrib_validate(struct fun_context *fctx)
{
    if (fctx->argc != 4)
        ERR_RETURN("fat_attrib requires a block offset, filename, and attributes (SHR)");

    CHECK_ARG_UINT(fctx->argv[1], "fat_mkfs requires a non-negative integer block offset");

    const char *attrib = fctx->argv[3];
    while (*attrib) {
        switch (*attrib) {
        case 'S':
        case 's':
        case 'H':
        case 'h':
        case 'R':
        case 'r':
            break;

        default:
            ERR_RETURN("fat_attrib only supports R, H, and S attributes");
        }
        attrib++;
    }
    return 0;
}

int fat_attrib_run(struct fun_context *fctx)
{
    FILE *fatfp;
    size_t offset;
    if (fctx->fatfs_ptr(fctx, strtoul(fctx->argv[1], NULL, 0), &fatfp, &offset) < 0)
        return -1;

    return fatfs_attrib(fatfp, offset, fctx->argv[2], fctx->argv[3]);
}

int fat_write_validate(struct fun_context *fctx)
{
    if (fctx->type != FUN_CONTEXT_FILE)
        ERR_RETURN("fat_write only usable in on-resource");

    if (fctx->argc != 3)
        ERR_RETURN("fat_write requires a block offset and destination filename");

    CHECK_ARG_UINT(fctx->argv[1], "fat_write requires a non-negative integer block offset");

    return 0;
}
int fat_write_run(struct fun_context *fctx)
{
    assert(fctx->on_event);

    cfg_t *resource = cfg_gettsec(fctx->cfg, "file-resource", fctx->on_event->title);
    if (!resource)
        ERR_RETURN("fat_write can't find matching file-resource");
    size_t expected_length = cfg_getint(resource, "length");
    char *expected_sha256 = cfg_getstr(resource, "sha256");
    if (strlen(expected_sha256) != SHA256_DIGEST_STRING_LENGTH - 1)
        ERR_RETURN("fat_write detected sha256 with the wrong length");

    FILE *fatfp;
    size_t fatfp_offset;
    size_t len_written = 0;
    if (fctx->fatfs_ptr(fctx, strtoul(fctx->argv[1], NULL, 0), &fatfp, &fatfp_offset) < 0)
        return -1;

    // enforce truncation semantics if the file exists
    fatfs_rm(fatfp, fatfp_offset, fctx->argv[2]);

    SHA256_CTX ctx256;
    SHA256_Init(&ctx256);
    for (;;) {
        int64_t offset;
        size_t len;
        const void *buffer;

        if (fctx->read(fctx, &buffer, &len, &offset) < 0)
            return -1;

        // Check if done.
        if (len == 0)
            break;

        SHA256_Update(&ctx256, (unsigned char*) buffer, len);

        if (fatfs_pwrite(fatfp, fatfp_offset, fctx->argv[2], (int) offset, buffer, len) < 0)
            return -1;

        len_written += len;
    }

    if (len_written != expected_length) {
        if (len_written == 0)
            ERR_RETURN("fat_write didn't write anything. Was it called twice in one on-resource?");
        else
            ERR_RETURN("fat_write didn't write the expected amount");
    }

    char digest[SHA256_DIGEST_STRING_LENGTH];
    SHA256_End(&ctx256, digest);
    if (memcmp(digest, expected_sha256, SHA256_DIGEST_STRING_LENGTH) != 0)
        ERR_RETURN("fat_write detected SHA256 mismatch");

    return 0;
}

int fat_mv_validate(struct fun_context *fctx)
{
    if (fctx->argc != 4)
        ERR_RETURN("fat_mv requires a block offset, old filename, new filename");

    CHECK_ARG_UINT(fctx->argv[1], "fat_mv requires a non-negative integer block offset");
    return 0;
}
int fat_mv_run(struct fun_context *fctx)
{
    FILE *fatfp;
    size_t fatfp_offset;
    if (fctx->fatfs_ptr(fctx, strtoul(fctx->argv[1], NULL, 0), &fatfp, &fatfp_offset) < 0)
        return -1;

    // TODO: Ignore the error code here??
    fatfs_mv(fatfp, fatfp_offset, fctx->argv[2], fctx->argv[3]);

    return 0;
}

int fat_rm_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        ERR_RETURN("fat_rm requires a block offset and filename");

    CHECK_ARG_UINT(fctx->argv[1], "fat_rm requires a non-negative integer block offset");

    return 0;
}
int fat_rm_run(struct fun_context *fctx)
{
    FILE *fatfp;
    size_t fatfp_offset;
    if (fctx->fatfs_ptr(fctx, strtoul(fctx->argv[1], NULL, 0), &fatfp, &fatfp_offset) < 0)
        return -1;

    // TODO: Ignore the error code here??
    fatfs_rm(fatfp, fatfp_offset, fctx->argv[2]);

    return 0;
}

int fat_mkdir_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        ERR_RETURN("fat_mkdir requires a block offset and directory name");

    CHECK_ARG_UINT(fctx->argv[1], "fat_mkdir requires a non-negative integer block offset");

    return 0;
}
int fat_mkdir_run(struct fun_context *fctx)
{
    FILE *fatfp;
    size_t fatfp_offset;
    if (fctx->fatfs_ptr(fctx, strtoul(fctx->argv[1], NULL, 0), &fatfp, &fatfp_offset) < 0)
        return -1;

    // TODO: Ignore the error code here??
    fatfs_mkdir(fatfp, fatfp_offset, fctx->argv[2]);

    return 0;
}

int fat_setlabel_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        ERR_RETURN("fat_setlabel requires a block offset and name");

    CHECK_ARG_UINT(fctx->argv[1], "fat_setlabel requires a non-negative integer block offset");

    return 0;
}
int fat_setlabel_run(struct fun_context *fctx)
{
    FILE *fatfp;
    size_t fatfp_offset;
    if (fctx->fatfs_ptr(fctx, strtoul(fctx->argv[1], NULL, 0), &fatfp, &fatfp_offset) < 0)
        return -1;

    // TODO: Ignore the error code here??
    fatfs_setlabel(fatfp, fatfp_offset, fctx->argv[2]);

    return 0;
}

int fw_create_validate(struct fun_context *fctx)
{
    if (fctx->argc != 2)
        ERR_RETURN("fw_create requires a filename");

    return 0;
}
int fw_create_run(struct fun_context *fctx)
{
    struct archive *a;
    bool created;
    if (fctx->subarchive_ptr(fctx, fctx->argv[1], &a, &created) < 0)
        return -1;

    if (!created)
        ERR_RETURN("fw_create called on archive that was already open");

    if (fwfile_add_meta_conf(fctx->cfg, a) < 0)
        return -1;

    return 0;
}

int fw_add_local_file_validate(struct fun_context *fctx)
{
    if (fctx->argc != 4)
        ERR_RETURN("fw_add_local_file requires a firmware filename, filename, and file with the contents");

    return 0;
}
int fw_add_local_file_run(struct fun_context *fctx)
{
    struct archive *a;
    bool created;
    if (fctx->subarchive_ptr(fctx, fctx->argv[1], &a, &created) < 0)
        return -1;

    if (created)
        ERR_RETURN("call fw_create before calling fw_add_local_file");

    if (fwfile_add_local_file(a, fctx->argv[2], fctx->argv[3]) < 0)
        return -1;

    return 0;
}

int mbr_write_validate(struct fun_context *fctx)
{
    if (fctx->argc != 2)
        ERR_RETURN("mbr_write requires an mbr");

    const char *mbr_name = fctx->argv[1];
    cfg_t *mbrsec = cfg_gettsec(fctx->cfg, "mbr", mbr_name);

    if (!mbrsec)
        ERR_RETURN("mbr_write can't find mbr reference");

    return 0;
}
int mbr_write_run(struct fun_context *fctx)
{
    const char *mbr_name = fctx->argv[1];
    cfg_t *mbrsec = cfg_gettsec(fctx->cfg, "mbr", mbr_name);
    uint8_t buffer[512];

    if (mbr_create_cfg(mbrsec, buffer) < 0)
        return -1;

    ssize_t written = pwrite(fctx->output_fd, buffer, 512, 0);
    if (written != 512)
        ERR_RETURN("unexpected error writing to destination");

    return 0;
}
