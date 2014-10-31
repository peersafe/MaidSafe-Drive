#include <cassert>
#include <memory>
#include <stdexcept>

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/config.h"
#include "maidsafe/common/on_scope_exit.h"
#include "maidsafe/common/test.h"
#include "maidsafe/drive/file.h"

namespace maidsafe {
namespace drive {
namespace detail {
namespace test {

const std::uint32_t kTestMemoryUsageMax = kMaxChunkSize;
const std::uint32_t kTestDiskUsageMax = kTestMemoryUsageMax;

namespace {
  class FileTests : public ::testing::Test {
   protected:
    FileTests()
      : ::testing::Test(),
        asio_service_(),
        test_path_() {
    }

    void WaitForHandlers(const std::size_t number_handlers) {
      std::size_t completed = 0;
      unsigned iterations = 0;
      do {
        ASSERT_GE(3, iterations);
        ++iterations;

        std::this_thread::sleep_for(detail::kFileInactivityDelay);
        asio_service_.reset();
        completed += asio_service_.poll();
      }
      while (completed < number_handlers);

      EXPECT_EQ(number_handlers, completed);
    }

    std::shared_ptr<File> CreateTestFile() {
      return File::Create(asio_service_, "foo", false);
    }

    void OpenTestFile(File& test_file) {
      if (test_path_ == nullptr) {
        test_path_ = ::maidsafe::test::CreateTestPath("MaidSafe_Test_Drive");
        if (test_path_ == nullptr || test_path_->string() == "") {
          throw std::runtime_error("Unable to create test path");
        }
      }

      test_file.Open(
          // callback used for retrieving from long-term storage (not needed in this test currently)
          [](const std::string&) { return NonEmptyString("bar"); },
          MemoryUsage(kTestMemoryUsageMax),
          DiskUsage(kTestDiskUsageMax),
          *test_path_);
    }

    static std::uint32_t WriteTestFile(
        File& test_file,
        const std::string contents,
        const std::uint32_t offset) {
      assert(contents.size() <= std::numeric_limits<std::uint32_t>::max());
      return test_file.Write(contents.data(), std::uint32_t(contents.size()), offset);
    }

    static std::string ReadTestFile(
        File& test_file, const std::uint32_t length, const std::uint32_t offset) {
      if (length == 0) {
        return std::string();
      }

      std::string file_contents;
      file_contents.resize(length);
      file_contents.resize(test_file.Read(&file_contents[0], length, offset));
      return file_contents;
    }

    static std::string ReadTestFile(File& test_file) {
      const auto file_size = test_file.meta_data.size();
      assert(file_size <= std::numeric_limits<std::uint32_t>::max());
      const std::string file_contents = ReadTestFile(test_file, std::uint32_t(file_size), 0);
      EXPECT_EQ(file_size, file_contents.size());
      return file_contents;
    }

