// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Externs generated from namespace: fileManagerPrivate */

/**
 * @const
 */
chrome.fileManagerPrivate = {};

/** @enum {string} */
chrome.fileManagerPrivate.VolumeType = {
  DRIVE: 'drive',
  DOWNLOADS: 'downloads',
  REMOVABLE: 'removable',
  ARCHIVE: 'archive',
  PROVIDED: 'provided',
  MTP: 'mtp',
  MEDIA_VIEW: 'media_view',
  CROSTINI: 'crostini',
  ANDROID_FILES: 'android_files',
  TESTING: 'testing',
};

/** @enum {string} */
chrome.fileManagerPrivate.DeviceType = {
  USB: 'usb',
  SD: 'sd',
  OPTICAL: 'optical',
  MOBILE: 'mobile',
  UNKNOWN: 'unknown',
};

/** @enum {string} */
chrome.fileManagerPrivate.MountCondition = {
  UNKNOWN: 'unknown',
  UNSUPPORTED: 'unsupported',
};

/** @enum {string} */
chrome.fileManagerPrivate.MountContext = {
  USER: 'user',
  AUTO: 'auto',
};

/** @enum {string} */
chrome.fileManagerPrivate.MountCompletedEventType = {
  MOUNT: 'mount',
  UNMOUNT: 'unmount',
};

/** @enum {string} */
chrome.fileManagerPrivate.MountCompletedStatus = {
  SUCCESS: 'success',
  ERROR_UNKNOWN: 'error_unknown',
  ERROR_INTERNAL: 'error_internal',
  ERROR_INVALID_ARGUMENT: 'error_invalid_argument',
  ERROR_INVALID_PATH: 'error_invalid_path',
  ERROR_PATH_ALREADY_MOUNTED: 'error_path_already_mounted',
  ERROR_PATH_NOT_MOUNTED: 'error_path_not_mounted',
  ERROR_DIRECTORY_CREATION_FAILED: 'error_directory_creation_failed',
  ERROR_INVALID_MOUNT_OPTIONS: 'error_invalid_mount_options',
  ERROR_INVALID_UNMOUNT_OPTIONS: 'error_invalid_unmount_options',
  ERROR_INSUFFICIENT_PERMISSIONS: 'error_insufficient_permissions',
  ERROR_MOUNT_PROGRAM_NOT_FOUND: 'error_mount_program_not_found',
  ERROR_MOUNT_PROGRAM_FAILED: 'error_mount_program_failed',
  ERROR_INVALID_DEVICE_PATH: 'error_invalid_device_path',
  ERROR_UNKNOWN_FILESYSTEM: 'error_unknown_filesystem',
  ERROR_UNSUPPORTED_FILESYSTEM: 'error_unsupported_filesystem',
  ERROR_INVALID_ARCHIVE: 'error_invalid_archive',
  ERROR_AUTHENTICATION: 'error_authentication',
  ERROR_PATH_UNMOUNTED: 'error_path_unmounted',
};

/** @enum {string} */
chrome.fileManagerPrivate.TransferState = {
  IN_PROGRESS: 'in_progress',
  COMPLETED: 'completed',
  FAILED: 'failed',
};

/** @enum {string} */
chrome.fileManagerPrivate.CopyProgressStatusType = {
  BEGIN_COPY_ENTRY: 'begin_copy_entry',
  END_COPY_ENTRY: 'end_copy_entry',
  PROGRESS: 'progress',
  SUCCESS: 'success',
  ERROR: 'error',
};

/** @enum {string} */
chrome.fileManagerPrivate.FileWatchEventType = {
  CHANGED: 'changed',
  ERROR: 'error',
};

/** @enum {string} */
chrome.fileManagerPrivate.ChangeType = {
  ADD_OR_UPDATE: 'add_or_update',
  DELETE: 'delete',
};

/** @enum {string} */
chrome.fileManagerPrivate.SearchType = {
  EXCLUDE_DIRECTORIES: 'EXCLUDE_DIRECTORIES',
  SHARED_WITH_ME: 'SHARED_WITH_ME',
  OFFLINE: 'OFFLINE',
  ALL: 'ALL',
};

/** @enum {string} */
chrome.fileManagerPrivate.ZoomOperationType = {
  IN: 'in',
  OUT: 'out',
  RESET: 'reset',
};

/** @enum {string} */
chrome.fileManagerPrivate.InspectionType = {
  NORMAL: 'normal',
  CONSOLE: 'console',
  ELEMENT: 'element',
  BACKGROUND: 'background',
};

