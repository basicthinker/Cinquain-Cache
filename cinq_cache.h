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

#ifndef __KERNEL__
#include "list.h"
#else
#include <linux/list.h>
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

// Returns data set sorted by offsets of its entries.
// Data set may include recently written data owned by the request uid.
// Consistency is NOT guaranteed.
extern struct data_set *ccache_read(struct fingerprint *fp, offset_t offset, offset_t len);

// Add previous non-hit data.
// NOT written data, and can be directly put into read cache.
extern void ccache_put(struct fingerprint *fp, struct data_entry *de);

// Provided data entry is subject to deallocation afterwards.
extern int ccache_write(struct fingerprint *fp, struct data_entry *de); 

// Returns all WRITE cache of the specified file.
// Invoked when the file is flushed or closed.
// Data entries are sorted by offsets, and have NO overlaps.
extern struct data_set *ccache_collect(struct fingerprint *fp);

// Invoked when changing a temporary fingerprint to a permanent one.
extern int ccache_move(struct fingerprint *old_fp, struct fingerprint *new_fp);

#endif // CINQAIN_CACHE_H
