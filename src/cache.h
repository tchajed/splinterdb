// Copyright 2018-2021 VMware, Inc.
// SPDX-License-Identifier: Apache-2.0

/*
 * cache.h --
 *
 *     This file contains the abstract interface for a cache.
 */

#ifndef __CACHE_H
#define __CACHE_H

#include "platform.h"
#include "context.h"
#include "allocator.h"
#include "io.h"

typedef struct page_handle {
   char   *data;
   uint64  disk_addr;
   bool    persistent;
} page_handle;

typedef struct cache cache;


typedef enum page_type {
   PAGE_TYPE_TRUNK,
   PAGE_TYPE_BRANCH,
   PAGE_TYPE_MEMTABLE,
   PAGE_TYPE_FILTER,
   PAGE_TYPE_LOG,
   PAGE_TYPE_MISC,
   PAGE_TYPE_LOCK_NO_DATA,
   PAGE_TYPE_MEMTABLE_INTERNAL,
   NUM_PAGE_TYPES,
   PAGE_TYPE_INVALID,
} page_type;

typedef struct cache_stats {
   uint64 cache_migrates_to_PMEM[NUM_PAGE_TYPES];
   uint64 cache_migrates_to_DRAM_without_shadow[NUM_PAGE_TYPES];
   uint64 cache_migrates_to_DRAM_with_shadow[NUM_PAGE_TYPES];

   uint64 cache_evicts_to_PMEM[NUM_PAGE_TYPES];
   uint64 cache_evicts_to_disk[NUM_PAGE_TYPES];

   uint64 cache_hits[NUM_PAGE_TYPES];
   uint64 cache_misses[NUM_PAGE_TYPES];
   uint64 cache_miss_time_ns[NUM_PAGE_TYPES];
   uint64 page_allocs[NUM_PAGE_TYPES];
   uint64 page_deallocs[NUM_PAGE_TYPES];
   uint64 page_writes[NUM_PAGE_TYPES];
   uint64 page_reads[NUM_PAGE_TYPES];
   uint64 prefetches_issued[NUM_PAGE_TYPES];
   uint64 writes_issued;
   uint64 syncs_issued;
} PLATFORM_CACHELINE_ALIGNED cache_stats;

/*
 * By defining a maximum for pages_per_extent, we can easily avoid a bunch of
 * variable-length arrays.
 * Our default is 128k extents and 4k pages, and 32 (usually pointers) easily
 * fits on the stack so we can avoid mallocs in addition to VLAs.
 * If we need to increase this, if we make the stack too large we'll need to
 * either allocate some scratch in a larger buffer, or add a lot of mallocs.
 */
#define MAX_PAGES_PER_EXTENT 32lu

/*
 * The Maximum ref count on a single page that a thread is allowed to
 * have. The sum of all threads' ref counts is MAX_THREADS times this.
 * See cache_get_read_ref() below.
 */
#define MAX_READ_REFCOUNT    UINT16_MAX

// This is probably necessary:
_Static_assert(IS_POWER_OF_2(MAX_PAGES_PER_EXTENT),
               "MAX_PAGES_PER_EXTENT not a power of 2");

typedef enum {
   // Success without needing async IO because of cache hit.
   async_success = 0xc0ffee,
   /*
    * Locked it's write-locked, or raced with eviction or
    * another thread was loading the page. Caller needs to retry.
    */
   async_locked,
   // Retry or throttle ingress lookups because we're out of io reqs.
   async_no_reqs,
   // Started async IO and caller will be notified via callback.
   async_io_started
} cache_async_result;

struct cache_async_ctxt;
typedef void (*cache_async_cb)(struct cache_async_ctxt *ctxt);

// User can embed this within an user-specific context
typedef struct cache_async_ctxt {
   cache               *cc;           // IN cache
   cache_async_cb       cb;           // IN callback for async_io_started
   void                *cbdata;       // IN opaque callback data
   platform_status      status;       // IN status of async IO
   page_handle         *page;         // OUT page handle
   // Internal stats
   struct {
      timestamp         issue_ts;     // issue time
      timestamp         compl_ts;     // completion time
   } stats;
} cache_async_ctxt;