/** @enum {string} */
chrome.fileManagerPrivate.DeviceEventType = {
  DISABLED: 'disabled',
  REMOVED: 'removed',
  HARD_UNPLUGGED: 'hard_unplugged',
  FORMAT_START: 'format_start',
  FORMAT_SUCCESS: 'format_success',
  FORMAT_FAIL: 'format_fail',
  RENAME_START: 'rename_start',
  RENAME_SUCCESS: 'rename_success',
  RENAME_FAIL: 'rename_fail',
};

/** @enum {string} */
chrome.fileManagerPrivate.DriveSyncErrorType = {
  DELETE_WITHOUT_PERMISSION: 'delete_without_permission',
  SERVICE_UNAVAILABLE: 'service_unavailable',
  NO_SERVER_SPACE: 'no_server_space',
  MISC: 'misc',
};

/** @enum {string} */
chrome.fileManagerPrivate.TaskResult = {
  OPENED: 'opened',
  MESSAGE_SENT: 'message_sent',
  FAILED: 'failed',
  EMPTY: 'empty',
};

/** @enum {string} */
chrome.fileManagerPrivate.DriveShareType = {
  CAN_EDIT: 'can_edit',
  CAN_COMMENT: 'can_comment',
  CAN_VIEW: 'can_view',
};

/** @enum {string} */
chrome.fileManagerPrivate.EntryPropertyName = {
  SIZE: 'size',
  MODIFICATION_TIME: 'modificationTime',
  MODIFICATION_BY_ME_TIME: 'modificationByMeTime',
  THUMBNAIL_URL: 'thumbnailUrl',
  CROPPED_THUMBNAIL_URL: 'croppedThumbnailUrl',
  IMAGE_WIDTH: 'imageWidth',
  IMAGE_HEIGHT: 'imageHeight',
  IMAGE_ROTATION: 'imageRotation',
  PINNED: 'pinned',
  PRESENT: 'present',
  HOSTED: 'hosted',
  AVAILABLE_OFFLINE: 'availableOffline',
  AVAILABLE_WHEN_METERED: 'availableWhenMetered',
  DIRTY: 'dirty',
  CUSTOM_ICON_URL: 'customIconUrl',
  CONTENT_MIME_TYPE: 'contentMimeType',
  SHARED_WITH_ME: 'sharedWithMe',
  SHARED: 'shared',
  STARRED: 'starred',
  EXTERNAL_FILE_URL: 'externalFileUrl',
  ALTERNATE_URL: 'alternateUrl',
  SHARE_URL: 'shareUrl',
};

/** @enum {string} */
chrome.fileManagerPrivate.EntryTagVisibility = {
  PRIVATE: 'private',
  PUBLIC: 'public',
};

/** @enum {string} */
chrome.fileManagerPrivate.Source = {
  FILE: 'file',
  DEVICE: 'device',
  NETWORK: 'network',
  SYSTEM: 'system',
};

/** @enum {string} */
chrome.fileManagerPrivate.Verb = {
  OPEN_WITH: 'open_with',
  ADD_TO: 'add_to',
  PACK_WITH: 'pack_with',
  SHARE_WITH: 'share_with',
};

/** @enum {string} */
chrome.fileManagerPrivate.SourceRestriction = {
  ANY_SOURCE: 'any_source',
  NATIVE_SOURCE: 'native_source',
  NATIVE_OR_DRIVE_SOURCE: 'native_or_drive_source',
};

/** @enum {string} */
chrome.fileManagerPrivate.InstallLinuxPackageResponse = {
  STARTED: 'started',
  FAILED: 'failed',
  INSTALL_ALREADY_ACTIVE: 'install_already_active',
};

/**
 * @typedef {{
 *   taskId: string,
 *   title: string,
 *   verb: (!chrome.fileManagerPrivate.Verb|undefined),
 *   iconUrl: string,
 *   isDefault: boolean,
 *   isGenericFileHandler: boolean
 * }}
 */
chrome.fileManagerPrivate.FileTask;

