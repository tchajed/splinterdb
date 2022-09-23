#include <iostream>


#include <rocksdb/db.h>

extern "C" {
#include "splinterdb/default_data_config.h"
#include "splinterdb/splinterdb.h"
}

#define DB_FILE_NAME      "splinter_rocks_db"
#define DB_FILE_SIZE_MB   1024 // Size of SplinterDB device; Fixed when created
#define CACHE_SIZE_MB     64   // Size of cache; can be changed across boots
#define USER_MAX_KEY_SIZE 100

class SplinterRocksDB : public rocksdb::DB {
public:
   SplinterRocksDB() : rocksdb::DB()
   {
      default_data_config_init(USER_MAX_KEY_SIZE, &spl_data_cfg_);

      // Basic configuration of a SplinterDB instance
      memset(&spl_cfg_, 0, sizeof(spl_cfg_));
      spl_cfg_.filename   = DB_FILE_NAME;
      spl_cfg_.disk_size  = (DB_FILE_SIZE_MB * 1024 * 1024);
      spl_cfg_.cache_size = (CACHE_SIZE_MB * 1024 * 1024);
      spl_cfg_.data_cfg   = &spl_data_cfg_;

      int rc = splinterdb_create(&spl_cfg_, &spl_handle_);
      assert(!rc);
      // FIXME: handle errors
   }

   rocksdb::Status
   Put(const rocksdb::WriteOptions &options,
       rocksdb::ColumnFamilyHandle *column_family,
       const rocksdb::Slice        &key,
       const rocksdb::Slice        &value) override
   {
      slice spl_key   = slice_create(key.size(), key.data());
      slice spl_value = slice_create(value.size(), value.data());

      int rc = splinterdb_insert(spl_handle_, spl_key, spl_value);
      assert(!rc);

      return rocksdb::Status::OK();
   }

   rocksdb::Status
   Put(const rocksdb::WriteOptions &options,
       rocksdb::ColumnFamilyHandle *column_family,
       const rocksdb::Slice        &key,
       const rocksdb::Slice        &ts,
       const rocksdb::Slice        &value) override
   {
      (void)ts;
      return Put(options, column_family, key, value);
   }

   rocksdb::Status
   Delete(const rocksdb::WriteOptions &options,
          rocksdb::ColumnFamilyHandle *column_family,
          const rocksdb::Slice        &key) override
   {
      slice spl_key = slice_create(key.size(), key.data());

      int rc = splinterdb_delete(spl_handle_, spl_key);
      assert(!rc);

      return rocksdb::Status::OK();
   }

   rocksdb::Status
   Delete(const rocksdb::WriteOptions &options,
          rocksdb::ColumnFamilyHandle *column_family,
          const rocksdb::Slice        &key,
          const rocksdb::Slice        &ts) override
   {
      (void)ts;
      return Delete(options, column_family, key);
   }

   rocksdb::Status
   SingleDelete(const rocksdb::WriteOptions &options,
                rocksdb::ColumnFamilyHandle *column_family,
                const rocksdb::Slice        &key) override
   {
      return Delete(options, column_family, key);
   }

   rocksdb::Status
   SingleDelete(const rocksdb::WriteOptions &options,
                rocksdb::ColumnFamilyHandle *column_family,
                const rocksdb::Slice        &key,
                const rocksdb::Slice        &ts) override
   {
      (void)ts;
      return SingleDelete(options, column_family, key);
   }

   rocksdb::Status
   Merge(const rocksdb::WriteOptions &options,
         rocksdb::ColumnFamilyHandle *column_family,
         const rocksdb::Slice        &key,
         const rocksdb::Slice        &value) override
   {
      return rocksdb::Status::NotSupported(
         "Merge is supported by SplinterDB yet");
   }

   rocksdb::Status
   Write(const rocksdb::WriteOptions &options,
         rocksdb::WriteBatch         *updates) override
   {
      return rocksdb::Status::NotSupported(
         "Write is supported by SplinterDB yet");
   }