typedef page_handle *(*page_alloc_fn)(cache *cc, uint64 addr, page_type type);
typedef bool (*page_dealloc_fn)(cache *cc, uint64 addr, page_type type);
typedef uint8 (*page_get_ref_fn)(cache *cc, uint64 addr);
typedef page_handle *(*page_get_fn)(cache *   cc,
                                    uint64    addr,
                                    bool      blocking,
                                    page_type type);
typedef cache_async_result (*page_get_async_fn)(cache *           cc,
                                                uint64            addr,
                                                page_type         type,
                                                cache_async_ctxt *ctxt);
typedef void (*page_async_done_fn)(cache *           cc,
                                   page_type         type,
                                   cache_async_ctxt *ctxt);
typedef void (*page_unget_fn)(cache *cc, page_handle *page);
typedef bool (*page_claim_fn)(cache *cc, page_handle *page);
typedef void (*page_unclaim_fn)(cache *cc, page_handle *page);
typedef void (*page_lock_fn)(cache *cc, page_handle **page);
typedef void (*page_unlock_fn)(cache *cc, page_handle **page);
typedef void (*page_pin_fn)(cache *cc, page_handle *page);
typedef void (*page_unpin_fn)(cache *cc, page_handle *page);
typedef void (*page_sync_fn)(cache *      cc,
                             page_handle *page,
                             bool         is_blocking,
                             page_type    type);
typedef void (*page_prefetch_fn)(cache *cc, uint64 addr, page_type type);
typedef void (*page_mark_dirty_fn)(cache *cc, page_handle *page);
typedef void (*flush_fn)(cache *cc);
typedef int (*evict_fn)(cache *cc, bool ignore_pinned);
typedef void (*cleanup_fn)(cache *cc);
typedef uint64 (*get_cache_size_fn)(cache *cc);
typedef void (*assert_ungot_fn)(cache *cc, uint64 addr);
typedef void (*assert_free_fn)(cache *cc);
typedef void (*assert_noleaks)(cache *cc);
typedef bool (*page_valid_fn)(cache *cc, uint64 addr);
typedef void (*validate_page_fn)(cache *cc, page_handle *page, uint64 addr);
typedef void (*print_fn)(cache *cc);
typedef void (*reset_stats_fn)(cache *cc);
typedef void (*io_stats_fn)(cache *cc, uint64 *read_bytes, uint64 *write_bytes);
typedef uint32 (*count_dirty_fn)(cache *cc);
typedef uint32 (*page_get_read_ref_fn)(cache *cc, page_handle *page);
typedef bool (*cache_present_fn)(cache *cc, page_handle *page);
typedef void (*enable_sync_get_fn)(cache *cc, bool enabled);

typedef allocator *(*cache_allocator_fn)(cache *cc);

typedef ThreadContext* (*cache_get_context_fn) (cache *cc);

typedef cache * (*cache_get_volatile_cache_fn)(cache *cc);
typedef bool (*cache_if_volatile_page_fn)(cache *cc, page_handle *page);
typedef bool (*cache_if_volatile_addr_fn)(cache *cc, uint64 addr);
typedef bool (*cache_if_diskaddr_in_volatile_cache_fn)(cache *cc, uint64 disk_addr);

typedef cache * (*cache_get_addr_cache_fn)(cache *cc, uint64 addr);

typedef struct cache_ops {
   page_alloc_fn        page_alloc;
   page_dealloc_fn      page_dealloc;
   page_get_ref_fn      page_get_ref;
   page_get_fn          page_get;
   page_get_async_fn    page_get_async;
   page_async_done_fn   page_async_done;
   page_unget_fn        page_unget;
   page_claim_fn        page_claim;
   page_unclaim_fn      page_unclaim;
   page_lock_fn         page_lock;
   page_unlock_fn       page_unlock;
   page_prefetch_fn     page_prefetch;
   page_mark_dirty_fn   page_mark_dirty;
   page_pin_fn          page_pin;
   page_unpin_fn        page_unpin;
   page_sync_fn         page_sync;
   flush_fn             flush;
   evict_fn             evict;
   cleanup_fn           cleanup;
   get_cache_size_fn    get_page_size;
   get_cache_size_fn    get_extent_size;
   assert_ungot_fn      assert_ungot;
   assert_free_fn       assert_free;
   assert_noleaks       assert_noleaks;
   print_fn             print;
   print_fn             print_stats;
   io_stats_fn          io_stats;
   reset_stats_fn       reset_stats;
   page_valid_fn        page_valid;
   validate_page_fn     validate_page;
   count_dirty_fn       count_dirty;
   page_get_read_ref_fn page_get_read_ref;
   cache_present_fn     cache_present;
   enable_sync_get_fn   enable_sync_get;
   cache_allocator_fn   cache_allocator;
   cache_get_context_fn cache_get_context;
   cache_get_volatile_cache_fn cache_get_volatile_cache;
   cache_if_volatile_page_fn   cache_if_volatile_page;
   cache_if_volatile_addr_fn   cache_if_volatile_addr;
   cache_if_diskaddr_in_volatile_cache_fn cache_if_diskaddr_in_volatile_cache;
   cache_get_addr_cache_fn     cache_get_addr_cache;
} cache_ops;