/**
 * @typedef {{
 *   size: (number|undefined),
 *   modificationTime: (number|undefined),
 *   modificationByMeTime: (number|undefined),
 *   thumbnailUrl: (string|undefined),
 *   croppedThumbnailUrl: (string|undefined),
 *   imageWidth: (number|undefined),
 *   imageHeight: (number|undefined),
 *   imageRotation: (number|undefined),
 *   pinned: (boolean|undefined),
 *   present: (boolean|undefined),
 *   hosted: (boolean|undefined),
 *   availableOffline: (boolean|undefined),
 *   availableWhenMetered: (boolean|undefined),
 *   dirty: (boolean|undefined),
 *   customIconUrl: (string|undefined),
 *   contentMimeType: (string|undefined),
 *   sharedWithMe: (boolean|undefined),
 *   shared: (boolean|undefined),
 *   starred: (boolean|undefined),
 *   externalFileUrl: (string|undefined),
 *   alternateUrl: (string|undefined),
 *   shareUrl: (string|undefined),
 *   canCopy: (boolean|undefined),
 *   canDelete: (boolean|undefined),
 *   canRename: (boolean|undefined),
 *   canAddChildren: (boolean|undefined),
 *   canShare: (boolean|undefined)
 * }}
 */
chrome.fileManagerPrivate.EntryProperties;

/**
 * @typedef {{
 *   totalSize: number,
 *   remainingSize: number
 * }}
 */
chrome.fileManagerPrivate.MountPointSizeStats;

/**
 * @typedef {{
 *   profileId: string,
 *   displayName: string,
 *   isCurrentProfile: boolean
 * }}
 */
chrome.fileManagerPrivate.ProfileInfo;

/**
 * @typedef {{
 *   icon16x16Url: (string|undefined),
 *   icon32x32Url: (string|undefined)
 * }}
 */
chrome.fileManagerPrivate.IconSet;

/**
 * @typedef {{
 *   volumeId: string,
 *   fileSystemId: (string|undefined),
 *   providerId: (string|undefined),
 *   source: !chrome.fileManagerPrivate.Source,
 *   volumeLabel: (string|undefined),
 *   profile: !chrome.fileManagerPrivate.ProfileInfo,
 *   sourcePath: (string|undefined),
 *   volumeType: !chrome.fileManagerPrivate.VolumeType,
 *   deviceType: (!chrome.fileManagerPrivate.DeviceType|undefined),
 *   devicePath: (string|undefined),
 *   isParentDevice: (boolean|undefined),
 *   isReadOnly: boolean,
 *   isReadOnlyRemovableDevice: boolean,
 *   hasMedia: boolean,
 *   configurable: boolean,
 *   watchable: boolean,
 *   mountCondition: (!chrome.fileManagerPrivate.MountCondition|undefined),
 *   mountContext: (!chrome.fileManagerPrivate.MountContext|undefined),
 *   diskFileSystemType: (string|undefined),
 *   iconSet: !chrome.fileManagerPrivate.IconSet
 * }}
 */
chrome.fileManagerPrivate.VolumeMetadata;

/**
 * @typedef {{
 *   eventType: !chrome.fileManagerPrivate.MountCompletedEventType,
 *   status: !chrome.fileManagerPrivate.MountCompletedStatus,
 *   volumeMetadata: !chrome.fileManagerPrivate.VolumeMetadata,
 *   shouldNotify: boolean
 * }}
 */
chrome.fileManagerPrivate.MountCompletedEvent;

/**
 * @typedef {{
 *   fileUrl: string,
 *   transferState: !chrome.fileManagerPrivate.TransferState,
 *   processed: number,
 *   total: number,
 *   num_total_jobs: number,
 *   hideWhenZeroJobs: boolean
 * }}
 */
chrome.fileManagerPrivate.FileTransferStatus;

/**
 * @typedef {{
 *   type: !chrome.fileManagerPrivate.DriveSyncErrorType,
 *   fileUrl: string
 * }}
 */
chrome.fileManagerPrivate.DriveSyncErrorEvent;

/**
 * @typedef {{
 *   type: !chrome.fileManagerPrivate.CopyProgressStatusType,
 *   sourceUrl: (string|undefined),
 *   destinationUrl: (string|undefined),
 *   size: (number|undefined),
 *   error: (string|undefined)
 * }}
 */
chrome.fileManagerPrivate.CopyProgressStatus;

/**
 * @typedef {{
 *   url: string,
 *   changes: !Array<!chrome.fileManagerPrivate.ChangeType>
 * }}
 */
chrome.fileManagerPrivate.FileChange;

/**
 * @typedef {{
 *   eventType: !chrome.fileManagerPrivate.FileWatchEventType,
 *   entry: Object,
 *   changedFiles: (!Array<!chrome.fileManagerPrivate.FileChange>|undefined)
 * }}
 */
