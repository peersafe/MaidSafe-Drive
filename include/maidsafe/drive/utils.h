/* Copyright 2011 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

#ifndef MAIDSAFE_DRIVE_UTILS_H_
#define MAIDSAFE_DRIVE_UTILS_H_


#include <memory>
#include <string>
#include <vector>

#include "boost/filesystem/path.hpp"

#include "maidsafe/encrypt/self_encryptor.h"
#include "maidsafe/drive/config.h"
#include "maidsafe/nfs/nfs.h"

namespace maidsafe {
namespace drive {

static const uint32_t kDirectorySize = 4096;

template<typename Storage>
struct FileContext {
  FileContext();
  FileContext(const boost::filesystem::path& name, bool is_directory);
  explicit FileContext(std::shared_ptr<MetaData> meta_data_in);

  std::shared_ptr<MetaData> meta_data;
  std::shared_ptr<encrypt::SelfEncryptor<Storage>> self_encryptor;
  bool content_changed;
  DirectoryId grandparent_directory_id, parent_directory_id;
};

template<typename Storage>
FileContext<Storage>::FileContext()
    : meta_data(new MetaData),
      self_encryptor(),
      content_changed(false),
      grandparent_directory_id(),
      parent_directory_id() {}

template<typename Storage>
FileContext<Storage>::FileContext(const boost::filesystem::path& name, bool is_directory)
      : meta_data(new MetaData(name, is_directory)),
        self_encryptor(),
        content_changed(!is_directory),
        grandparent_directory_id(),
        parent_directory_id() {}

template<typename Storage>
FileContext<Storage>::FileContext(std::shared_ptr<MetaData> meta_data_in)
    : meta_data(meta_data_in),
      self_encryptor(),
      content_changed(false),
      grandparent_directory_id(),
      parent_directory_id() {}

#ifndef MAIDSAFE_WIN32
template<typename Storage>
int ForceFlush(std::shared_ptr<DirectoryListingHandler<Storage>> directory_listing_handler,
               FileContext<Storage>* file_context);
#endif

bool ExcludedFilename(const boost::filesystem::path& path);

bool MatchesMask(std::wstring mask, const boost::filesystem::path& file_name);
bool SearchesMask(std::wstring mask, const boost::filesystem::path& file_name);

namespace detail {

template<typename Storage, typename Directory>
struct Put {

  void operator()(Storage& storage, const Directory& directory) {
    storage.Put(directory.name(), directory.Serialise());
  }
};

template<typename Directory>
struct Put<nfs::ClientMaidNfs, Directory> {
  typedef nfs::ClientMaidNfs ClientNfs;

  void operator()(ClientNfs& storage, const Directory& directory) {
    storage.Put<Directory>(directory,
                           passport::PublicPmid::name_type(directory.name()),
                           nullptr);
  }
};

template<typename Storage, typename Directory>
struct Get {

  NonEmptyString operator()(Storage& storage, const typename Directory::name_type& name) {
    return storage.Get(name);
  }
};

template<typename Directory>
struct Get<nfs::ClientMaidNfs, Directory> {
  typedef nfs::ClientMaidNfs ClientNfs;

  NonEmptyString operator()(ClientNfs& storage, const typename Directory::name_type& name) {
    storage.Get<Directory>(name, nullptr);  // FIXME ...value returned in response_functor
    return NonEmptyString();
  }
};

template<typename Storage, typename Directory>
struct Delete {

  void operator()(Storage& storage, const typename Directory::name_type& name) {
    storage.Delete(name);
  }
};

template<typename Directory>
struct Delete<nfs::ClientMaidNfs, Directory> {
  typedef nfs::ClientMaidNfs ClientNfs;

  void operator()(ClientNfs& storage, const typename Directory::name_type& name) {
    storage.Delete<Directory>(name, nullptr);
  }
};

}  // namespace detail

}  // namespace drive
}  // namespace maidsafe

#endif  // MAIDSAFE_DRIVE_UTILS_H_