// To sub-class cache, make a cache your first field;
struct cache {
   const cache_ops *ops;
};

static inline cache *
cache_get_volatile_cache(cache *cc)
{
  return cc->ops->cache_get_volatile_cache(cc);
}


static inline cache*
cache_get_addr_cache(cache *cc, uint64 addr)
{
  return cc->ops->cache_get_addr_cache(cc, addr);
}

static inline bool
cache_if_volatile_page(cache *cc, page_handle *page)
{
  return cc->ops->cache_if_volatile_page(cc, page);
}


static inline bool
cache_if_volatile_addr(cache *cc, uint64 addr)
{
  return cc->ops->cache_if_volatile_addr(cc, addr);
}

static inline bool
cache_if_diskaddr_in_volatile_cache(cache *cc, uint64 disk_addr)
{
  return cc->ops->cache_if_diskaddr_in_volatile_cache(cc, disk_addr);
}

//TODO: cache alloc cc type can only be decided by user
static inline page_handle *
cache_alloc(cache *cc, uint64 addr, page_type type)
{
   page_handle* page = cc->ops->page_alloc(cc, addr, type);

   /*
   if(cache_if_diskaddr_in_volatile_cache(cc, addr))
      assert(cache_get_volatile_cache(cc) == NULL);
   else
      assert(cache_get_volatile_cache(cc) != NULL);
   */

   //assert(cache_get_addr_cache(cc, addr) == cc);

   return page;
}

static inline bool
cache_dealloc(cache *cc, uint64 addr, page_type type)
{
   if(cache_if_diskaddr_in_volatile_cache(cc, addr))
   {
      cache *vcc = cache_get_volatile_cache(cc);
      return cc->ops->page_dealloc(vcc, addr, type);
   }
   return cc->ops->page_dealloc(cc, addr, type);
   /*
   cache *rcc = cache_get_addr_cache(cc, addr);
   return rcc->ops->page_dealloc(rcc, addr, type);
   */
}

static inline uint8
cache_get_ref(cache *cc, uint64 addr)
{
   if(cache_if_diskaddr_in_volatile_cache(cc, addr))
   {
      cache *vcc = cache_get_volatile_cache(cc);
      return cc->ops->page_get_ref(vcc, addr);
   }
   return cc->ops->page_get_ref(cc, addr);
}

static inline page_handle *
cache_get(cache *cc, uint64 addr, bool blocking, page_type type)
{
   if(cache_if_diskaddr_in_volatile_cache(cc, addr))
   {
      cache *vcc = cache_get_volatile_cache(cc);
      return cc->ops->page_get(vcc, addr, blocking, type);
   }

   return cc->ops->page_get(cc, addr, blocking, type);
}

//TODO: Find out how to decide cc type in this init
static inline void
cache_ctxt_init(cache * cc, cache_async_cb cb, void *cbdata, cache_async_ctxt * ctxt)
{
   ctxt->cc = cc;
   ctxt->cb = cb;
   ctxt->cbdata = cbdata;
   ctxt->page = NULL;
}


static inline cache_async_result
cache_get_async(cache *cc, uint64 addr, page_type type,
                cache_async_ctxt *ctxt)
{
   if(cache_if_diskaddr_in_volatile_cache(cc, addr))
   {
      cache *vcc = cache_get_volatile_cache(cc);
      ctxt->cc = vcc;
      return cc->ops->page_get_async(vcc, addr, type, ctxt);
   }

   return cc->ops->page_get_async(cc, addr, type, ctxt);
}

//TODO: Figure out the cache type
static inline void
cache_async_done(cache *cc, page_type type, cache_async_ctxt *ctxt)
{
   return cc->ops->page_async_done(cc, type, ctxt);
}

