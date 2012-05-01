#include "cinq_cache.h"

#ifdef __KERNEL__

#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

// Either users or the internal should use the predefined malloc/free functions.
#define ALLOC(nbytes)   ((nbytes) <= PAGE_SIZE ? kmalloc((nbytes), GFP_KERNEL) : vmalloc(nbytes))
#define FREE(ptr, size)       ((size) <= PAGE_SIZE ? kfree(ptr) : vfree(ptr))


typedef spinlock_t lock_t;

#define LOCK_INIT   SPIN_LOCK_UNLOCKED
#define lock(m)     spin_lock(&(m))
#define unlock(m)   spin_unlock(&(m))

#else // userspace

#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif // __APPLE__

#include <pthread.h>
#include <string.h> // for memcpy
#include "rbtree.h"


#define ALLOC(nbytes)   malloc(nbytes)
#define FREE(ptr, size)       free(ptr)


typedef pthread_mutex_t lock_t;

#define LOCK_INIT   PTHRED_MUTEX_INIT
#define lock(m)     pthread_mutex_lock(&(m))
#define unlock(m)   pthread_mutex_unlock(&(m))

#endif // __KERNEL__

#include "trace.h"


struct hash_entry {
    struct fingerprint fpnt;
    struct list_head entry;
    struct rb_root root;
};




// rbtree node containing data
struct mynode {
    char *data;
    offset_t offset;
    offset_t len;
    struct rb_node node;
    struct hash_entry* h_entry;
    struct list_head lru_entry; // used by LRU on R-cache
};


// number of hash slots
#define N_SLOT 1024


// input: fingerprint, return: the slot possibly containing the hash entry
#define fp_slot(fpnt)     (*((int *)(fpnt).value) % N_SLOT)


// write cache, using a linked hash
static struct list_head wcache[N_SLOT];

// read cache, using a linked hash
static struct list_head rcache[N_SLOT];

static ssize_t rcache_size = 0;

static ssize_t rcache_limit = 1024 * 1024 * 512; // 512M cache

// newly accessed element at head, old element at tail
LIST_HEAD(lru_list);


// init cache system
void rwcache_init() {
    int i;
    for (i = 0; i < N_SLOT; i++) {
        INIT_LIST_HEAD(&wcache[i]);
        INIT_LIST_HEAD(&rcache[i]);
    }
}


static int fpnt_eql(struct fingerprint* fpnt1, struct fingerprint* fpnt2) {
    int i;
    for (i = 0; i < FINGERPRINT_BYTES; i++) {
        if (fpnt1->value[i] != fpnt2->value[i]) {
            return 0;
        }
    }
    // TODO: what about UID?
    return 1;
}


static struct hash_entry* hash_find(struct list_head* htab, struct fingerprint *fpnt) {
    struct list_head* slot_list = &htab[fp_slot(*fpnt)];
    struct list_head *cur, *tmp;
    list_for_each_safe(cur, tmp, slot_list) {
        struct hash_entry* he = list_entry(cur, struct hash_entry, entry);
        if (fpnt_eql(&(he->fpnt), fpnt)) {
            return he;
        }
    }
    return NULL;
}


void free_data_set(struct data_set* ds, int free_data) {
    struct list_head *cur, *tmp;
    
    list_for_each_safe(cur, tmp, &(ds->entries)) {
        struct data_entry* de = list_entry(cur, struct data_entry, entry);
        list_del(&(de->entry));
        if (free_data) {
            FREE(de->data, de->len);
        }
        FREE(de, sizeof(struct data_entry));
    }
    
    // release the data set itself
    FREE(ds, sizeof(struct data_set));
}

// Returns data set sorted by offsets of its entries without overlaps.
// Users take charge of deallocation of returned data.
struct data_set *wcache_collect(struct fingerprint *fp) {
    struct data_set* dset = NULL;
    struct hash_entry* he = hash_find(wcache, fp);

    if (he == NULL) {
        // nothing found, return NULL
        return NULL;
    }
    
    struct rb_root* rbroot = &(he->root);
    
    dset = (struct data_set *) ALLOC(sizeof(struct data_set));
    INIT_LIST_HEAD(&(dset->entries));
    
    // release all the rbtree nodes (nodes only, all data have been transfered)
    for (;;) {
        struct rb_node* first = rb_first(rbroot);
        if (first == NULL) {
            break;
        }
        rb_erase(first, rbroot);
        
        struct mynode *node = rb_entry(first, struct mynode, node);
        // don't FREE(node->data) here, all data have been transfered
        
        struct data_entry *de = (struct data_entry *) ALLOC(sizeof(struct data_entry));
        de->data = node->data;
        de->offset = node->offset;
        de->len = node->len;
        list_add(&(de->entry), &(dset->entries));
        
        FREE(node, sizeof(struct mynode));
    }
    
    return dset;
}