chrome.fileManagerPrivate.FileWatchEvent;

/**
 * @typedef {{
 *   driveEnabled: boolean,
 *   cellularDisabled: boolean,
 *   hostedFilesDisabled: boolean,
 *   searchSuggestEnabled: boolean,
 *   use24hourClock: boolean,
 *   allowRedeemOffers: boolean,
 *   timezone: string
 * }}
 */
chrome.fileManagerPrivate.Preferences;

/**
 * @typedef {{
 *   cellularDisabled: (boolean|undefined),
 *   hostedFilesDisabled: (boolean|undefined)
 * }}
 */
chrome.fileManagerPrivate.PreferencesChange;

/**
 * @typedef {{
 *   query: string,
 *   nextFeed: string
 * }}
 */
chrome.fileManagerPrivate.SearchParams;

/**
 * @typedef {{
 *   query: string,
 *   types: string,
 *   maxResults: number
 * }}
 */
chrome.fileManagerPrivate.SearchMetadataParams;

/**
 * @typedef {{
 *   entry: Entry,
 *   highlightedBaseName: string
 * }}
 */
chrome.fileManagerPrivate.SearchResult;

/**
 * @typedef {{
 *   type: string,
 *   reason: (string|undefined),
 *   hasCellularNetworkAccess: boolean
 * }}
 */
chrome.fileManagerPrivate.DriveConnectionState;

/**
 * @typedef {{
 *   type: !chrome.fileManagerPrivate.DeviceEventType,
 *   devicePath: string
 * }}
 */
chrome.fileManagerPrivate.DeviceEvent;

/**
 * @typedef {{
 *   providerId: string,
 *   iconSet: !chrome.fileManagerPrivate.IconSet,
 *   name: string,
 *   configurable: boolean,
 *   watchable: boolean,
 *   multipleMounts: boolean,
 *   source: string
 * }}
 */
chrome.fileManagerPrivate.Provider;

/**
 * Logout the current user for navigating to the re-authentication screen for
 * the Google account.
 */
chrome.fileManagerPrivate.logoutUserForReauthentication = function() {};

/**
 * Cancels file selection.
 */
chrome.fileManagerPrivate.cancelDialog = function() {};

/**
 * Executes file browser task over selected files. |taskId| The unique
 * identifier of task to execute. |entries| Array of file entries |callback|
 * @param {string} taskId
 * @param {!Array<!Entry>} entries
 * @param {function((boolean|undefined))} callback |result| Result of the task
 *     execution.
 */
chrome.fileManagerPrivate.executeTask = function(taskId, entries, callback) {};

/**
 * Sets the default task for the supplied MIME types and path extensions.
 * Lists of MIME types and entries may contain duplicates.
 * |taskId| The unique identifier of task to mark as default. |entries| Array
 * of selected file entries to extract path extensions from. |mimeTypes| Array
 * of selected file MIME types. |callback|
 * @param {string} taskId
 * @param {!Array<!Entry>} entries
 * @param {!Array<string>} mimeTypes
 * @param {!function()} callback Callback that does not take arguments.
 */
chrome.fileManagerPrivate.setDefaultTask = function(taskId, entries, mimeTypes,
    callback) {};

/**
 * Gets the list of tasks that can be performed over selected files. |entries|
 * Array of selected entries |callback|
 * @param {!Array<!Entry>} entries
 * @param {function((!Array<!chrome.fileManagerPrivate.FileTask>|undefined))} callback |tasks| The list of
 *     matched file entries for this task.
 */
chrome.fileManagerPrivate.getFileTasks = function(entries, callback) {};

/**
 * Gets the MIME type of a file.
 * @param {!Entry} entry
 * @param {function((string|undefined))} callback Callback that MIME type of the
 *     file is passed.
 */
chrome.fileManagerPrivate.getMimeType = function(entry, callback) {};

/**
 * Gets localized strings and initialization data. |callback|
 * @param {function((!Object|undefined))} callback |result| Hash containing the
 *     string assets.
 */
chrome.fileManagerPrivate.getStrings = function(callback) {};

/**
 * Adds file watch. |entry| Entry of file to watch |callback|
 * @param {!Entry} entry
 * @param {function((boolean|undefined))} callback |success| True when file
 *     watch is successfully added.
 */
chrome.fileManagerPrivate.addFileWatch = function(entry, callback) {};

/**
 * Removes file watch. |entry| Entry of watched file to remove |callback|
 * @param {!Entry} entry
 * @param {function((boolean|undefined))} callback |success| True when file
 *     watch is successfully
 * removed.
 */