static inline void
cache_unget(cache *cc, page_handle *page)
{
   if(cache_if_volatile_page(cc, page))
   {
      cache *vcc = cache_get_volatile_cache(cc);
      return cc->ops->page_unget(vcc, page);
   }

   return cc->ops->page_unget(cc, page);
}

static inline bool
cache_claim(cache *cc, page_handle *page)
{
   if(cache_if_volatile_page(cc, page))
   {
      cache *vcc = cache_get_volatile_cache(cc);
      return cc->ops->page_claim(vcc, page);
   }

   return cc->ops->page_claim(cc, page);
}

static inline void
cache_unclaim(cache *cc, page_handle *page)
{
   if(cache_if_volatile_page(cc, page))
   {
      cache *vcc = cache_get_volatile_cache(cc);
      return cc->ops->page_unclaim(vcc, page);
   }

   return cc->ops->page_unclaim(cc, page);
}

static inline void
cache_lock(cache *cc, page_handle **page)
{
   if(cache_if_volatile_page(cc, *page))
   {
      cache *vcc = cache_get_volatile_cache(cc);
      return cc->ops->page_lock(vcc, page);
   }

   return cc->ops->page_lock(cc, page);
}

static inline void
cache_unlock(cache *cc, page_handle **page)
{
   if(cache_if_volatile_page(cc, *page))
   {
      cache *vcc = cache_get_volatile_cache(cc);
      return cc->ops->page_unlock(vcc, page);
   }

   return cc->ops->page_unlock(cc, page);
}

//TODO: By default, prefetching into PMEM
// The addr is the to-be-fetched addr
static inline void
cache_prefetch(cache *cc, uint64 addr, page_type type)
{
   //cache *vcc = cache_get_volatile_cache(cc);
   //return cc->ops->page_prefetch(vcc, addr, type);
   return cc->ops->page_prefetch(cc, addr, type);
}

static inline void
cache_mark_dirty(cache *cc, page_handle *page)
{
   if(cache_if_volatile_page(cc, page))
   {
      cache *vcc = cache_get_volatile_cache(cc);
      return cc->ops->page_mark_dirty(vcc, page);
   }

   return cc->ops->page_mark_dirty(cc, page);
}

static inline void
cache_pin(cache *cc, page_handle *page)
{
   if(cache_if_volatile_page(cc, page))
   {
      cache *vcc = cache_get_volatile_cache(cc);
      return cc->ops->page_pin(vcc, page);
   }

   return cc->ops->page_pin(cc, page);
}

static inline void
cache_unpin(cache *cc, page_handle *page)
{
   if(cache_if_volatile_page(cc, page))
   {
      cache *vcc = cache_get_volatile_cache(cc);
      return cc->ops->page_unpin(vcc, page);
   }

   return cc->ops->page_unpin(cc, page);
}

static inline void
cache_page_sync(cache *cc, page_handle *page, bool is_blocking, page_type type)
{
   if(cache_if_volatile_page(cc, page))
   {
      cache *vcc = cache_get_volatile_cache(cc);
      return cc->ops->page_sync(vcc, page, is_blocking, type);
   }

   return cc->ops->page_sync(cc, page, is_blocking, type);
}

static inline void
cache_flush(cache *cc)
{
   cache *vcc = cache_get_volatile_cache(cc);
   if (vcc) {
     cc->ops->flush(vcc);
   }

   cc->ops->flush(cc);
}

static inline int
cache_evict(cache *cc, bool ignore_pinned_pages)
{
   cache *vcc = cache_get_volatile_cache(cc);
   if (vcc) {
     cc->ops->evict(vcc, ignore_pinned_pages);
   }

   return cc->ops->evict(cc, ignore_pinned_pages);
}

static inline void
cache_cleanup(cache *cc)
{
   cache *vcc = cache_get_volatile_cache(cc);
   if (vcc) {
     cc->ops->cleanup(vcc);
   }

   return cc->ops->cleanup(cc);
}

//TODO: assuming the below functions return same value for both caches
static inline uint64
cache_page_size(cache *cc)
{
   return cc->ops->get_page_size(cc);
}

static inline uint64
cache_extent_size(cache *cc)
{
   return cc->ops->get_extent_size(cc);
}

