#include <iostream>
#include "rocksdb/utilities/optimistic_transaction_db.h"

int
main()
{
   rocksdb::Options options;
   rocksdb::DB     *db = nullptr;
   rocksdb::DB::Open(options, "splinterdb", &db);

   rocksdb::WriteOptions write_options;
   rocksdb::ReadOptions  read_options;

   rocksdb::Status s;

   // Insert a few kv-pairs, describing properties of fruits.
   s = db->Put(write_options, "apple", "An apple a day keeps the doctor away!");
   assert(s.ok());

   s = db->Put(write_options, "orange", "Is a good source of vitamin-C.");
   assert(s.ok());

   s = db->Put(write_options, "Mango", "Mango is the king of fruits.");
   assert(s.ok());

   std::string value = "If you saw me, something would be wrong.";
   s                 = db->Get(read_options, "orange", &value);
   assert(s.ok());
   assert(value == "Is a good source of vitamin-C.");

   s = db->Get(read_options, "banana", &value);
   assert(s.IsNotFound());

   db->Close();

   return 0;
}
