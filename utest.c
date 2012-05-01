/*
 * Copyright (C) 2012 Yang Zhang <santa@yzhang.net>
 * Copyright (C) 2012 Jinglei Ren <jinglei.ren@stanzax.org>
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
 *
 */

//
//  test.c
//  Cinquain Cache
//
//  Created by Jinglei Ren <jinglei.ren@gmail.com> on 4/27/12.
//  Modified by Yang Zhang.
//

#include <stdio.h>
#include <string.h>

// for malloc & free
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif // __APPLE__

#include "cinq_cache.h"
#include "trace.h"

void rc_write(struct fingerprint* fpnt, offset_t ofst, offset_t len, char fill) {
    struct data_entry de;
    de.offset = ofst;
    de.len = len;
    de.data = (char *) malloc(len);
    memset(de.data, fill, de.len);
    trace("write: ofst=%ld, len=%ld, fill='%c'", ofst, len, fill);
    rcache_put(fpnt, &de);
    free(de.data);
}

void rc_print(struct fingerprint* fpnt, offset_t ofst, offset_t len) {
    struct data_set* ds = rcache_get(fpnt, ofst, len);
    if (ds == NULL) {
        trace("rcache_get returned NULL, nothing found!");
        return;
    }
    
    struct list_head *cur, *tmp;
    list_for_each_safe(cur, tmp, &(ds->entries)) {
        struct data_entry* de = list_entry(cur, struct data_entry, entry);
        char *data_str = (char *) malloc(de->len + 1);
        memcpy(data_str, de->data, de->len);
        data_str[de->len] = '\0';
        printf("got: ofst=%ld, len=%ld, data='%s'\n", de->offset, de->len, data_str);
        free(data_str);
    }
    free_data_set(ds, 1);
}


void test1() {
    printf("*** donig test1\n");
    struct fingerprint fpnt = { .value = "t-01\0\0\0\0\0\0\0\0\0\0\0\0" };
    rc_print(&fpnt, 0, 10);
    rc_write(&fpnt, 0, 1, 'h');
    rc_print(&fpnt, 0, 10);
    rc_write(&fpnt, 0, 1, 'H');
    rc_write(&fpnt, 3, 5, 'x');
    rc_print(&fpnt, 0, 10);
    rc_write(&fpnt, 0, 7, 'u');
    rc_print(&fpnt, 0, 10);
    printf("*** done test1\n");
}

int main(int argc, const char *argv[]) {
    rwcache_init();
    test1();
    rwcache_fini();
    return 0;
}

