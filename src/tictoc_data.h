#ifndef _TICTOC_DATA_H_
#define _TICTOC_DATA_H_

#include "splinterdb/data.h"
#include "splinterdb/public_platform.h"
#include "splinterdb/public_util.h"
#include "splinterdb/transaction.h"
#include "lock_table.h"
#include "util.h"

typedef uint32 tictoc_timestamp;

typedef struct ONDISK tictoc_timestamp_set {
   tictoc_timestamp rts;
   tictoc_timestamp wts;
} tictoc_timestamp_set;

extern tictoc_timestamp_set ZERO_TICTOC_TIMESTAMP_SET;

typedef struct ONDISK tictoc_tuple_header {
   tictoc_timestamp_set ts_set;
   char                 value[]; // value provided by application
} tictoc_tuple_header;

// read_set and write_set entry stored locally
typedef struct tictoc_rw_entry {
   message_type    op;
   writable_buffer key;
   writable_buffer tuple;
   range_lock      rng_lock;
} tictoc_rw_entry;

tictoc_timestamp_set
get_ts_from_tictoc_rw_entry(tictoc_rw_entry *entry);

bool
tictoc_rw_entry_is_invalid(tictoc_rw_entry *entry);

bool
tictoc_rw_entry_is_not_in_write_set(tictoc_transaction *tt_txn,
                                    tictoc_rw_entry    *entry,
                                    const data_config  *cfg);

void
tictoc_transaction_init(tictoc_transaction *tt_txn);

void
tictoc_transaction_deinit(tictoc_transaction *tt_txn, lock_table *lock_tbl);

tictoc_rw_entry *
tictoc_get_new_read_set_entry(tictoc_transaction *tt_txn);

tictoc_rw_entry *
tictoc_get_read_set_entry(tictoc_transaction *tt_txn, uint64 i);

void
tictoc_delete_last_read_set_entry(tictoc_transaction *tt_txn);

tictoc_rw_entry *
tictoc_get_new_write_set_entry(tictoc_transaction *tt_txn);

tictoc_rw_entry *
tictoc_get_write_set_entry(tictoc_transaction *tt_txn, uint64 i);

void
tictoc_transaction_sort_write_set(tictoc_transaction *tt_txn,
                                  const data_config  *cfg);

bool
tictoc_transaction_lock_all_write_set(tictoc_transaction *tt_txn,
                                      lock_table         *lock_tbl);

void
tictoc_transaction_unlock_all_write_set(tictoc_transaction *tt_txn,
                                        lock_table         *lock_tbl);

#endif // _TICTOC_DATA_H_