   rocksdb::Status
   Get(const rocksdb::ReadOptions  &options,
       rocksdb::ColumnFamilyHandle *column_family,
       const rocksdb::Slice        &key,
       rocksdb::PinnableSlice      *value) override
   {
      splinterdb_lookup_result result;
      splinterdb_lookup_result_init(spl_handle_, &result, 0, NULL);

      slice spl_key = slice_create(key.size(), key.data());
      int   rc      = splinterdb_lookup(spl_handle_, spl_key, &result);
      assert(!rc);

      if (!splinterdb_lookup_found(&result)) {
         splinterdb_lookup_result_deinit(&result);
         return rocksdb::Status::NotFound();
      }

      slice spl_value = slice_create(value->size(), value->data());
      rc              = splinterdb_lookup_result_value(&result, &spl_value);
      assert(!rc);
      rocksdb::Slice result_slice((const char *)slice_data(spl_value),
                                  slice_length(spl_value));
      value->PinSelf(result_slice);

      splinterdb_lookup_result_deinit(&result);

      return rocksdb::Status::OK();
   }

   rocksdb::Status
   GetMergeOperands(
      const rocksdb::ReadOptions       &options,
      rocksdb::ColumnFamilyHandle      *column_family,
      const rocksdb::Slice             &key,
      rocksdb::PinnableSlice           *value,
      rocksdb::GetMergeOperandsOptions *get_merge_operands_options,
      int                              *number_of_operands) override
   {
      return rocksdb::Status::NotSupported(
         "GetMergeOperands is supported by SplinterDB yet");
   }

   std::vector<rocksdb::Status>
   MultiGet(const rocksdb::ReadOptions                       &options,
            const std::vector<rocksdb::ColumnFamilyHandle *> &column_family,
            const std::vector<rocksdb::Slice>                &keys,
            std::vector<std::string>                         *values) override
   {
      std::vector<rocksdb::Status> ret;
      ret.push_back(rocksdb::Status::NotSupported(
         "MultiGet is supported by SplinterDB yet"));
      return ret;
   }

   rocksdb::Iterator *
   NewIterator(const rocksdb::ReadOptions  &options,
               rocksdb::ColumnFamilyHandle *column_family) override
   {
      return nullptr;
   }

   rocksdb::Status
   NewIterators(
      const rocksdb::ReadOptions                       &options,
      const std::vector<rocksdb::ColumnFamilyHandle *> &column_families,
      std::vector<rocksdb::Iterator *>                 *iterators)
   {
      return rocksdb::Status::NotSupported(
         "NewIterators is supported by SplinterDB yet");
   }

   const rocksdb::Snapshot *
   GetSnapshot() override
   {
      return nullptr;
   }

   void
   ReleaseSnapshot(const rocksdb::Snapshot *snapshot) override
   {}

   bool
   GetProperty(rocksdb::ColumnFamilyHandle *column_families,
               const rocksdb::Slice        &property,
               std::string                 *value) override
   {
      return false;
   }

   bool
   GetMapProperty(rocksdb::ColumnFamilyHandle                *column_families,
                  const rocksdb::Slice                       &property,
                  std::map<std::__cxx11::basic_string<char>,
                           std::__cxx11::basic_string<char>> *value) override
   {
      return false;
   }

   bool
   GetIntProperty(rocksdb::ColumnFamilyHandle *column_families,
                  const rocksdb::Slice        &property,
                  uint64_t                    *value) override
   {
      return false;
   }

   bool
   GetAggregatedIntProperty(const rocksdb::Slice &property,
                            uint64_t             *value) override
   {
      return false;
   }

   rocksdb::Status
   GetApproximateSizes(const rocksdb::SizeApproximationOptions &options,
                       rocksdb::ColumnFamilyHandle             *column_families,
                       const rocksdb::Range                    *ranges,
                       int                                      n,
                       uint64_t                                *sizes) override
   {
      return rocksdb::Status::NotSupported(
         "GetApproximateSizes is not supported by SplinterDB");
   }

   void
   GetApproximateMemTableStats(rocksdb::ColumnFamilyHandle *column_families,
                               const rocksdb::Range        &range,
                               uint64_t                    *count,
                               uint64_t                    *size) override
   {}

   rocksdb::Status
   CompactRange(const rocksdb::CompactRangeOptions &options,
                rocksdb::ColumnFamilyHandle        *column_families,
                const rocksdb::Slice               *begin,
                const rocksdb::Slice               *end) override
   {
      return rocksdb::Status::NotSupported(
         "CompactRange is not supported by SplinterDB");
   }

   rocksdb::Status
   SetDBOptions(const std::unordered_map<std::__cxx11::basic_string<char>,
                                         std::__cxx11::basic_string<char>>
                   &new_options) override
   {
      return rocksdb::Status::OK();
   }

