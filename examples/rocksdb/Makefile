CFLAGS=-DSPLINTERDB_PLATFORM_DIR=platform_linux

LDFLAGS=-lsplinterdb -lrocksdb

all:
	$(CC) $(CFLAGS) optimistic_transaction.cc  optimistic_transaction_db_impl.cc  optimistic_transaction_rocksdb.cc $(LDFLAGS)

.PHONY: clean

clean:
	rm -f a.out splinter_rocks_db