// find the first overlap in range [offset, offset + len)
// return NULL if not found
static struct mynode* first_overlap(struct rb_root* root, offset_t offset, offset_t len) {
    struct rb_node* n = root->rb_node;
    struct mynode* ret = NULL;
    while (n) {
        struct mynode* my = container_of(n, struct mynode, node);
        
        if (offset + len <= my->offset) {
            // go left
            n = n->rb_left;
        } else if (my->offset + my->len <= offset) {
            // go right
            n = n->rb_right;
        } else {
            // have over lap, go on to left, seeking the first match
            ret = my;
            n = n->rb_left;
        }
    }
    return ret;
}



struct data_set *rcache_get(struct fingerprint *fp, offset_t offset, offset_t len) {
    struct data_set* dset = NULL;
    struct hash_entry *he = hash_find(rcache, fp);
    
    if (he == NULL) {
        // nothing found, return NULL
        return NULL;
    }
    struct rb_root* rbroot = &(he->root);
    
    dset = (struct data_set *) ALLOC(sizeof(struct data_set));
    INIT_LIST_HEAD(&(dset->entries));
    
    struct mynode* my = first_overlap(rbroot, offset, len);
    while (my) {
        
        if (offset + len <= my->offset) {
            break;
        }
        
        // move newly accessed element to head
        list_move(&(my->lru_entry), &lru_list);
        
        struct data_entry *de = (struct data_entry *) ALLOC(sizeof(struct data_entry));
        de->data = (char *) ALLOC(my->len);
        // copy data
        memcpy(de->data, my->data, my->len);
        de->offset = my->offset;
        de->len = my->len;
        list_add(&(de->entry), &(dset->entries));
        
        struct rb_node* next = rb_next(&(my->node));
        my = container_of(next, struct mynode, node);
    }
    
    return dset;
}


static int rcache_insert_data(struct rb_root *root, offset_t offset, offset_t len, char* data, struct hash_entry* h_entry) {
    struct rb_node **new = &(root->rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new) {
		struct mynode *this = container_of(*new, struct mynode, node);

		parent = *new;
		if (offset + len <= this->offset)
			new = &((*new)->rb_left);
		else if (this->offset + this->len <= offset)
			new = &((*new)->rb_right);
		else
			return -1;
	}
	
    struct mynode* my_new = (struct mynode *) ALLOC(sizeof(struct mynode));
    my_new->offset = offset;
    my_new->len = len;
    my_new->data = (char *) ALLOC(len);
    my_new->h_entry = h_entry;
    memcpy(my_new->data, data, len);
    // add LRU entry to head of list
    list_add(&(my_new->lru_entry), &lru_list);

	/* Add new node and rebalance tree. */
	rb_link_node(&my_new->node, parent, new);
	rb_insert_color(&my_new->node, root);
	
    return 0;
}

static void limit_rcache_size() {
    if (rcache_size < rcache_limit) {
        return;
    }
    
    struct mynode *cur, *tmp;
    list_for_each_entry_safe_reverse(cur, tmp, &lru_list, lru_entry) {
        if (rcache_size < rcache_limit) {
            return;
        }
        
        rcache_size -= cur->len;
        // remove from lru_list
        list_del(&(cur->lru_entry));
        // remove from rbtree
        rb_erase(&(cur->node), &(cur->h_entry->root));

        FREE(cur->data, cur->len);
        FREE(cur, sizeof(struct mynode));
    }
}

void rcache_put(struct fingerprint *fpnt, struct data_entry *de) {
    struct hash_entry* he = hash_find(rcache, fpnt);
    
    
    if (he == NULL) {
        // new element in hash
        struct list_head* slot_list = &rcache[fp_slot(*fpnt)];
        he = (struct hash_entry *) ALLOC(sizeof(struct hash_entry));
        he->fpnt = *fpnt;
        he->root = RB_ROOT;
        list_add(&(he->entry), slot_list);
    }
    struct rb_root* rbroot = &(he->root);
    
    // find first overlap
    struct mynode* my = first_overlap(rbroot, de->offset, de->len);
    if (my == NULL) {
        // no overlap, just insert and quit
        rcache_insert_data(rbroot, de->offset, de->len, de->data, he);
        limit_rcache_size();
        return;
    }
    
    // split and insert
    offset_t offset = de->offset, len = de->len;
    while (len > 0) {
        
        my = first_overlap(rbroot, offset, len);
        if (my == NULL) {
            rcache_insert_data(rbroot, offset, len, de->data + (offset - de->offset), he);
            break;
        } else if (my->offset <= offset) {
            // case 1
            offset_t write_len = len; // the length of data written to overlapped segment
            if (offset + write_len > my->offset + my->len) {
                write_len = my->offset + my->len - offset;
            }
            
            // write to overlapped segment
            memcpy(my->data + (offset - my->offset), de->data + (offset - de->offset), write_len);
            
            offset += write_len;
            len -= write_len;
            rcache_size += write_len;
            // go on to next round
        } else {
            // offst < my->offset
            // case 2
            // insert non-overlapping part
            offset_t seg_len = my->offset - offset;
            rcache_insert_data(rbroot, offset, seg_len, de->data + (offset - de->offset), he);
            offset += seg_len;
            len -= seg_len;
            rcache_size += seg_len;
            // go on to next round, will be handled immediately by case 1
        }
    }
    
    limit_rcache_size();
}