   rocksdb::Status
   CompactFiles(
      const rocksdb::CompactionOptions                    &compact_options,
      rocksdb::ColumnFamilyHandle                         *column_family,
      const std::vector<std::__cxx11::basic_string<char>> &input_file_names,
      const int                                            output_level,
      const int                                            output_path_id = -1,
      std::vector<std::__cxx11::basic_string<char>> *const output_file_names =
         nullptr,
      rocksdb::CompactionJobInfo *compaction_job_info = nullptr) override
   {
      return rocksdb::Status::NotSupported(
         "CompactFiles is not supported by SplinterDB");
   }

   rocksdb::Status
   PauseBackgroundWork() override
   {
      return rocksdb::Status::NotSupported(
         "PauseBackgroundWork is not supported by SplinterDB");
   }

   rocksdb::Status
   ContinueBackgroundWork() override
   {
      return rocksdb::Status::NotSupported(
         "ContinueBackgroundWork is not supported by SplinterDB");
   }

   rocksdb::Status
   EnableAutoCompaction(const std::vector<rocksdb::ColumnFamilyHandle *>
                           &column_family_handles) override
   {
      return rocksdb::Status::NotSupported(
         "EnableAutoCompaction is not supported by SplinterDB");
   }

   void
   DisableManualCompaction() override
   {}

   void
   EnableManualCompaction() override
   {}

   int
   NumberLevels(rocksdb::ColumnFamilyHandle *column_family) override
   {
      return 0;
   }

   int
   MaxMemCompactionLevel(rocksdb::ColumnFamilyHandle *column_family) override
   {
      return 0;
   }

   int
   Level0StopWriteTrigger(rocksdb::ColumnFamilyHandle *column_family) override
   {
      return 0;
   }

   const std::string &
   GetName() const override
   {
      return dbname_;
   }

   rocksdb::Env *
   GetEnv() const override
   {
      return rocksdb::Env::Default();
   }

   rocksdb::Options
   GetOptions(rocksdb::ColumnFamilyHandle *column_family) const override
   {
      return options_;
   }

   rocksdb::DBOptions
   GetDBOptions() const override
   {
      return dboptions_;
   }

   rocksdb::Status
   Flush(const rocksdb::FlushOptions &options,
         rocksdb::ColumnFamilyHandle *column_family) override
   {
      return rocksdb::Status::NotSupported(
         "Flush is not supported by SplinterDB");
   }

   rocksdb::Status
   Flush(const rocksdb::FlushOptions &options,
         const std::vector<rocksdb::ColumnFamilyHandle *>
            &column_family_hanldes) override
   {
      return rocksdb::Status::NotSupported(
         "Flush is not supported by SplinterDB");
   }

   rocksdb::Status
   SyncWAL() override
   {
      return rocksdb::Status::NotSupported(
         "SyncWAL is not supported by SplinterDB");
   }

   rocksdb::SequenceNumber
   GetLatestSequenceNumber() const override
   {
      rocksdb::SequenceNumber sn = 0;
      return sn;
   }

   rocksdb::Status
   DisableFileDeletions() override
   {
      return rocksdb::Status::NotSupported(
         "DisableFileDeletions is not supported by SplinterDB");
   }

   rocksdb::Status
   IncreaseFullHistoryTsLow(rocksdb::ColumnFamilyHandle *, std::string) override
   {
      return rocksdb::Status::NotSupported(
         "IncreaseFullHistoryTsLow is not supported by SplinterDB");
   }

   rocksdb::Status
   GetFullHistoryTsLow(rocksdb::ColumnFamilyHandle *, std::string *) override
   {
      return rocksdb::Status::NotSupported(
         "GetFullHistoryTsLow is not supported by SplinterDB");
   }

   rocksdb::Status
   EnableFileDeletions(bool) override
   {
      return rocksdb::Status::NotSupported(
         "EnableFileDeletions is not supported by SplinterDB");
   }

   rocksdb::Status
   GetCreationTimeOfOldestFile(uint64_t *) override
   {
      return rocksdb::Status::NotSupported(
         "GetCreationTimeOfOldestFile is not supported by SplinterDB");
   }

   rocksdb::Status
   GetUpdatesSince(
      rocksdb::SequenceNumber,
      std::unique_ptr<rocksdb::TransactionLogIterator> *,
      const rocksdb::TransactionLogIterator::ReadOptions &) override
   {
      return rocksdb::Status::NotSupported(
         "GetUpdatesSince is not supported by SplinterDB");
   }

   rocksdb::Status
   DeleteFile(std::string) override
   {
      return rocksdb::Status::NotSupported(
         "DeleteFile is not supported by SplinterDB");
   }

