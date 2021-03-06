/*
 * Copyright 2014-2017 Frank Hunleth
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

#ifndef FWFILE_H
#define FWFILE_H

#include <confuse.h>
#include <archive.h>
#include "sparse_file.h"

#define FWFILE_MAX_ARCHIVE_PATH     512

struct fwfile_assertions {
    off_t assert_lte; // bytes
    off_t assert_gte; // bytes
};

int fwfile_add_meta_conf(cfg_t *cfg, struct archive *a, const unsigned char *signing_key);
int fwfile_add_meta_conf_str(const char *configtxt, int configtxt_len,
                             struct archive *a, const unsigned char *signing_key);

#endif // FWFILE_H
