// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_INFO_H_
#define EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_INFO_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"

namespace extensions {

class Alias;

// A global object that holds the extension permission instances and provides
// methods for accessing them.
class PermissionsInfo {
 public:
  static PermissionsInfo* GetInstance();

  // Registers the permissions specified by |infos| along with the
  // |aliases|.
  // TODO(devlin): Convert |aliases| to be a base::span.
  void RegisterPermissions(base::span<const APIPermissionInfo::InitInfo> infos,
                           const std::vector<Alias>& aliases);

  // Returns the permission with the given |id|, and NULL if it doesn't exist.
  const APIPermissionInfo* GetByID(APIPermission::ID id) const;

  // Returns the permission with the given |name|, and NULL if none
  // exists.
  const APIPermissionInfo* GetByName(const std::string& name) const;

  // Returns a set containing all valid api permission ids.
  APIPermissionSet GetAll() const;

  // Converts all the permission names in |permission_names| to permission ids.
  APIPermissionSet GetAllByName(
      const std::set<std::string>& permission_names) const;

  // Checks if any permissions have names that start with |name| followed by a
  // period.
  bool HasChildPermissions(const std::string& name) const;

  // Gets the total number of API permissions.
  size_t get_permission_count() const { return permission_count_; }

 private:
  friend struct base::LazyInstanceTraitsBase<PermissionsInfo>;

  PermissionsInfo();

  virtual ~PermissionsInfo();

  // Registers an |alias| for a given permission |name|.
  void RegisterAlias(const Alias& alias);

  // Registers a permission with the specified attributes and flags.
  void RegisterPermission(std::unique_ptr<APIPermissionInfo> permission);

  // Maps permission ids to permissions. Owns the permissions.
  typedef std::map<APIPermission::ID, std::unique_ptr<APIPermissionInfo>> IDMap;

  // Maps names and aliases to permissions. Doesn't own the permissions.
  typedef std::map<std::string, APIPermissionInfo*> NameMap;

  IDMap id_map_;
  NameMap name_map_;

  size_t permission_count_;

  DISALLOW_COPY_AND_ASSIGN(PermissionsInfo);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_INFO_H_
