// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The drive mount path used in the storage. It must be '/drive'.
 * @type {string}
 */
var STORED_DRIVE_MOUNT_PATH = '/drive';

/**
 * Model for the folder shortcuts. This object is cr.ui.ArrayDataModel-like
 * object with additional methods for the folder shortcut feature.
 * This uses chrome.storage as backend. Items are always sorted by URL.
 *
 * @param {!VolumeManagerWrapper} volumeManager Volume manager instance.
 * @constructor
 * @extends {cr.EventTarget}
 */
function FolderShortcutsDataModel(volumeManager) {
  this.volumeManager_ = volumeManager;
  this.array_ = [];
  this.pendingPaths_ = {};  // Hash map for easier deleting.
  this.unresolvablePaths_ = {};
  this.lastDriveRootURL_ = null;

  // Queue to serialize resolving entries.
  this.queue_ = new AsyncUtil.Queue();
  this.queue_.run(
      this.volumeManager_.ensureInitialized.bind(this.volumeManager_));

  // Load the shortcuts. Runs within the queue.
  this.load_();

  // Listening for changes in the storage.
  chrome.storage.onChanged.addListener(function(changes, namespace) {
    if (!(FolderShortcutsDataModel.NAME in changes) || namespace !== 'sync')
      return;
    this.reload_();  // Runs within the queue.
  }.bind(this));

  // If the volume info list is changed, then shortcuts have to be reloaded.
  this.volumeManager_.volumeInfoList.addEventListener(
      'permuted', this.reload_.bind(this));

  // If the drive status has changed, then shortcuts have to be re-resolved.
  this.volumeManager_.addEventListener(
      'drive-connection-changed', this.reload_.bind(this));
}

/**
 * Key name in chrome.storage. The array are stored with this name.
 * @type {string}
 * @const
 */
FolderShortcutsDataModel.NAME = 'folder-shortcuts-list';

