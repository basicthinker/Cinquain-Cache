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

#include "cinq_cache.h"

int main(int argc, const char *argv[]) {
    printf("This shall be done!\n");
    
    rwcache_init();
    
    rwcache_fini();
    
    return 0;
}