   private:
    boost::asio::io_service asio_service_;
    ::maidsafe::test::TestPath test_path_;
  };
} // anonymous


TEST_F(FileTests, BEH_EmptyFile) {
  const std::shared_ptr<File> test_file = CreateTestFile();
  ASSERT_NE(nullptr, test_file.get());

  EXPECT_EQ("foo", test_file->meta_data.name().string());
  EXPECT_NE(nullptr, test_file->meta_data.data_map());
  EXPECT_EQ(nullptr, test_file->meta_data.directory_id());

  const auto creation_time = test_file->meta_data.creation_time();
  EXPECT_EQ(creation_time, test_file->meta_data.last_status_time());
  EXPECT_EQ(creation_time, test_file->meta_data.last_write_time());
  EXPECT_EQ(creation_time, test_file->meta_data.last_access_time());

  EXPECT_EQ(0u, test_file->meta_data.size());
  EXPECT_EQ(0u, test_file->meta_data.allocation_size());
  EXPECT_EQ(MetaData::FileType::regular_file, test_file->meta_data.file_type());
}

TEST_F(FileTests, BEH_WriteReadFile) {
  const std::shared_ptr<File> test_file = CreateTestFile();
  ASSERT_NE(nullptr, test_file.get());
  EXPECT_EQ("foo", test_file->meta_data.name().string());
  EXPECT_NE(nullptr, test_file->meta_data.data_map());
  EXPECT_EQ(nullptr, test_file->meta_data.directory_id());
  const auto creation_time = test_file->meta_data.creation_time();
  EXPECT_EQ(creation_time, test_file->meta_data.last_status_time());
  EXPECT_EQ(creation_time, test_file->meta_data.last_write_time());
  EXPECT_EQ(creation_time, test_file->meta_data.last_access_time());
  EXPECT_EQ(0u, test_file->meta_data.size());
  EXPECT_EQ(0u, test_file->meta_data.allocation_size());
  EXPECT_EQ(MetaData::FileType::regular_file, test_file->meta_data.file_type());

  const on_scope_exit close_file([test_file] { test_file->Close(); });
  OpenTestFile(*test_file);

  const std::string test_output("output text");
  EXPECT_EQ(test_output.size(), WriteTestFile(*test_file, test_output, 0));
  const auto last_write_time = test_file->meta_data.last_write_time();
  EXPECT_EQ(creation_time, test_file->meta_data.creation_time());
  EXPECT_LE(creation_time, test_file->meta_data.last_write_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_status_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_access_time());
  EXPECT_EQ(test_output.size(), test_file->meta_data.size());
  EXPECT_EQ(test_output.size(), test_file->meta_data.allocation_size());
  EXPECT_EQ(MetaData::FileType::regular_file, test_file->meta_data.file_type());

  EXPECT_EQ(test_output, ReadTestFile(*test_file));
  EXPECT_EQ(creation_time, test_file->meta_data.creation_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_write_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_status_time());
  EXPECT_LE(last_write_time, test_file->meta_data.last_access_time());
  EXPECT_EQ(test_output.size(), test_file->meta_data.size());
  EXPECT_EQ(test_output.size(), test_file->meta_data.allocation_size());
  EXPECT_EQ(MetaData::FileType::regular_file, test_file->meta_data.file_type());
}

TEST_F(FileTests, BEH_ReadPastEnd) {
  const std::shared_ptr<File> test_file = CreateTestFile();
  ASSERT_NE(nullptr, test_file.get());
  EXPECT_EQ("foo", test_file->meta_data.name().string());
  EXPECT_NE(nullptr, test_file->meta_data.data_map());
  EXPECT_EQ(nullptr, test_file->meta_data.directory_id());
  const auto creation_time = test_file->meta_data.creation_time();
  EXPECT_EQ(creation_time, test_file->meta_data.last_status_time());
  EXPECT_EQ(creation_time, test_file->meta_data.last_write_time());
  EXPECT_EQ(creation_time, test_file->meta_data.last_access_time());
  EXPECT_EQ(0u, test_file->meta_data.size());
  EXPECT_EQ(0u, test_file->meta_data.allocation_size());
  EXPECT_EQ(MetaData::FileType::regular_file, test_file->meta_data.file_type());

  const on_scope_exit close_file([test_file] { test_file->Close(); });
  OpenTestFile(*test_file);

  EXPECT_EQ(std::string(), ReadTestFile(*test_file, 100, 0));
  EXPECT_EQ(creation_time, test_file->meta_data.creation_time());
  EXPECT_EQ(creation_time, test_file->meta_data.last_write_time());
  EXPECT_EQ(creation_time, test_file->meta_data.last_status_time());
  EXPECT_LE(creation_time, test_file->meta_data.last_access_time());
  EXPECT_EQ(0u, test_file->meta_data.size());
  EXPECT_EQ(0u, test_file->meta_data.allocation_size());
  EXPECT_EQ(MetaData::FileType::regular_file, test_file->meta_data.file_type());

  const std::string test_output(100, 'f');
  EXPECT_EQ(test_output.size(), WriteTestFile(*test_file, test_output, 0));
  const auto last_write_time = test_file->meta_data.last_write_time();
  EXPECT_EQ(creation_time, test_file->meta_data.creation_time());
  EXPECT_LE(creation_time, test_file->meta_data.last_write_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_status_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_access_time());
  EXPECT_EQ(test_output.size(), test_file->meta_data.size());
  EXPECT_EQ(test_output.size(), test_file->meta_data.allocation_size());
  EXPECT_EQ(MetaData::FileType::regular_file, test_file->meta_data.file_type());

  EXPECT_EQ(std::string(), ReadTestFile(*test_file, 100, 101));
  const auto last_read_time = test_file->meta_data.last_write_time();
  EXPECT_EQ(creation_time, test_file->meta_data.creation_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_write_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_status_time());
  EXPECT_LE(last_write_time, test_file->meta_data.last_access_time());
  EXPECT_EQ(test_output.size(), test_file->meta_data.size());
  EXPECT_EQ(test_output.size(), test_file->meta_data.allocation_size());
  EXPECT_EQ(MetaData::FileType::regular_file, test_file->meta_data.file_type());

  EXPECT_EQ(std::string(50, 'f'), ReadTestFile(*test_file, 100, 50));
  EXPECT_EQ(creation_time, test_file->meta_data.creation_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_write_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_status_time());
  EXPECT_LE(last_read_time, test_file->meta_data.last_access_time());
  EXPECT_EQ(test_output.size(), test_file->meta_data.size());
  EXPECT_EQ(test_output.size(), test_file->meta_data.allocation_size());
  EXPECT_EQ(MetaData::FileType::regular_file, test_file->meta_data.file_type());
}

TEST_F(FileTests, BEH_TruncateIncrease) {
  const std::shared_ptr<File> test_file = CreateTestFile();
  ASSERT_NE(nullptr, test_file.get());
  EXPECT_EQ("foo", test_file->meta_data.name().string());
  EXPECT_NE(nullptr, test_file->meta_data.data_map());
  EXPECT_EQ(nullptr, test_file->meta_data.directory_id());
  const auto creation_time = test_file->meta_data.creation_time();
  EXPECT_EQ(creation_time, test_file->meta_data.last_status_time());
  EXPECT_EQ(creation_time, test_file->meta_data.last_write_time());
  EXPECT_EQ(creation_time, test_file->meta_data.last_access_time());
  EXPECT_EQ(0u, test_file->meta_data.size());
  EXPECT_EQ(0u, test_file->meta_data.allocation_size());
  EXPECT_EQ(MetaData::FileType::regular_file, test_file->meta_data.file_type());

  const on_scope_exit close_file([test_file] { test_file->Close(); });
  OpenTestFile(*test_file);

  const std::size_t new_file_size = 100;
  test_file->Truncate(new_file_size);
  const auto last_write_time = test_file->meta_data.last_write_time();
  EXPECT_EQ(creation_time, test_file->meta_data.creation_time());
  EXPECT_LE(creation_time, test_file->meta_data.last_write_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_status_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_access_time());
  EXPECT_EQ(new_file_size, test_file->meta_data.size());
  EXPECT_EQ(new_file_size, test_file->meta_data.allocation_size());
  EXPECT_EQ(MetaData::FileType::regular_file, test_file->meta_data.file_type());

  EXPECT_EQ(std::string(new_file_size, '\0'), ReadTestFile(*test_file));
  EXPECT_EQ(creation_time, test_file->meta_data.creation_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_write_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_status_time());
  EXPECT_LE(last_write_time, test_file->meta_data.last_access_time());
  EXPECT_EQ(new_file_size, test_file->meta_data.size());
  EXPECT_EQ(new_file_size, test_file->meta_data.allocation_size());
  EXPECT_EQ(MetaData::FileType::regular_file, test_file->meta_data.file_type());
}

TEST_F(FileTests, BEH_TruncateDecrease) {
  const std::shared_ptr<File> test_file = CreateTestFile();
  ASSERT_NE(nullptr, test_file.get());
  EXPECT_EQ("foo", test_file->meta_data.name().string());
  EXPECT_NE(nullptr, test_file->meta_data.data_map());
  EXPECT_EQ(nullptr, test_file->meta_data.directory_id());
  const auto creation_time = test_file->meta_data.creation_time();
  EXPECT_EQ(creation_time, test_file->meta_data.last_status_time());
  EXPECT_EQ(creation_time, test_file->meta_data.last_write_time());
  EXPECT_EQ(creation_time, test_file->meta_data.last_access_time());
  EXPECT_EQ(0u, test_file->meta_data.size());
  EXPECT_EQ(0u, test_file->meta_data.allocation_size());
  EXPECT_EQ(MetaData::FileType::regular_file, test_file->meta_data.file_type());

  const on_scope_exit close_file([test_file] { test_file->Close(); });
  OpenTestFile(*test_file);

  const std::string test_output(100, 'f');
  EXPECT_EQ(test_output.size(), WriteTestFile(*test_file, test_output, 0));
  const auto first_write_time = test_file->meta_data.last_write_time();
  EXPECT_EQ(creation_time, test_file->meta_data.creation_time());
  EXPECT_LE(creation_time, test_file->meta_data.last_write_time());
  EXPECT_EQ(first_write_time, test_file->meta_data.last_status_time());
  EXPECT_EQ(first_write_time, test_file->meta_data.last_access_time());
  EXPECT_EQ(test_output.size(), test_file->meta_data.size());
  EXPECT_EQ(test_output.size(), test_file->meta_data.allocation_size());
  EXPECT_EQ(MetaData::FileType::regular_file, test_file->meta_data.file_type());

  const std::size_t new_file_size = 50;
  test_file->Truncate(new_file_size);
  const auto last_write_time = test_file->meta_data.last_write_time();
  EXPECT_EQ(creation_time, test_file->meta_data.creation_time());
  EXPECT_LE(first_write_time, test_file->meta_data.last_write_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_status_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_access_time());
  EXPECT_EQ(new_file_size, test_file->meta_data.size());
  EXPECT_EQ(new_file_size, test_file->meta_data.allocation_size());
  EXPECT_EQ(MetaData::FileType::regular_file, test_file->meta_data.file_type());

  EXPECT_EQ(std::string(new_file_size, 'f'), ReadTestFile(*test_file));
  EXPECT_EQ(creation_time, test_file->meta_data.creation_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_write_time());
  EXPECT_EQ(last_write_time, test_file->meta_data.last_status_time());
  EXPECT_LE(last_write_time, test_file->meta_data.last_access_time());
  EXPECT_EQ(new_file_size, test_file->meta_data.size());
  EXPECT_EQ(new_file_size, test_file->meta_data.allocation_size());
  EXPECT_EQ(MetaData::FileType::regular_file, test_file->meta_data.file_type());
}

TEST_F(FileTests, BEH_CloseTimer) {
  const std::shared_ptr<File> test_file = CreateTestFile();
  EXPECT_EQ(0u, test_file->meta_data.size());
  EXPECT_EQ(0u, test_file->meta_data.allocation_size());

  const std::size_t file_size = 500;
  {
    const on_scope_exit close_file([test_file] { test_file->Close(); });
    OpenTestFile(*test_file);
    test_file->Truncate(file_size);
    EXPECT_EQ(file_size, test_file->meta_data.size());
    EXPECT_EQ(file_size, test_file->meta_data.allocation_size());
  }

  WaitForHandlers(1);
  EXPECT_EQ(file_size, test_file->meta_data.size());
  EXPECT_EQ(file_size, test_file->meta_data.allocation_size());
}

TEST_F(FileTests, BEH_ExceedMaxDiskUsage) {
  const std::shared_ptr<File> test_file = CreateTestFile();
  EXPECT_EQ(0u, test_file->meta_data.size());
  EXPECT_EQ(0u, test_file->meta_data.allocation_size());

  const std::string random_data(
      RandomString((kTestMemoryUsageMax + kTestDiskUsageMax) * 2));
  {
    const on_scope_exit close_file([test_file] { test_file->Close(); });
    OpenTestFile(*test_file);
    WriteTestFile(*test_file, random_data, 0);
    EXPECT_EQ(random_data.size(), test_file->meta_data.size());
    EXPECT_EQ(random_data.size(), test_file->meta_data.allocation_size());
  }

  // This should throw an exception once the chunks are properly being stored
  WaitForHandlers(1);
}

} // test
} // detail
} // drive
} // maidsafe