static inline void
cache_assert_ungot(cache *cc, uint64 addr)
{
   if(cache_if_diskaddr_in_volatile_cache(cc, addr))
   {
      cache *vcc = cache_get_volatile_cache(cc);
      return cc->ops->assert_ungot(vcc, addr);
   }

   return cc->ops->assert_ungot(cc, addr);
}

static inline void
cache_assert_free(cache *cc)
{
   cache *vcc = cache_get_volatile_cache(cc);
   if (vcc) {
     cc->ops->assert_free(vcc);
   }

   return cc->ops->assert_free(cc);
}

static inline void
cache_assert_noleaks(cache *cc)
{
   cache *vcc = cache_get_volatile_cache(cc);
   if (vcc) {
     cc->ops->assert_noleaks(vcc);
   }

   return cc->ops->assert_noleaks(cc);
}

static inline void
cache_print(cache *cc)
{
   cache *vcc = cache_get_volatile_cache(cc);
   if (vcc) {
     platform_log("--------------PRINTING VOLATILE CACHE--------------\n");
     cc->ops->print(vcc);
   }
   platform_log("--------------PRINTING PERSISTENT CACHE--------------\n");
   return cc->ops->print(cc);
}

static inline void
cache_print_stats(cache *cc)
{
   cache *vcc = cache_get_volatile_cache(cc);
   if (vcc) {
     platform_log("--------------PRINTING VOLATILE STATS--------------\n");
     cc->ops->print_stats(vcc);
   }
   platform_log("--------------PRINTING PERSISTENT CACHE--------------\n");
   return cc->ops->print_stats(cc);
}

static inline void
cache_reset_stats(cache *cc)
{
   cache *vcc = cache_get_volatile_cache(cc);
   if (vcc) {
      cc->ops->reset_stats(vcc);
   }
   return cc->ops->reset_stats(cc);
}

static inline void
cache_io_stats(cache *cc, uint64 *read_bytes, uint64 *write_bytes)
{
   cache *vcc = cache_get_volatile_cache(cc);
   if (vcc) {
      cc->ops->io_stats(vcc, read_bytes, write_bytes);
   }
   return cc->ops->io_stats(cc, read_bytes, write_bytes);
}

static inline bool
cache_page_valid(cache *cc, uint64 addr)
{
   if(cache_if_diskaddr_in_volatile_cache(cc, addr))
   {
      cache *vcc = cache_get_volatile_cache(cc);
      return cc->ops->page_valid(vcc, addr);
   }

   return cc->ops->page_valid(cc, addr);
}

static inline void
cache_validate_page(cache *cc, page_handle *page, uint64 addr)
{
   assert(page->disk_addr == addr);
   if(cache_if_volatile_page(cc, page)){
      assert(cache_if_diskaddr_in_volatile_cache(cc, addr));
      cache *vcc = cache_get_volatile_cache(cc);
      cc->ops->validate_page(vcc, page, addr);
   }
   else{
      assert(!cache_if_diskaddr_in_volatile_cache(cc, addr));
      cc->ops->validate_page(cc, page, addr);
   }
}

//FIXME: Find out where this is called
// Seems only used on cache test
static inline uint32
cache_count_dirty(cache *cc)
{
   return cc->ops->count_dirty(cc);
}


static inline uint32
cache_get_read_ref(cache *cc, page_handle *page)
{
   if(cache_if_volatile_page(cc, page))
   {
      cache *vcc = cache_get_volatile_cache(cc);
      return cc->ops->page_get_read_ref(vcc, page);
   }

   return cc->ops->page_get_read_ref(cc, page);
}

static inline bool
cache_present(cache *cc, page_handle *page)
{
   if(cache_if_volatile_page(cc, page))
   {
      cache *vcc = cache_get_volatile_cache(cc);
      return cc->ops->cache_present(vcc, page);
   }

   return cc->ops->cache_present(cc, page);
}

//FIXME: Find out where this is called
// Seems only used for debug
static inline void
cache_enable_sync_get(cache *cc, bool enabled)
{
   cc->ops->enable_sync_get(cc, enabled);
}

// Two caches share the same allocator
static inline allocator *
cache_allocator(cache *cc)
{
   return cc->ops->cache_allocator(cc);
}

static inline ThreadContext *
cache_get_context(cache *cc)
{
   return cc->ops->cache_get_context(cc);
}



#endif // __CACHE_H
