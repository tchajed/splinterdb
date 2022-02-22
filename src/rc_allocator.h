// Copyright 2018-2021 VMware, Inc.
// SPDX-License-Identifier: Apache-2.0

/*
 * rc_allocator.h --
 *
 * This file contains the interface for the ref count allocator.
 */

#ifndef __RC_ALLOCATOR_H
#define __RC_ALLOCATOR_H

#include "allocator.h"
#include "platform.h"
#include "util.h"

/*
 * In the current system, every Splinter instance has a superblock, one
 * for each table that is mapped to the Splinter instance. This limit
 * is the max number of superblocks (special pages) that can be accessed.
 * All of these superblocks are required to be on the 1st extent.
 */
#define RC_ALLOCATOR_MAX_ROOT_IDS (30)

/*
 * Configuration structure to set up the Ref Count Allocation sub-system.
 */
typedef struct rc_allocator_config {
   io_config *io_cfg;
   uint64     capacity;
   uint64     page_capacity;
   uint64     extent_capacity;
} rc_allocator_config;

/*
 *----------------------------------------------------------------------
 * rc_allocator_meta_page -- Disk-resident structure.
 *
 *  An on disk structure to hold the super block information about all the
 *  Splinter tables using this allocator. This is persisted at
 *  offset 0 of the device.
 *----------------------------------------------------------------------
 */
typedef struct ONDISK rc_allocator_meta_page {
   allocator_root_id splinters[RC_ALLOCATOR_MAX_ROOT_IDS];
   checksum128       checksum;
} rc_allocator_meta_page;

_Static_assert(offsetof(rc_allocator_meta_page, splinters) == 0,
               "splinters array should be first field in meta_page struct");

/*
 *----------------------------------------------------------------------
 * rc_allocator_stats --
 *----------------------------------------------------------------------
 */
typedef struct rc_allocator_stats {
   int64 curr_allocated;
   int64 max_allocated;
   int64 extent_allocs[NUM_PAGE_TYPES];
   int64 extent_deallocs[NUM_PAGE_TYPES];
} rc_allocator_stats;

/*
 *----------------------------------------------------------------------
 * rc_allocator -- Ref Count allocator context structure.
 *----------------------------------------------------------------------
 */
typedef struct rc_allocator {
   allocator               super;
   rc_allocator_config    *cfg;
   buffer_handle          *bh;
   uint8                  *ref_count;
   uint64                  hand;
   io_handle              *io;
   rc_allocator_meta_page *meta_page;

   /*
    * mutex to synchronize updates to super block addresses of the splinter
    * tables in the meta page.
    */
   platform_mutex       lock;
   platform_heap_handle heap_handle;
   platform_heap_id     heap_id;

   // Stats -- not distributed for now
   rc_allocator_stats stats;
} rc_allocator;


void
rc_allocator_config_init(rc_allocator_config *allocator_cfg,
                         io_config           *io_cfg,
                         uint64               capacity);

platform_status
rc_allocator_init(rc_allocator        *al,
                  rc_allocator_config *cfg,
                  io_handle           *io,
                  platform_heap_handle hh,
                  platform_heap_id     hid,
                  platform_module_id   mid);

void
rc_allocator_deinit(rc_allocator *al);

platform_status
rc_allocator_mount(rc_allocator        *al,
                   rc_allocator_config *cfg,
                   io_handle           *io,
                   platform_heap_handle hh,
                   platform_heap_id     hid,
                   platform_module_id   mid);

void
rc_allocator_dismount(rc_allocator *al);

void
rc_allocator_print_stats_to_stream(rc_allocator          *al,
                                   platform_stream_handle stream);

void
rc_allocator_debug_print_to_stream(rc_allocator          *al,
                                   platform_stream_handle stream);

#endif /* __RC_ALLOCATOR_H */
