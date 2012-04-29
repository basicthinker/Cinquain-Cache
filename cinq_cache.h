/*
 * Copyright (C) 2012 Yang Zhang <yang.zhang@stanzax.org>
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
//  cinq_cache.h
//  Cinquain Cache
//
//  Created by Jinglei Ren <jinglei.ren@gmail.com> on 4/27/12.
//  Modified by Yang Zhang.
//

#ifndef CINQUAIN_CACHE_H_
#define CINQUAIN_CACHE_H_



#ifdef __KERNEL__
#include <linux/list.h>
#include <linux/slab.h>

// Either users or the internal should use the predefined malloc/free functions.
#define ALLOC(nbytes)   kmalloc(nbytes, GFP_KERNEL)
#define FREE(ptr)       kfree(ptr)

#else
#include <stddef.h> // for NULL
#include "list.h"

#define ALLOC(nbytes)   malloc(nbytes)
#define FREE(ptr)       free(ptr)

#endif // __KERNEL__


#ifndef OFFSET_T_
typedef unsigned long offset_t;
#define OFFSET_T_
#endif // OFFSET_T_

#define FINGERPRINT_BYTES 16


struct fingerprint {
	unsigned long uid; // denotes who makes request
	char value[FINGERPRINT_BYTES];
};

// Readonly chunck of dat.
struct data_entry {
	const char *data;
	offset_t offset;
	offset_t len;
	struct list_head entry;
};

struct data_set {
	struct list_head entries;
};

// helper function to free memory used by a data_set
void free_data_set(struct data_set* ds);

// init cache system
void rwcache_init();

// finalize cache system
void rwcache_fini();

// Returns data set sorted by offsets of its entries without overlaps.
// Users take charge of deallocation of returned data.
extern struct data_set *rcache_get(struct fingerprint *fp, offset_t offset, offset_t len);

// Add previous non-hit data.
// Data input are SAFE to free by users after the function returns.
extern void rcache_put(struct fingerprint *fp, struct data_entry *de);

// Returns data set sorted by offsets of its entries without overlaps.
// Users should NOT deallocate returned data.
// They are SAFE to use until wcache_collect() is invoked.
extern struct data_set *wcache_read(struct fingerprint *fp, offset_t offset, offset_t len);

// Data input are SAFE to free by users after the function returns.
extern int wcache_write(struct fingerprint *fp, struct data_entry *de); 

// Returns data set sorted by offsets of its entries without overlaps.
// Users take charge of deallocation of returned data.
extern struct data_set *wcache_collect(struct fingerprint *fp);


#endif // CINQAIN_CACHE_H_
