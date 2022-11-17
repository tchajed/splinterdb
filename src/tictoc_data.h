#ifndef _TICTOC_DATA_H_
#define _TICTOC_DATA_H_

#include "splinterdb/data.h"
#include "splinterdb/public_platform.h"
#include "splinterdb/public_util.h"
#include "lock_table.h"
#include "util.h"

typedef uint32 tictoc_timestamp;

typedef struct ONDISK tictoc_timestamp_set {
   tictoc_timestamp rts;
   tictoc_timestamp wts;
} tictoc_timestamp_set;

inline bool
ts_set_is_nonzero(tictoc_timestamp_set ts_set)
{
   return ts_set.rts == 0 && ts_set.wts == 0;
}

extern tictoc_timestamp_set ZERO_TICTOC_TIMESTAMP_SET;

typedef struct ONDISK tictoc_tuple {
   tictoc_timestamp_set ts_set;
   // char absent; // to indicate whether this tuple is deleted or not
   char value[]; // value provided by application
} tictoc_tuple;

inline uint64
tictoc_tuple_header_size()
{
   return sizeof(tictoc_timestamp_set);
}

// read_set and write_set entry stored locally
typedef struct entry {
   message_type    op;
   slice           key;
   writable_buffer tuple;
   void           *latch;
   bool            written;
} entry;

tictoc_timestamp_set
get_ts_from_entry(entry *entry);

bool
entry_is_invalid(entry *entry);

#define SET_SIZE_LIMIT 1024

typedef struct tictoc_transaction {
   entry  entries[2 * SET_SIZE_LIMIT];
   entry *read_set;
   entry *write_set;
   uint64 read_cnt;
   uint64 write_cnt;
   uint64 commit_ts;
} tictoc_transaction;

bool
tictoc_entry_is_not_in_write_set(tictoc_transaction *tt_txn, entry *e);

void
tictoc_transaction_init(tictoc_transaction *tt_txn);

entry *
tictoc_get_new_read_set_entry(tictoc_transaction *tt_txn);

entry *
tictoc_get_read_set_entry(tictoc_transaction *tt_txn, uint64 i);

entry *
tictoc_get_new_write_set_entry(tictoc_transaction *tt_txn);

entry *
tictoc_get_write_set_entry(tictoc_transaction *tt_txn, uint64 i);

void
tictoc_transaction_sort_write_set(tictoc_transaction *tt_txn);

void
tictoc_transaction_lock_all_write_set(tictoc_transaction *tt_txn,
                                      lock_table         *lock_tbl);

void
tictoc_transaction_unlock_all_write_set(tictoc_transaction *tt_txn,
                                        lock_table         *lock_tbl);

#endif // _TICTOC_DATA_H_