   rocksdb::Status
   GetLiveFilesChecksumInfo(rocksdb::FileChecksumList *) override
   {
      return rocksdb::Status::NotSupported(
         "GetLiveFilesChecksumInfo is not supported by SplinterDB");
   }

   rocksdb::Status
   GetLiveFilesStorageInfo(const rocksdb::LiveFilesStorageInfoOptions &,
                           std::vector<rocksdb::LiveFileStorageInfo> *) override
   {
      return rocksdb::Status::NotSupported(
         "GetLiveFilesStorageInfo is not supported by SplinterDB");
   }

   rocksdb::Status
   GetLiveFiles(std::vector<std::__cxx11::basic_string<char>> &,
                uint64_t *,
                bool) override
   {
      return rocksdb::Status::NotSupported(
         "GetLiveFiles is not supported by SplinterDB");
   }

   rocksdb::Status
   GetSortedWalFiles(rocksdb::VectorLogPtr &) override
   {
      return rocksdb::Status::NotSupported(
         "GetSortedWalFiles is not supported by SplinterDB");
   }

   rocksdb::Status
   GetCurrentWalFile(std::unique_ptr<rocksdb::LogFile> *) override
   {
      return rocksdb::Status::NotSupported(
         "GetCurrentWalFile is not supported by SplinterDB");
   }

   rocksdb::Status
   IngestExternalFile(rocksdb::ColumnFamilyHandle *,
                      const std::vector<std::__cxx11::basic_string<char>> &,
                      const rocksdb::IngestExternalFileOptions &) override
   {
      return rocksdb::Status::NotSupported(
         "IngestExternalFile is not supported by SplinterDB");
   }

   rocksdb::Status
   IngestExternalFiles(
      const std::vector<rocksdb::IngestExternalFileArg> &) override
   {
      return rocksdb::Status::NotSupported(
         "IngestExternalFiles is not supported by SplinterDB");
   }

   rocksdb::Status
   CreateColumnFamilyWithImport(const rocksdb::ColumnFamilyOptions &,
                                const std::string &,
                                const rocksdb::ImportColumnFamilyOptions &,
                                const rocksdb::ExportImportFilesMetaData &,
                                rocksdb::ColumnFamilyHandle **) override
   {
      return rocksdb::Status::NotSupported(
         "CreateColumnFamilyWithImport is not supported by SplinterDB");
   }

   rocksdb::Status
   VerifyChecksum(const rocksdb::ReadOptions &) override
   {
      return rocksdb::Status::NotSupported(
         "VerifyChecksum is not supported by SplinterDB");
   }

   rocksdb::Status
   GetDbIdentity(std::string &) const override
   {
      return rocksdb::Status::NotSupported(
         "GetDbIdentity is not supported by SplinterDB");
   }

   rocksdb::Status
   GetDbSessionId(std::string &) const override
   {
      return rocksdb::Status::NotSupported(
         "GetDbSessionId is not supported by SplinterDB");
   }

   rocksdb::ColumnFamilyHandle *
   DefaultColumnFamily() const override
   {
      return nullptr;
   }

   rocksdb::Status
   GetPropertiesOfAllTables(rocksdb::ColumnFamilyHandle *,
                            rocksdb::TablePropertiesCollection *) override
   {
      return rocksdb::Status::NotSupported(
         "GetPropertiesOfAllTables is not supported by SplinterDB");
   }

   rocksdb::Status
   GetPropertiesOfTablesInRange(rocksdb::ColumnFamilyHandle *,
                                const rocksdb::Range *,
                                std::size_t,
                                rocksdb::TablePropertiesCollection *) override
   {
      return rocksdb::Status::NotSupported(
         "GetPropertiesOfTablesInRange is not supported by SplinterDB");
   }

   rocksdb::Status
   Close() override
   {
      splinterdb_close(&spl_handle_);
      return rocksdb::Status::OK();
   }

private:
   std::string        dbname_;
   rocksdb::Options   options_;
   rocksdb::DBOptions dboptions_;

   splinterdb       *spl_handle_;
   data_config       spl_data_cfg_;
   splinterdb_config spl_cfg_;
};

rocksdb::Status
rocksdb::DB::Open(const rocksdb::Options &options,
                  const std::string      &name,
                  rocksdb::DB           **dbptr)
{
   *dbptr = new SplinterRocksDB();
   return rocksdb::Status::OK();
}

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