chrome.fileManagerPrivate.removeFileWatch = function(entry, callback) {};

/**
 * Enables the extenal file scheme necessary to initiate drags to the browser
 * window for files on the external backend.
 */
chrome.fileManagerPrivate.enableExternalFileScheme = function() {};

/**
 * Requests R/W access to the specified entries as |entryUrls|. Note, that only
 * files backed by external file system backend will be granted the access.
 * @param {!Array<string>} entryUrls
 * @param {function()} callback Completion callback.
 */
chrome.fileManagerPrivate.grantAccess = function(entryUrls, callback) {};

/**
 * Selects multiple files. |selectedPaths| Array of selected paths
 * |shouldReturnLocalPath| true if paths need to be resolved to local paths.
 * |callback|
 * @param {!Array<string>} selectedPaths
 * @param {boolean} shouldReturnLocalPath
 * @param {function()} callback Callback that does not take arguments.
 */
chrome.fileManagerPrivate.selectFiles = function(selectedPaths,
    shouldReturnLocalPath, callback) {};

/**
 * Selects a file. |selectedPath| A selected path |index| Index of Filter
 * |forOpening| true if paths are selected for opening. false if for saving.
 * |shouldReturnLocalPath| true if paths need to be resolved to local paths.
 * |callback|
 * @param {string} selectedPath
 * @param {number} index
 * @param {boolean} forOpening
 * @param {boolean} shouldReturnLocalPath
 * @param {function()} callback Callback that does not take arguments.
 */
chrome.fileManagerPrivate.selectFile = function(selectedPath, index, forOpening,
    shouldReturnLocalPath, callback) {};

/**
 * Requests additional properties for files. |entries| list of entries of files
 * |callback|
 * @param {!Array<!Entry>} entries
 * @param {!Array<string>} names
 * @param {function((!Array<!chrome.fileManagerPrivate.EntryProperties>|undefined))} callback
 *     |entryProperties| A dictionary containing properties of the requested
 *     entries.
 */
chrome.fileManagerPrivate.getEntryProperties = function(entries, names,
    callback) {};

/**
 * Pins/unpins a Drive file in the cache. |entry| Entry of a file to pin/unpin.
 * |pin| Pass true to pin the file. |callback| Completion callback.
 * $(ref:runtime.lastError) will be set if     there was an error.
 * @param {!Entry} entry
 * @param {boolean} pin
 * @param {function()} callback Callback that does not take arguments.
 */
chrome.fileManagerPrivate.pinDriveFile = function(entry, pin, callback) {};

/**
 * If |entry| is a Drive file, ensures the file is downloaded to the cache.
 * Otherwise, finishes immediately in success. For example, when the file is
 * under Downloads, MTP, removeable media, or provided by extensions for
 * other cloud storage services than Google Drive, this does nothing.
 * This is a workaround to avoid intermittent and duplicated downloading of
 * a Drive file by current implementation of Drive integration when an
 * extension reads a file sequentially but intermittently.
 * @param {!Entry} entry A regular file entry to be read.
 * @param {function()} callback Callback called after having the file in cache.
 *     runtime.lastError will be set if there was an error.
 */
chrome.fileManagerPrivate.ensureFileDownloaded = function(entry, callback) {};

/**
 * Resolves file entries in the isolated file system and returns corresponding
 * entries in the external file system mounted to Chrome OS file manager
 * backend. If resolving entry fails, the entry will be just ignored and the
 * corresponding entry does not appear in the result.
 * @param {!Array<!Entry>} entries
 * @param {function((!Array<!Entry>|undefined))} callback Completion callback
 *     with resolved entries.
 */
chrome.fileManagerPrivate.resolveIsolatedEntries = function(entries,
    callback) {};

/**
 * Mount a resource or a file. |source| Mount point source. For compressed
 * files it is relative file path     within external file system |callback|
 * @param {string} source
 * @param {function((string|undefined))} callback Callback with source path of
 *     the mount.
 */
chrome.fileManagerPrivate.addMount = function(source, callback) {};

/**
 * Unmounts a mounted resource. |volumeId| An ID of the volume.
 * @param {string} volumeId
 */
chrome.fileManagerPrivate.removeMount = function(volumeId) {};