FolderShortcutsDataModel.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * @return {number} Number of elements in the array.
   */
  get length() {
    return this.array_.length;
  },

  /**
   * Remembers the Drive volume's root URL used for conversions between virtual
   * paths and URLs.
   * @private
   */
  rememberLastDriveURL_: function() {
    if (this.lastDriveRootURL_)
      return;
    var volumeInfo = this.volumeManager_.getCurrentProfileVolumeInfo(
        VolumeManagerCommon.VolumeType.DRIVE);
    if (volumeInfo)
      this.lastDriveRootURL_ = volumeInfo.fileSystem.root.toURL();
  },

  /**
   * Resolves Entries from a list of stored virtual paths. Runs within a queue.
   * @param {Array<string>} list List of virtual paths.
   * @private
   */
  processEntries_: function(list) {
    this.queue_.run(function(callback) {
      this.pendingPaths_ = {};
      this.unresolvablePaths_ = {};
      list.forEach(function(path) {
        this.pendingPaths_[path] = true;
      }, this);
      callback();
    }.bind(this));

    this.queue_.run(function(queueCallback) {
      var volumeInfo = this.volumeManager_.getCurrentProfileVolumeInfo(
          VolumeManagerCommon.VolumeType.DRIVE);
      var changed = false;
      var resolvedURLs = {};
      this.rememberLastDriveURL_();  // Required for conversions.

      var onResolveSuccess = function(path, entry) {
        if (path in this.pendingPaths_)
          delete this.pendingPaths_[path];
        if (path in this.unresolvablePaths_) {
          changed = true;
          delete this.unresolvablePaths_[path];
        }
        if (!this.exists(entry)) {
          changed = true;
          this.addInternal_(entry);
        }
        resolvedURLs[entry.toURL()] = true;
      }.bind(this);

      var onResolveFailure = function(path, url) {
        if (path in this.pendingPaths_)
          delete this.pendingPaths_[path];
        var existingIndex = this.getIndexByURL_(url);
        if (existingIndex !== -1) {
          changed = true;
          this.removeInternal_(this.item(existingIndex));
        }
        // Remove the shortcut on error, only if Drive is fully online.
        // Only then we can be sure, that the error means that the directory
        // does not exist anymore.
        if (!volumeInfo ||
            this.volumeManager_.getDriveConnectionState().type !==
                VolumeManagerCommon.DriveConnectionType.ONLINE) {
          if (!this.unresolvablePaths_[path]) {
            changed = true;
            this.unresolvablePaths_[path] = true;
          }
        }
        // Not adding to the model nor to the |unresolvablePaths_| means
        // that it will be removed from the storage permanently after the
        // next call to save_().
      }.bind(this);

      // Resolve the items all at once, in parallel.
      var group = new AsyncUtil.Group();
      list.forEach(function(path) {
        group.add(function(path, callback) {
          var url =
              this.lastDriveRootURL_ && this.convertStoredPathToUrl_(path);
          if (url && volumeInfo) {
            window.webkitResolveLocalFileSystemURL(
                url,
                function(entry) {
                  onResolveSuccess(path, entry);
                  callback();
                },
                function() {
                  onResolveFailure(path, url);
                  callback();
                });
          } else {
            onResolveFailure(path, url);
            callback();
          }
        }.bind(this, path));
      }, this);

      // Save the model after finishing.
      group.run(function() {
        // Remove all of those old entries, which were resolved by this method.
        var index = 0;
        while (index < this.length) {
          var entry = this.item(index);
          if (!resolvedURLs[entry.toURL()]) {
            this.removeInternal_(entry);
            changed = true;
          } else {
            index++;
          }
        }
        // If something changed, then save.
        if (changed)
          this.save_();
        queueCallback();
      }.bind(this));
    }.bind(this));
  },

  /**
   * Initializes the model and loads the shortcuts.
   * @private
   */
  load_: function() {
    this.queue_.run(function(callback) {
      chrome.storage.sync.get(FolderShortcutsDataModel.NAME, function(value) {
        if (chrome.runtime.lastError) {
          console.error('Failed to load shortcut paths from chrome.storage: ' +
              chrome.runtime.lastError.message);
          callback();
          return;
        }
        var shortcutPaths = value[FolderShortcutsDataModel.NAME] || [];

        // Record metrics.
        metrics.recordSmallCount('FolderShortcut.Count', shortcutPaths.length);

        // Resolve and add the entries to the model.
        this.processEntries_(shortcutPaths);  // Runs within a queue.
        callback();
      }.bind(this));
    }.bind(this));
  },

  /**
   * Reloads the model and loads the shortcuts.
   * @private
   */
  reload_: function() {
    var shortcutPaths;
    this.queue_.run(function(callback) {
      chrome.storage.sync.get(FolderShortcutsDataModel.NAME, function(value) {
        var shortcutPaths = value[FolderShortcutsDataModel.NAME] || [];
        this.processEntries_(shortcutPaths);  // Runs within a queue.
        callback();
      }.bind(this));
    }.bind(this));
  },

  /**
   * Returns the entries in the given range as a new array instance. The
   * arguments and return value are compatible with Array.slice().
   *
   * @param {number} begin Where to start the selection.
   * @param {number=} opt_end Where to end the selection.
   * @return {Array<Entry>} Entries in the selected range.
   */
  slice: function(begin, opt_end) {
    return this.array_.slice(begin, opt_end);
  },

  /**
   * @param {number} index Index of the element to be retrieved.
   * @return {Entry} The value of the |index|-th element.
   */
  item: function(index) {
    return this.array_[index];
  },

  /**
   * @param {string} value URL of the entry to be found.
   * @return {number} Index of the element with the specified |value|.
   * @private
   */
  getIndexByURL_: function(value) {
    for (var i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (this.array_[i].toURL() === value)
        return i;
    }
    return -1;
  },

  /**
   * @param {Entry} value Value of the element to be retrieved.
   * @return {number} Index of the element with the specified |value|.
   */
  getIndex: function(value) {
    for (var i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (util.isSameEntry(this.array_[i], value))
        return i;
    }
    return -1;
  },

  /**
   * Compares 2 entries and returns a number indicating one entry comes before
   * or after or is the same as the other entry in sort order.
   *
   * @param {Entry} a First entry.
   * @param {Entry} b Second entry.
   * @return {number} Returns -1, if |a| < |b|. Returns 0, if |a| === |b|.
   *     Otherwise, returns 1.
   */
  compare: function(a, b) {
    return util.comparePath(a, b);
  },

  /**
   * Adds the given item to the array. If there were already same item in the
   * list, return the index of the existing item without adding a duplicate
   * item.
   *
   * @param {Entry} value Value to be added into the array.
   * @return {number} Index in the list which the element added to.
   */
  add: function(value) {
    var result = this.addInternal_(value);
    metrics.recordUserAction('FolderShortcut.Add');
    this.save_();
    return result;
  },

  /**
   * Adds the given item to the array. If there were already same item in the
   * list, return the index of the existing item without adding a duplicate
   * item.
   *
   * @param {Entry} value Value to be added into the array.
   * @return {number} Index in the list which the element added to.
   * @private
   */
  addInternal_: function(value) {
    this.rememberLastDriveURL_();  // Required for saving.

    var oldArray = this.array_.slice(0);  // Shallow copy.
    var addedIndex = -1;
    for (var i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (util.isSameEntry(this.array_[i], value))
        return i;

      // Since the array is sorted, new item will be added just before the first
      // larger item.
      if (this.compare(this.array_[i], value) >= 0) {
        this.array_.splice(i, 0, value);
        addedIndex = i;
        break;
      }
    }
    // If value is not added yet, add it at the last.
    if (addedIndex == -1) {
      this.array_.push(value);
      addedIndex = this.length;
    }

    this.firePermutedEvent_(
        this.calculatePermutation_(oldArray, this.array_));
    return addedIndex;
  },

  /**
   * Removes the given item from the array.
   * @param {Entry} value Value to be removed from the array.
   * @return {number} Index in the list which the element removed from.
   */
  remove: function(value) {
    var result = this.removeInternal_(value);
    if (result !== -1) {
      this.save_();
      metrics.recordUserAction('FolderShortcut.Remove');
    }
    return result;
  },

  /**
   * Removes the given item from the array.
   *
   * @param {Entry} value Value to be removed from the array.
   * @return {number} Index in the list which the element removed from.
   * @private
   */
  removeInternal_: function(value) {
    var removedIndex = -1;
    var oldArray = this.array_.slice(0);  // Shallow copy.
    for (var i = 0; i < this.length; i++) {
      // Same item check: must be exact match.
      if (util.isSameEntry(this.array_[i], value)) {
        this.array_.splice(i, 1);
        removedIndex = i;
        break;
      }
    }

    if (removedIndex !== -1) {
      this.firePermutedEvent_(
          this.calculatePermutation_(oldArray, this.array_));
      return removedIndex;
    }

    // No item is removed.
    return -1;
  },

  /**
   * @param {Entry} entry Entry to be checked.
   * @return {boolean} True if the given |entry| exists in the array. False
   *     otherwise.
   */
  exists: function(entry) {
    var index = this.getIndex(entry);
    return (index >= 0);
  },

  /**
   * Saves the current array to chrome.storage.
   * @private
   */
  save_: function() {
    this.rememberLastDriveURL_();
    if (!this.lastDriveRootURL_)
      return;

    // TODO(mtomasz): Migrate to URL.
    var paths = this.array_.
                map(function(entry) { return entry.toURL(); }).
                map(this.convertUrlToStoredPath_.bind(this)).
                concat(Object.keys(this.pendingPaths_)).
                concat(Object.keys(this.unresolvablePaths_));

    var prefs = {};
    prefs[FolderShortcutsDataModel.NAME] = paths;
    chrome.storage.sync.set(prefs, function() {});
  },

  /**
   * Creates a permutation array for 'permuted' event, which is compatible with
   * a permutation array used in cr/ui/array_data_model.js.
   *
   * @param {Array<Entry>} oldArray Previous array before changing.
   * @param {Array<Entry>} newArray New array after changing.
   * @return {Array<number>} Created permutation array.
   * @private
   */
  calculatePermutation_: function(oldArray, newArray) {
    var oldIndex = 0;  // Index of oldArray.
    var newIndex = 0;  // Index of newArray.

    // Note that both new and old arrays are sorted.
    var permutation = [];
    for (; oldIndex < oldArray.length; oldIndex++) {
      if (newIndex >= newArray.length) {
        // oldArray[oldIndex] is deleted, which is not in the new array.
        permutation[oldIndex] = -1;
        continue;
      }

      while (newIndex < newArray.length) {
        // Unchanged item, which exists in both new and old array. But the
        // index may be changed.
        if (util.isSameEntry(oldArray[oldIndex], newArray[newIndex])) {
          permutation[oldIndex] = newIndex;
          newIndex++;
          break;
        }

        // oldArray[oldIndex] is deleted, which is not in the new array.
        if (this.compare(oldArray[oldIndex], newArray[newIndex]) < 0) {
          permutation[oldIndex] = -1;
          break;
        }

        // In the case of this.compare(oldArray[oldIndex]) > 0:
        // newArray[newIndex] is added, which is not in the old array.
        newIndex++;
      }
    }
    return permutation;
  },

  /**
   * Fires a 'permuted' event, which is compatible with cr.ui.ArrayDataModel.
   * @param {Array<number>} permutation Permutation array.
   */
  firePermutedEvent_: function(permutation) {
    var permutedEvent = new Event('permuted');
    permutedEvent.newLength = this.length;
    permutedEvent.permutation = permutation;
    this.dispatchEvent(permutedEvent);

    // Note: This model only fires 'permuted' event, because:
    // 1) 'change' event is not necessary to fire since it is covered by
    //    'permuted' event.
    // 2) 'splice' and 'sorted' events are not implemented. These events are
    //    not used in NavigationListModel. We have to implement them when
    //    necessary.
  },

  /**
   * Called externally when one of the items is not found on the filesystem.
   * @param {Entry} entry The entry which is not found.
   */
  onItemNotFoundError: function(entry) {
    // If Drive is online, then delete the shortcut permanently. Otherwise,
    // delete from model and add to |unresolvablePaths_|.
    if (this.volumeManager_.getDriveConnectionState().type !==
        VolumeManagerCommon.DriveConnectionType.ONLINE) {
      var path = this.convertUrlToStoredPath_(entry.toURL());
      // TODO(mtomasz): Add support for multi-profile.
      this.unresolvablePaths_[path] = true;
    }
    this.removeInternal_(entry);
    this.save_();
  },

  /**
   * Converts the given "stored path" to the URL.
   *
   * This conversion is necessary because the shortcuts are not stored with
   * stored-formatted mount paths for compatibility. See http://crbug.com/336155
   * for detail.
   *
   * @param {string} path Path in Drive with the stored drive mount path.
   * @return {?string} URL of the given path.
   * @private
   */
  convertStoredPathToUrl_: function(path) {
    if (path.indexOf(STORED_DRIVE_MOUNT_PATH + '/') !== 0) {
      console.warn(path + ' is neither a drive mount path nor a stored path.');
      return null;
    }
    return this.lastDriveRootURL_ + encodeURIComponent(
        path.substr(STORED_DRIVE_MOUNT_PATH.length));
  },

  /**
   * Converts the URL to the stored-formatted path.
   *
   * See the comment of convertStoredPathToUrl_() for further information.
   *
   * @param {string} url URL of the directory in Drive.
   * @return {?string} Path with the stored drive mount path.
   * @private
   */
  convertUrlToStoredPath_: function(url) {
    // Root URLs contain a trailing slash.
    if (url.indexOf(this.lastDriveRootURL_) !== 0) {
      console.warn(url + ' is not a drive URL.');
      return null;
    }

    return STORED_DRIVE_MOUNT_PATH + '/' + decodeURIComponent(
        url.substr(this.lastDriveRootURL_.length));
  },
};
