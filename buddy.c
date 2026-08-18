#include "buddy.h"
#include <stddef.h>

#define MAX_RANK 16
#define MAX_PAGES (1 << 20)

typedef struct block {
    struct block *next;
} block_t;

static block_t *free_lists[MAX_RANK + 1];
static unsigned char page_ranks[MAX_PAGES]; 
static unsigned char page_is_free[MAX_PAGES];
static void *pool_start;
static int total_pages;

static int get_page_idx(void *p) {
    return (int)((unsigned char *)p - (unsigned char *)pool_start) / 4096;
}

static void *get_addr(int idx) {
    return (void *)((unsigned char *)pool_start + (idx * 4096));
}

int init_page(void *p, int pgcount) {
    pool_start = p;
    total_pages = pgcount;
    for (int i = 0; i <= MAX_RANK; i++) free_lists[i] = NULL;
    for (int i = 0; i < MAX_PAGES; i++) {
        page_ranks[i] = 0;
        page_is_free[i] = 0;
    }

    int current_page = 0;
    while (current_page < total_pages) {
        int rank = MAX_RANK;
        while (rank >= 1) {
            if (current_page % (1 << (rank - 1)) == 0 && 
                current_page + (1 << (rank - 1)) <= total_pages) {
                break;
            }
            rank--;
        }
        
        block_t *b = (block_t *)get_addr(current_page);
        b->next = free_lists[rank];
        free_lists[rank] = b;
        page_ranks[current_page] = rank;
        page_is_free[current_page] = 1;
        
        current_page += (1 << (rank - 1));
    }
    return OK;
}

void *alloc_pages(int rank) {
    if (rank < 1 || rank > MAX_RANK) return ERR_PTR(-EINVAL);
    
    int current_rank = rank;
    while (current_rank <= MAX_RANK && free_lists[current_rank] == NULL) {
        current_rank++;
    }
    
    if (current_rank > MAX_RANK) return ERR_PTR(-ENOSPC);
    
    block_t *b = free_lists[current_rank];
    free_lists[current_rank] = b->next;
    int idx = get_page_idx(b);
    page_is_free[idx] = 0;
    
    while (current_rank > rank) {
        current_rank--;
        int buddy_idx = idx + (1 << (current_rank - 1));
        block_t *buddy = (block_t *)get_addr(buddy_idx);
        buddy->next = free_lists[current_rank];
        free_lists[current_rank] = buddy;
        page_ranks[buddy_idx] = current_rank;
        page_is_free[buddy_idx] = 1;
    }
    
    page_ranks[idx] = rank;
    return b;
}

int return_pages(void *p) {
    if (p == NULL) return -EINVAL;
    int idx = get_page_idx(p);
    if (idx < 0 || idx >= total_pages || page_ranks[idx] == 0 || page_is_free[idx]) return -EINVAL;
    
    int rank = page_ranks[idx];
    
    while (rank < MAX_RANK) {
        int buddy_idx = idx ^ (1 << (rank - 1));
        if (buddy_idx < 0 || buddy_idx >= total_pages || 
            page_ranks[buddy_idx] != rank || !page_is_free[buddy_idx]) {
            break;
        }
        
        block_t **prev = &free_lists[rank];
        while (*prev && get_page_idx(*prev) != buddy_idx) {
            prev = &(*prev)->next;
        }
        if (*prev) {
            *prev = (*prev)->next;
        } else {
            break; 
        }
        
        page_is_free[buddy_idx] = 0;
        page_ranks[buddy_idx] = 0;
        idx = idx & ~(1 << (rank - 1));
        rank++;
    }
    
    block_t *b = (block_t *)get_addr(idx);
    b->next = free_lists[rank];
    free_lists[rank] = b;
    page_ranks[idx] = rank;
    page_is_free[idx] = 1;
    
    return OK;
}

int query_ranks(void *p) {
    if (p == NULL) return -EINVAL;
    int idx = get_page_idx(p);
    if (idx < 0 || idx >= total_pages) return -EINVAL;
    
    for (int r = MAX_RANK; r >= 1; r--) {
        int size = (1 << (r - 1));
        int start_idx = idx & ~(size - 1);
        if (start_idx < 0 || start_idx >= total_pages) continue;
        if (page_ranks[start_idx] == r) {
            return r;
        }
    }
    return -EINVAL;
}

int query_page_counts(int rank) {
    if (rank < 1 || rank > MAX_RANK) return -EINVAL;
    int count = 0;
    block_t *curr = free_lists[rank];
    while (curr) {
        count++;
        curr = curr->next;
    }
    return count;
}