/**
 * Marks a cache file of Drive as mounted or unmounted.
 * Does nothing if the file is not under Drive directory.
 * @param {string} sourcePath Mounted source file. Relative file path within
 *     external file system.
 * @param {boolean} isMounted Mark as mounted if true. Mark as unmounted
 *     otherwise.
 * @param {function()} callback Completion callback. runtime.lastError will be
 *     set if there was an error.
 */
chrome.fileManagerPrivate.markCacheAsMounted = function(
    sourcePath, isMounted, callback) {};

/**
 * Get the list of mounted volumes. |callback|
 * @param {function((!Array<!chrome.fileManagerPrivate.VolumeMetadata>|undefined))} callback Callback with
 *     the list of chrome.fileManagerPrivate.VolumeMetadata representing mounted
 * volumes.
 */
chrome.fileManagerPrivate.getVolumeMetadataList = function(callback) {};

/**
 * Cancels ongoing file transfers for selected files. |entries| Array of files
 * for which ongoing transfer should be canceled.
 * @param {!Array<!FileEntry>} entries
 * @param {function()} callback
 */
chrome.fileManagerPrivate.cancelFileTransfers = function(entries, callback) {};

/**
 * Starts to copy an entry. If the source is a directory, the copy is done
 * recursively. |entry| Entry of the source entry to be copied. |parent| Entry
 * of the destination directory. |newName| Name of the new entry. It must not
 * contain '/'. |callback| Completion callback.
 * @param {!Entry} entry
 * @param {!DirectoryEntry} parentEntry
 * @param {string} newName
 * @param {function((number|undefined))} callback |copyId| ID of the copy task.
 *     Can be used to identify the progress, and to cancel the task.
 */
chrome.fileManagerPrivate.startCopy = function(entry, parentEntry, newName,
    callback) {};

/**
 * Cancels the running copy task. |copyId| ID of the copy task to be cancelled.
 * |callback| Completion callback of the cancel.
 * @param {number} copyId
 * @param {function()} callback Callback that does not take arguments.
 */
chrome.fileManagerPrivate.cancelCopy = function(copyId, callback) {};

/**
 * Retrieves total and remaining size of a mount point. |volumeId| ID of the
 * volume to be checked. |callback|
 * @param {string} volumeId
 * @param {function((!chrome.fileManagerPrivate.MountPointSizeStats|undefined))} callback Name/value pairs
 *     of size stats. Will be undefined if stats could not be determined.
 */
chrome.fileManagerPrivate.getSizeStats = function(volumeId, callback) {};

/**
 * Formats a mounted volume. |volumeId| ID of the volume to be formatted.
 * @param {string} volumeId
 */
chrome.fileManagerPrivate.formatVolume = function(volumeId) {};

/**
 * Renames a mounted volume. |volumeId| ID of the volume to be renamed to
 * |newName|.
 * @param {string} volumeId
 * @param {string} newName
 */
chrome.fileManagerPrivate.renameVolume = function(volumeId, newName) {};

/**
 * Retrieves file manager preferences. |callback|
 * @param {function((!chrome.fileManagerPrivate.Preferences|undefined))} callback
 */
chrome.fileManagerPrivate.getPreferences = function(callback) {};

/**
 * Sets file manager preferences. |changeInfo|
 * @param {chrome.fileManagerPrivate.PreferencesChange} changeInfo
 */
chrome.fileManagerPrivate.setPreferences = function(changeInfo) {};

/**
 * Performs drive content search. |searchParams| |callback|
 * @param {chrome.fileManagerPrivate.SearchParams} searchParams
 * @param {function((!Array<Entry>|undefined), (string|undefined))} callback
 * Entries and ID of the feed that contains next chunk of the search result.
 * Should be sent to the next searchDrive request to perform incremental search.
 */
chrome.fileManagerPrivate.searchDrive = function(searchParams, callback) {};

/**
 * Performs drive metadata search. |searchParams| |callback|
 * @param {chrome.fileManagerPrivate.SearchMetadataParams} searchParams
 * @param {function((!Array<!chrome.fileManagerPrivate.SearchResult>|undefined))} callback
 */
chrome.fileManagerPrivate.searchDriveMetadata = function(searchParams,
    callback) {};

/**
 * Search for files in the given volume, whose content hash matches the list of
 * given hashes.
 * @param {string} volumeId
 * @param {!Array<string>} hashes
 * @param {function((!Object<string, !Array<string>>|undefined))} callback
 */
chrome.fileManagerPrivate.searchFilesByHashes = function(volumeId, hashes,
    callback) {};

