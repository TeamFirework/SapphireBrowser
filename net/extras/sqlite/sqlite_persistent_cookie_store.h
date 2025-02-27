// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_COOKIE_STORE_H_
#define NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_COOKIE_STORE_H_

#include <list>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/cookies/cookie_monster.h"
#include "net/log/net_log_with_source.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace net {
class CanonicalCookie;
class CookieCryptoDelegate;

// Implements the PersistentCookieStore interface in terms of a SQLite database.
// For documentation about the actual member functions consult the documentation
// of the parent class |CookieMonster::PersistentCookieStore|.
class COMPONENT_EXPORT(NET_EXTRAS) SQLitePersistentCookieStore
    : public CookieMonster::PersistentCookieStore {
 public:
  // Contains the origin and a bool indicating whether or not the
  // origin is secure.
  typedef std::pair<std::string, bool> CookieOrigin;

  // All blocking database accesses will be performed on
  // |background_task_runner|, while |client_task_runner| is used to invoke
  // callbacks.
  SQLitePersistentCookieStore(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      bool restore_old_session_cookies,
      CookieCryptoDelegate* crypto_delegate);

  // Deletes the cookies whose origins match those given in |cookies|.
  void DeleteAllInList(const std::list<CookieOrigin>& cookies);

  // CookieMonster::PersistentCookieStore:
  void Load(const LoadedCallback& loaded_callback,
            const NetLogWithSource& net_log) override;
  void LoadCookiesForKey(const std::string& key,
                         const LoadedCallback& callback) override;
  void AddCookie(const CanonicalCookie& cc) override;
  void UpdateCookieAccessTime(const CanonicalCookie& cc) override;
  void DeleteCookie(const CanonicalCookie& cc) override;
  void SetForceKeepSessionState() override;
  void SetBeforeFlushCallback(base::RepeatingClosure callback) override;
  void Flush(base::OnceClosure callback) override;

  // Returns how many operations are currently queued. For test use only;
  // and the background thread needs to be wedged for accessing this to be
  // non-racey. Also requires the client thread to be current.
  size_t GetQueueLengthForTesting();

 private:
  ~SQLitePersistentCookieStore() override;
  void CompleteLoad(const LoadedCallback& callback,
                    std::vector<std::unique_ptr<CanonicalCookie>> cookie_list);
  void CompleteKeyedLoad(
      const std::string& key,
      const LoadedCallback& callback,
      std::vector<std::unique_ptr<CanonicalCookie>> cookie_list);

  class Backend;

  const scoped_refptr<Backend> backend_;
  NetLogWithSource net_log_;

  DISALLOW_COPY_AND_ASSIGN(SQLitePersistentCookieStore);
};

}  // namespace net

#endif  // NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_COOKIE_STORE_H_