struct data_set *wcache_read(struct fingerprint *fp, offset_t offset, offset_t len) {
    struct data_set* dset = NULL;
    struct hash_entry* he = hash_find(wcache, fp);

    if (he == NULL) {
        // nothing found, return NULL
        return NULL;
    }
    struct rb_root* rbroot = &(he->root);
    
    dset = (struct data_set *) ALLOC(sizeof(struct data_set));
    INIT_LIST_HEAD(&(dset->entries));
    
    struct mynode* my = first_overlap(rbroot, offset, len);
    while (my) {
        
        if (offset + len <= my->offset) {
            break;
        }
        
        struct data_entry *de = (struct data_entry *) ALLOC(sizeof(struct data_entry));
        de->data = my->data;
        de->offset = my->offset;
        de->len = my->len;
        list_add(&(de->entry), &(dset->entries));
        
        struct rb_node* next = rb_next(&(my->node));
        if (next == NULL) {
            break;
        }
        my = container_of(next, struct mynode, node);
    }
    
    return dset;
}



static int wcache_insert_data(struct rb_root *root, offset_t offset, offset_t len, char* data) {
    struct rb_node **new = &(root->rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new) {
		struct mynode *this = container_of(*new, struct mynode, node);

		parent = *new;
		if (offset + len <= this->offset)
			new = &((*new)->rb_left);
		else if (this->offset + this->len <= offset)
			new = &((*new)->rb_right);
		else
			return -1;
	}
	
    struct mynode* my_new = (struct mynode *) ALLOC(sizeof(struct mynode));
    my_new->offset = offset;
    my_new->len = len;
    my_new->data = (char *) ALLOC(len);
    memcpy(my_new->data, data, len);
    // lru_entry not set for this

	/* Add new node and rebalance tree. */
	rb_link_node(&my_new->node, parent, new);
	rb_insert_color(&my_new->node, root);
	
    return 0;
}


// Data input are SAFE to free by users after the function returns.
int wcache_write(struct fingerprint *fpnt, struct data_entry *de) {
    struct hash_entry* he = hash_find(wcache, fpnt);

    if (he == NULL) {
        // new element in hash
        struct list_head* slot_list = &wcache[fp_slot(*fpnt)];
        he = (struct hash_entry *) ALLOC(sizeof(struct hash_entry));
        he->fpnt = *fpnt;
        he->root = RB_ROOT;
        list_add(&(he->entry), slot_list);
    }
    struct rb_root* rbroot = &(he->root);
    
    // find first overlap
    struct mynode* my = first_overlap(rbroot, de->offset, de->len);
    if (my == NULL) {
        // no overlap, just insert and quit
        wcache_insert_data(rbroot, de->offset, de->len, de->data);
        return 0;
    }
    
    // split and insert
    offset_t offset = de->offset, len = de->len;
    while (len > 0) {
        
        my = first_overlap(rbroot, offset, len);
        if (my == NULL) {
            wcache_insert_data(rbroot, offset, len, de->data + (offset - de->offset));
            break;
        } else if (my->offset <= offset) {
            // case 1
            offset_t write_len = len; // the length of data written to overlapped segment
            if (offset + write_len > my->offset + my->len) {
                write_len = my->offset + my->len - offset;
            }
            
            // write to overlapped segment
            memcpy(my->data + (offset - my->offset), de->data + (offset - de->offset), write_len);
            
            offset += write_len;
            len -= write_len;
            // go on to next round
        } else {
            // offst < my->offset
            // case 2
            // insert non-overlapping part
            offset_t seg_len = my->offset - offset;
            wcache_insert_data(rbroot, offset, seg_len, de->data + (offset - de->offset));
            offset += seg_len;
            len -= seg_len;
            // go on to next round, will be handled immediately by case 1
        }
    }
    
    return 0;
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
                FREE(node->data, node->len);
                FREE(node, sizeof(struct mynode));
            }
            
            FREE(he, sizeof(struct hash_entry));
        }
    }
    
    // fini rcache
    for (i = 0; i < N_SLOT; i++) {
        struct list_head* slot_list = &rcache[i];
        // TODO free all the rbtrees in the list
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
                FREE(node->data, node->len);
                list_del(&(node->lru_entry)); // remove from lru
                FREE(node, sizeof(struct mynode));
            }
            
            FREE(he, sizeof(struct hash_entry));
        }
    }
}