/**
 * Create a zip file for the selected files. |parentEntry| Entry of the
 * directory containing the selected files. |entries| Selected entries.
 * The files must be under the directory specified by |parentEntry|. |destName|
 * Name of the destination zip file. The zip file will be created under the
 * directory specified by |parentEntry|.
 * @param {!Array<!Entry>} entries
 * @param {!DirectoryEntry} parentEntry
 * @param {string} destName
 * @param {function((boolean|undefined))} callback
 */
chrome.fileManagerPrivate.zipSelection = function(entries, parentEntry,
    destName, callback) {};

/**
 * Retrieves the state of the current drive connection. |callback|
 * @param {function((!chrome.fileManagerPrivate.DriveConnectionState|undefined))} callback
 */
chrome.fileManagerPrivate.getDriveConnectionState = function(callback) {};

/**
 * Checks whether the path name length fits in the limit of the filesystem.
 * |parentEntry| The parent directory entry. |name| The name of the file.
 * |callback| Called back when the check is finished.
 * @param {!DirectoryEntry} parentEntry
 * @param {string} name
 * @param {function((boolean|undefined))} callback |result| true if the length
 *     is in the valid range, false otherwise.
 */
chrome.fileManagerPrivate.validatePathNameLength = function(
    parentEntry, name, callback) {};

/**
 * Changes the zoom factor of the Files app. |operation| Zooming mode.
 * @param {string} operation
 */
chrome.fileManagerPrivate.zoom = function(operation) {};

/**
 * Requests a Drive API OAuth2 access token. |refresh| Whether the token should
 * be refetched instead of using the cached     one. |callback|
 * @param {boolean} refresh
 * @param {function((string|undefined))} callback |accessToken| OAuth2 access
 *     token, or an empty string if failed to fetch.
 */
chrome.fileManagerPrivate.requestAccessToken = function(refresh, callback) {};

/**
 * Requests a Webstore API OAuth2 access token. |callback|
 * @param {function((string|undefined))} callback |accessToken| OAuth2 access
 *     token, or an empty string if failed to fetch.
 */
chrome.fileManagerPrivate.requestWebStoreAccessToken = function(callback) {};

/**
 * Requests a download url to download the file contents.
 * @param {!Entry} entry
 * @param {function((string|undefined))} callback Callback with the result url.
 */
chrome.fileManagerPrivate.getDownloadUrl = function(entry, callback) {};

/**
 * Requests to share drive files.
 * @param {!Entry} entry
 * @param {string} shareType
 * @param {function()} callback Callback that does not take arguments.
 */
chrome.fileManagerPrivate.requestDriveShare = function(entry, shareType,
    callback) {};

/**
 * Obtains a list of profiles that are logged-in.
 * @param {function((!Array<!chrome.fileManagerPrivate.ProfileInfo>|undefined), (string|undefined),
 *     (string|undefined))} callback Callback with list of profile information,
 *     |runningProfile| ID of the profile that runs the application instance.
 *     |showingProfile| ID of the profile that shows the application window.
 */
chrome.fileManagerPrivate.getProfiles = function(callback) {};

/**
 * Opens inspector window. |type| InspectionType which specifies how to open
 * inspector.
 * @param {string} type
 */
chrome.fileManagerPrivate.openInspector = function(type) {};

/**
 * Opens settings sub page. |sub_page| Name of a sub page.
 * @param {string} sub_page
 */
chrome.fileManagerPrivate.openSettingsSubpage = function(sub_page) {};

/**
 * Computes an MD5 checksum for the given file.
 * @param {!Entry} entry
 * @param {function((string|undefined))} callback
 */
chrome.fileManagerPrivate.computeChecksum = function(entry, callback) {};

/**
 * Gets a flag indicating whether user metrics reporting is enabled.
 * @param {function((boolean|undefined))} callback
 */
chrome.fileManagerPrivate.isUMAEnabled = function(callback) {};

/**
 * Sets a tag on a file or a directory. Only Drive files are supported.
 * @param {!Entry} entry
 * @param {string} visibility 'private' or 'public'
 * @param {string} key
 * @param {string} value
 * @param {function()} callback
 */
chrome.fileManagerPrivate.setEntryTag = function(entry, visibility, key,
    value, callback) {};

/**
 * Gets a flag indicating whether PiexLoader is enabled.
 * @param {function((boolean|undefined))} callback
 */
chrome.fileManagerPrivate.isPiexLoaderEnabled = function(callback) {};

