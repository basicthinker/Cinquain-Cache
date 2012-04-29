#include "cinq_cache.h"

#ifdef __KERNEL__

#include <linux/rbtree.h>
#include <linux/spinlock.h>

typedef spinlock_t lock_t;

#define LOCK_INIT   SPIN_LOCK_UNLOCKED
#define lock(m)     spin_lock(&(m))
#define unlock(m)   spin_unlock(&(m))

#else // userspace

#include <pthread.h>
#include "rbtree.h"

typedef pthread_mutex_t lock_t;

#define LOCK_INIT   PTHRED_MUTEX_INIT
#define lock(m)     pthread_mutex_lock(&(m))
#define unlock(m)   pthread_mutex_unlock(&(m))

#endif // __KERNEL__


// rbtree node containing data
struct mynode {
    const char *data;
    offset_t offset;
    offset_t len;
    struct rb_node node;
    struct list_head lru_entry; // used by LRU on R-cache
};


struct hash_entry {
    struct fingerprint fpnt;
    struct list_head entry;
    struct rb_root root;
};


// number of hash slots
#define N_SLOT 1024

// write cache, using a linked hash
static struct list_head wcache[N_SLOT];

// read cache, using a linked hash
static struct list_head rcache[N_SLOT];


// init cache system
void rwcache_init() {
    int i;
    for (i = 0; i < N_SLOT; i++) {
        INIT_LIST_HEAD(&wcache[i]);
        INIT_LIST_HEAD(&rcache[i]);
    }
}




// finalize cache system
void rwcache_fini() {
    int i;
    
    // fini wcache
    for (i = 0; i < N_SLOT; i++) {
        struct list_head* slot_list = &wcache[i];
        struct list_head *cur, *tmp;
        list_for_each_safe(cur, tmp, slot_list) {
            struct hash_entry* he = list_entry(cur, struct hash_entry, entry);
            list_del(&(he->entry));
            
            // free all the rbtrees in the list
            for (;;) {
                struct rb_node* first = rb_first(&(he->root));
                if (first == NULL) {
                    break;
                }
                rb_erase(first, &(he->root));
                
                struct mynode *node = rb_entry(first, struct mynode, node);
                FREE(node->data);
                FREE(node);
            }
            
            FREE(he);
        }
    }
    
    // fini rcache
    for (i = 0; i < N_SLOT; i++) {
        struct list_head slot_list = rcache[i];
        // TODO free all the rbtrees in the list
    }
}