/**
 * Returns list of available providers.
 * @param {function((!Array<!chrome.fileManagerPrivate.Provider>|undefined))} callback
 */
chrome.fileManagerPrivate.getProviders = function(callback) {};

/**
 * Requests adding a new provided file system. If not possible, then an error
 * via chrome.runtime.lastError is returned.
 * @param {string} providerId
 * @param {function()} callback
 */
chrome.fileManagerPrivate.addProvidedFileSystem =
    function(providerId, callback) {};

/**
 * Requests configuring an existing file system. If not possible, then returns
 * an error via chrome.runtime.lastError.
 * @param {string} volumeId
 * @param {function()} callback
 */
chrome.fileManagerPrivate.configureVolume = function(volumeId, callback) {};

/**
 * Requests fetching list of actions for the specified set of entries. If not
 * possible, then returns an error via chrome.runtime.lastError.
 * @param {!Array<!Entry>} entries
 * @param {function((!Array<!chrome.fileSystemProvider.Action>|undefined))} callback
 */
chrome.fileManagerPrivate.getCustomActions = function(entries, callback) {};

/**
 * Executes the action on the specified set of entries. If not possible, then
 * returns an error via chrome.runtime.lastError.
 * @param {!Array<!Entry>} entries
 * @param {string} actionId
 * @param {function()} callback
 */
chrome.fileManagerPrivate.executeCustomAction = function(
    entries, actionId, callback) {};

/**
 * Get the total size of a directory. |entry| Entry of the target directory.
 * |callback|
 * @param {!DirectoryEntry} entry
 * @param {function(number)} callback
 */
chrome.fileManagerPrivate.getDirectorySize = function(entry, callback) {};

/**
 * Gets recently modified files across file systems.
 * @param {string} restriction
 * @param {function((!Array<!FileEntry>))} callback
 */
chrome.fileManagerPrivate.getRecentFiles = function(restriction, callback) {};

/**
 * Returns true if crostini is enabled.
 * @param {function(boolean)} callback
 */
chrome.fileManagerPrivate.isCrostiniEnabled = function(callback) {};

/**
 * Starts and mounts crostini container.
 * @param {function()} callback Callback called after the crostini container
 *     is started and mounted.
 *     chrome.runtime.lastError will be set if there was an error.
 */
chrome.fileManagerPrivate.mountCrostini = function(callback) {};

/**
 * Shares directory with crostini container.
 * @param {!DirectoryEntry} entry Entry of the directory to share.
 * @param {function()} callback Callback called after the folder is shared.
 *     chrome.runtime.lastError will be set if there was an error.
 */
chrome.fileManagerPrivate.sharePathWithCrostini = function(
    entry, callback) {};

/**
 * Begin installation of a Linux package.
 * @param {!Entry} entry
 * @param {function(!chrome.fileManagerPrivate.InstallLinuxPackageResponse,
 *    string)} callback
 *    Called when the installation is either started or fails to start.
 */
chrome.fileManagerPrivate.installLinuxPackage = function(entry, callback) {};

/**
 * For a file in DriveFS, retrieves its thumbnail. If |cropToSquare| is true,
 * returns a thumbnail appropriate for file list or grid views; otherwise,
 * returns a thumbnail appropriate for quickview.
 * @param {Object} entry
 * @param {boolean} cropToSquare
 * @param {function(string):void} callback |thumbnailDataUrl| A data URL for the
 *     thumbnail as a PNG; |thumbnailDataUrl| is empty if no thumbnail was
 *     available.
 */
chrome.fileManagerPrivate.getThumbnail = function(entry, cropToSquare, callback) {};

/** @type {!ChromeEvent} */
chrome.fileManagerPrivate.onMountCompleted;

/** @type {!ChromeEvent} */
chrome.fileManagerPrivate.onFileTransfersUpdated;

/** @type {!ChromeEvent} */
chrome.fileManagerPrivate.onCopyProgress;

/** @type {!ChromeEvent} */
chrome.fileManagerPrivate.onDirectoryChanged;

/** @type {!ChromeEvent} */
chrome.fileManagerPrivate.onPreferencesChanged;

/** @type {!ChromeEvent} */
chrome.fileManagerPrivate.onDriveConnectionStatusChanged;

/** @type {!ChromeEvent} */
chrome.fileManagerPrivate.onDeviceChanged;

/** @type {!ChromeEvent} */
chrome.fileManagerPrivate.onDriveSyncError;

/** @type {!ChromeEvent} */
chrome.fileManagerPrivate.onAppsUpdated;
