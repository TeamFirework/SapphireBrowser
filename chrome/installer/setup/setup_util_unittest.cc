// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/setup_util_unittest.h"

#include <windows.h>
#include <shlobj.h>

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/updating_app_registration_data.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test that we are parsing Chrome version correctly.
TEST(SetupUtilTest, GetMaxVersionFromArchiveDirTest) {
  // Create a version dir
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  base::FilePath chrome_dir = test_dir.GetPath().AppendASCII("1.0.0.0");
  base::CreateDirectory(chrome_dir);
  ASSERT_TRUE(base::PathExists(chrome_dir));
  std::unique_ptr<base::Version> version(
      installer::GetMaxVersionFromArchiveDir(test_dir.GetPath()));
  ASSERT_EQ(version->GetString(), "1.0.0.0");

  base::DeleteFile(chrome_dir, true);
  ASSERT_FALSE(base::PathExists(chrome_dir)) << chrome_dir.value();
  ASSERT_TRUE(installer::GetMaxVersionFromArchiveDir(test_dir.GetPath()) ==
              NULL);

  chrome_dir = test_dir.GetPath().AppendASCII("ABC");
  base::CreateDirectory(chrome_dir);
  ASSERT_TRUE(base::PathExists(chrome_dir));
  ASSERT_TRUE(installer::GetMaxVersionFromArchiveDir(test_dir.GetPath()) ==
              NULL);

  chrome_dir = test_dir.GetPath().AppendASCII("2.3.4.5");
  base::CreateDirectory(chrome_dir);
  ASSERT_TRUE(base::PathExists(chrome_dir));
  version.reset(installer::GetMaxVersionFromArchiveDir(test_dir.GetPath()));
  ASSERT_EQ(version->GetString(), "2.3.4.5");

  // Create multiple version dirs, ensure that we select the greatest.
  chrome_dir = test_dir.GetPath().AppendASCII("9.9.9.9");
  base::CreateDirectory(chrome_dir);
  ASSERT_TRUE(base::PathExists(chrome_dir));
  chrome_dir = test_dir.GetPath().AppendASCII("1.1.1.1");
  base::CreateDirectory(chrome_dir);
  ASSERT_TRUE(base::PathExists(chrome_dir));

  version.reset(installer::GetMaxVersionFromArchiveDir(test_dir.GetPath()));
  ASSERT_EQ(version->GetString(), "9.9.9.9");
}

TEST(SetupUtilTest, DeleteFileFromTempProcess) {
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  base::FilePath test_file;
  base::CreateTemporaryFileInDir(test_dir.GetPath(), &test_file);
  ASSERT_TRUE(base::PathExists(test_file));
  base::WriteFile(test_file, "foo", 3);
  EXPECT_TRUE(installer::DeleteFileFromTempProcess(test_file, 0));
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout() * 3);
  EXPECT_FALSE(base::PathExists(test_file)) << test_file.value();
}

TEST(SetupUtilTest, RegisterEventLogProvider) {
  registry_util::RegistryOverrideManager registry_override_manager;
  ASSERT_NO_FATAL_FAILURE(
      registry_override_manager.OverrideRegistry(HKEY_LOCAL_MACHINE));

  const base::Version version("1.2.3.4");
  const base::FilePath install_directory(
      FILE_PATH_LITERAL("c:\\some_path\\test"));
  installer::RegisterEventLogProvider(install_directory, version);

  base::string16 reg_path(
      L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\");
  reg_path.append(install_static::InstallDetails::Get().install_full_name());
  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ));
  EXPECT_TRUE(key.HasValue(L"CategoryCount"));
  EXPECT_TRUE(key.HasValue(L"TypesSupported"));
  EXPECT_TRUE(key.HasValue(L"CategoryMessageFile"));
  EXPECT_TRUE(key.HasValue(L"EventMessageFile"));
  EXPECT_TRUE(key.HasValue(L"ParameterMessageFile"));
  base::string16 value;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"CategoryMessageFile", &value));
  const base::FilePath expected_directory(
      install_directory.AppendASCII(version.GetString()));
  const base::FilePath provider_path(value);
  EXPECT_EQ(expected_directory, provider_path.DirName());
  key.Close();

  installer::DeRegisterEventLogProvider();

  EXPECT_NE(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ));
}

const char kAdjustProcessPriority[] = "adjust-process-priority";

PriorityClassChangeResult DoProcessPriorityAdjustment() {
  return installer::AdjustProcessPriority() ? PCCR_CHANGED : PCCR_UNCHANGED;
}

namespace {

// A scoper that sets/resets the current process's priority class.
class ScopedPriorityClass {
 public:
  // Applies |priority_class|, returning an instance if a change was made.
  // Otherwise, returns an empty scoped_ptr.
  static std::unique_ptr<ScopedPriorityClass> Create(DWORD priority_class);
  ~ScopedPriorityClass();

 private:
  explicit ScopedPriorityClass(DWORD original_priority_class);
  DWORD original_priority_class_;
  DISALLOW_COPY_AND_ASSIGN(ScopedPriorityClass);
};

std::unique_ptr<ScopedPriorityClass> ScopedPriorityClass::Create(
    DWORD priority_class) {
  HANDLE this_process = ::GetCurrentProcess();
  DWORD original_priority_class = ::GetPriorityClass(this_process);
  EXPECT_NE(0U, original_priority_class);
  if (original_priority_class && original_priority_class != priority_class) {
    BOOL result = ::SetPriorityClass(this_process, priority_class);
    EXPECT_NE(FALSE, result);
    if (result) {
      return std::unique_ptr<ScopedPriorityClass>(
          new ScopedPriorityClass(original_priority_class));
    }
  }
  return std::unique_ptr<ScopedPriorityClass>();
}

ScopedPriorityClass::ScopedPriorityClass(DWORD original_priority_class)
    : original_priority_class_(original_priority_class) {}

ScopedPriorityClass::~ScopedPriorityClass() {
  BOOL result = ::SetPriorityClass(::GetCurrentProcess(),
                                   original_priority_class_);
  EXPECT_NE(FALSE, result);
}

PriorityClassChangeResult RelaunchAndDoProcessPriorityAdjustment() {
  base::CommandLine cmd_line(*base::CommandLine::ForCurrentProcess());
  cmd_line.AppendSwitch(kAdjustProcessPriority);
  base::Process process = base::LaunchProcess(cmd_line, base::LaunchOptions());
  int exit_code = 0;
  if (!process.IsValid()) {
    ADD_FAILURE() << " to launch subprocess.";
  } else if (!process.WaitForExit(&exit_code)) {
    ADD_FAILURE() << " to wait for subprocess to exit.";
  } else {
    return static_cast<PriorityClassChangeResult>(exit_code);
  }
  return PCCR_UNKNOWN;
}

}  // namespace

// Launching a subprocess at normal priority class is a noop.
TEST(SetupUtilTest, AdjustFromNormalPriority) {
  ASSERT_EQ(static_cast<DWORD>(NORMAL_PRIORITY_CLASS),
            ::GetPriorityClass(::GetCurrentProcess()));
  EXPECT_EQ(PCCR_UNCHANGED, RelaunchAndDoProcessPriorityAdjustment());
}

// Launching a subprocess below normal priority class drops it to bg mode for
// sufficiently recent operating systems.
TEST(SetupUtilTest, AdjustFromBelowNormalPriority) {
  std::unique_ptr<ScopedPriorityClass> below_normal =
      ScopedPriorityClass::Create(BELOW_NORMAL_PRIORITY_CLASS);
  ASSERT_TRUE(below_normal);
  EXPECT_EQ(PCCR_CHANGED, RelaunchAndDoProcessPriorityAdjustment());
}

TEST(SetupUtilTest, GetInstallAge) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  installer::InstallerState installer_state;
  installer_state.set_target_path_for_testing(temp_dir.GetPath());

  // Wait a beat to let time advance.
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_GE(0, installer::GetInstallAge(installer_state));

  // Crank back the directory's creation time.
  constexpr int kAgeDays = 28;
  base::Time now = base::Time::Now();
  base::win::ScopedHandle dir(::CreateFile(
      temp_dir.GetPath().value().c_str(),
      FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
  ASSERT_TRUE(dir.IsValid());

  FILE_BASIC_INFO info = {};
  ASSERT_NE(0, ::GetFileInformationByHandleEx(dir.Get(), FileBasicInfo, &info,
                                              sizeof(info)));
  FILETIME creation_time =
      (now - base::TimeDelta::FromDays(kAgeDays)).ToFileTime();
  info.CreationTime.u.LowPart = creation_time.dwLowDateTime;
  info.CreationTime.u.HighPart = creation_time.dwHighDateTime;
  ASSERT_NE(0, ::SetFileInformationByHandle(dir.Get(), FileBasicInfo, &info,
                                            sizeof(info)));

  EXPECT_EQ(kAgeDays, installer::GetInstallAge(installer_state));
}

TEST(SetupUtilTest, RecordUnPackMetricsTest) {
  base::HistogramTester histogram_tester;
  std::string unpack_status_metrics_name =
      std::string(installer::kUnPackStatusMetricsName) + "_SetupExePatch";
  std::string ntstatus_metrics_name =
      std::string(installer::kUnPackNTSTATUSMetricsName) + "_SetupExePatch";
  histogram_tester.ExpectTotalCount(unpack_status_metrics_name, 0);

  RecordUnPackMetrics(UnPackStatus::UNPACK_NO_ERROR, 0,
                      installer::UnPackConsumer::SETUP_EXE_PATCH);
  histogram_tester.ExpectTotalCount(unpack_status_metrics_name, 1);
  histogram_tester.ExpectBucketCount(unpack_status_metrics_name, 0, 1);
  histogram_tester.ExpectTotalCount(ntstatus_metrics_name, 1);
  histogram_tester.ExpectBucketCount(ntstatus_metrics_name, 0, 1);

  RecordUnPackMetrics(UnPackStatus::UNPACK_CLOSE_FILE_ERROR, 1,
                      installer::UnPackConsumer::SETUP_EXE_PATCH);
  histogram_tester.ExpectTotalCount(unpack_status_metrics_name, 2);
  histogram_tester.ExpectBucketCount(unpack_status_metrics_name, 10, 1);
  histogram_tester.ExpectTotalCount(ntstatus_metrics_name, 2);
  histogram_tester.ExpectBucketCount(ntstatus_metrics_name, 1, 1);
}

namespace {

// A test fixture that configures an InstallationState and an InstallerState
// with a product being updated.
class FindArchiveToPatchTest : public testing::Test {
 protected:
  class FakeInstallationState : public installer::InstallationState {
  };

  class FakeProductState : public installer::ProductState {
   public:
    static FakeProductState* FromProductState(const ProductState* product) {
      return static_cast<FakeProductState*>(const_cast<ProductState*>(product));
    }

    void set_version(const base::Version& version) {
      if (version.IsValid())
        version_.reset(new base::Version(version));
      else
        version_.reset();
    }

    void set_uninstall_command(const base::CommandLine& uninstall_command) {
      uninstall_command_ = uninstall_command;
    }
  };

  FindArchiveToPatchTest() {}

  void SetUp() override {
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    product_version_ = base::Version("30.0.1559.0");
    max_version_ = base::Version("47.0.1559.0");

    // Install the product according to the version.
    original_state_.reset(new FakeInstallationState());
    InstallProduct();

    // Prepare to update the product in the temp dir.
    installer_state_.reset(new installer::InstallerState(
        kSystemInstall_ ? installer::InstallerState::SYSTEM_LEVEL :
        installer::InstallerState::USER_LEVEL));
    installer_state_->AddProductFromState(
        *original_state_->GetProductState(kSystemInstall_));

    // Create archives in the two version dirs.
    ASSERT_TRUE(
        base::CreateDirectory(GetProductVersionArchivePath().DirName()));
    ASSERT_EQ(1, base::WriteFile(GetProductVersionArchivePath(), "a", 1));
    ASSERT_TRUE(
        base::CreateDirectory(GetMaxVersionArchivePath().DirName()));
    ASSERT_EQ(1, base::WriteFile(GetMaxVersionArchivePath(), "b", 1));
  }

  void TearDown() override {
    original_state_.reset();
  }

  base::FilePath GetArchivePath(const base::Version& version) const {
    return test_dir_.GetPath()
        .AppendASCII(version.GetString())
        .Append(installer::kInstallerDir)
        .Append(installer::kChromeArchive);
  }

  base::FilePath GetMaxVersionArchivePath() const {
    return GetArchivePath(max_version_);
  }

  base::FilePath GetProductVersionArchivePath() const {
    return GetArchivePath(product_version_);
  }

  void InstallProduct() {
    FakeProductState* product = FakeProductState::FromProductState(
        original_state_->GetNonVersionedProductState(kSystemInstall_));

    product->set_version(product_version_);
    base::CommandLine uninstall_command(
        test_dir_.GetPath()
            .AppendASCII(product_version_.GetString())
            .Append(installer::kInstallerDir)
            .Append(installer::kSetupExe));
    uninstall_command.AppendSwitch(installer::switches::kUninstall);
    product->set_uninstall_command(uninstall_command);
  }

  void UninstallProduct() {
    FakeProductState::FromProductState(
        original_state_->GetNonVersionedProductState(kSystemInstall_))
        ->set_version(base::Version());
  }

  static const bool kSystemInstall_;
  base::ScopedTempDir test_dir_;
  base::Version product_version_;
  base::Version max_version_;
  std::unique_ptr<FakeInstallationState> original_state_;
  std::unique_ptr<installer::InstallerState> installer_state_;

 private:
  registry_util::RegistryOverrideManager registry_override_manager_;

  DISALLOW_COPY_AND_ASSIGN(FindArchiveToPatchTest);
};

const bool FindArchiveToPatchTest::kSystemInstall_ = false;

}  // namespace

// Test that the path to the advertised product version is found.
TEST_F(FindArchiveToPatchTest, ProductVersionFound) {
  base::FilePath patch_source(installer::FindArchiveToPatch(
      *original_state_, *installer_state_, base::Version()));
  EXPECT_EQ(GetProductVersionArchivePath().value(), patch_source.value());
}

// Test that the path to the max version is found if the advertised version is
// missing.
TEST_F(FindArchiveToPatchTest, MaxVersionFound) {
  // The patch file is absent.
  ASSERT_TRUE(base::DeleteFile(GetProductVersionArchivePath(), false));
  base::FilePath patch_source(installer::FindArchiveToPatch(
      *original_state_, *installer_state_, base::Version()));
  EXPECT_EQ(GetMaxVersionArchivePath().value(), patch_source.value());

  // The product doesn't appear to be installed, so the max version is found.
  UninstallProduct();
  patch_source = installer::FindArchiveToPatch(
      *original_state_, *installer_state_, base::Version());
  EXPECT_EQ(GetMaxVersionArchivePath().value(), patch_source.value());
}

// Test that an empty path is returned if no version is found.
TEST_F(FindArchiveToPatchTest, NoVersionFound) {
  // The product doesn't appear to be installed and no archives are present.
  UninstallProduct();
  ASSERT_TRUE(base::DeleteFile(GetProductVersionArchivePath(), false));
  ASSERT_TRUE(base::DeleteFile(GetMaxVersionArchivePath(), false));

  base::FilePath patch_source(installer::FindArchiveToPatch(
      *original_state_, *installer_state_, base::Version()));
  EXPECT_EQ(base::FilePath::StringType(), patch_source.value());
}

TEST_F(FindArchiveToPatchTest, DesiredVersionFound) {
  base::FilePath patch_source1(installer::FindArchiveToPatch(
    *original_state_, *installer_state_, product_version_));
  EXPECT_EQ(GetProductVersionArchivePath().value(), patch_source1.value());
  base::FilePath patch_source2(installer::FindArchiveToPatch(
    *original_state_, *installer_state_, max_version_));
  EXPECT_EQ(GetMaxVersionArchivePath().value(), patch_source2.value());
}

TEST_F(FindArchiveToPatchTest, DesiredVersionNotFound) {
  base::FilePath patch_source(installer::FindArchiveToPatch(
    *original_state_, *installer_state_, base::Version("1.2.3.4")));
  EXPECT_EQ(base::FilePath().value(), patch_source.value());
}

TEST(SetupUtilTest, ContainsUnsupportedSwitch) {
  EXPECT_FALSE(installer::ContainsUnsupportedSwitch(
      base::CommandLine::FromString(L"foo.exe")));
  EXPECT_FALSE(installer::ContainsUnsupportedSwitch(
      base::CommandLine::FromString(L"foo.exe --multi-install --chrome")));
  EXPECT_TRUE(installer::ContainsUnsupportedSwitch(
      base::CommandLine::FromString(L"foo.exe --chrome-frame")));
}

TEST(SetupUtilTest, GetRegistrationDataCommandKey) {
  const base::string16 key = installer::GetCommandKey(L"test_name");
  EXPECT_TRUE(base::EndsWith(key, L"\\Commands\\test_name",
                             base::CompareCase::SENSITIVE));
}

TEST(SetupUtilTest, GetConsoleSessionStartTime) {
  base::Time start_time = installer::GetConsoleSessionStartTime();
  EXPECT_FALSE(start_time.is_null());
}

TEST(SetupUtilTest, DecodeDMTokenSwitchValue) {
  // Expect false with empty or badly formed base64-encoded string.
  EXPECT_FALSE(installer::DecodeDMTokenSwitchValue(L""));
  EXPECT_FALSE(installer::DecodeDMTokenSwitchValue(L"not-ascii\xff"));
  EXPECT_FALSE(installer::DecodeDMTokenSwitchValue(L"not-base64-string"));

  std::string token("this is a token");
  std::string encoded;
  base::Base64Encode(token, &encoded);
  EXPECT_EQ(token,
            *installer::DecodeDMTokenSwitchValue(base::UTF8ToUTF16(encoded)));
}

TEST(SetupUtilTest, StoreDMTokenToRegistrySuccess) {
  install_static::ScopedInstallDetails scoped_install_details(true);
  registry_util::RegistryOverrideManager registry_override_manager;
  registry_override_manager.OverrideRegistry(HKEY_LOCAL_MACHINE);

  // Use the 2 argument std::string constructor so that the length of the string
  // is not calculated by assuming the input char array is null terminated.
  static constexpr char kTokenData[] = "tokens are \0 binary data";
  static constexpr DWORD kExpectedSize = sizeof(kTokenData) - 1;
  std::string token(&kTokenData[0], kExpectedSize);
  ASSERT_EQ(kExpectedSize, token.length());
  EXPECT_TRUE(installer::StoreDMToken(token));

  std::wstring path;
  std::wstring name;
  InstallUtil::GetMachineLevelUserCloudPolicyDMTokenRegistryPath(&path, &name);
  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_LOCAL_MACHINE, path.c_str(),
                                    KEY_QUERY_VALUE | KEY_WOW64_64KEY));

  DWORD size = kExpectedSize;
  std::vector<char> raw_value(size);
  DWORD dtype;
  ASSERT_EQ(ERROR_SUCCESS,
            key.ReadValue(name.c_str(), raw_value.data(), &size, &dtype));
  EXPECT_EQ(REG_BINARY, dtype);
  ASSERT_EQ(kExpectedSize, size);
  EXPECT_EQ(0, memcmp(token.data(), raw_value.data(), kExpectedSize));
}

TEST(SetupUtilTest, StoreDMTokenToRegistryShouldFailWhenDMTokenTooLarge) {
  install_static::ScopedInstallDetails scoped_install_details(true);
  registry_util::RegistryOverrideManager registry_override_manager;
  registry_override_manager.OverrideRegistry(HKEY_LOCAL_MACHINE);

  std::string token_too_large(installer::kMaxDMTokenLength + 1, 'x');
  ASSERT_GT(token_too_large.size(), installer::kMaxDMTokenLength);

  EXPECT_FALSE(installer::StoreDMToken(token_too_large));
}

namespace installer {

class DeleteRegistryKeyPartialTest : public ::testing::Test {
 protected:
  using RegKey = base::win::RegKey;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(_registry_override_manager.OverrideRegistry(root_));
    to_preserve_.push_back(L"preSERve1");
    to_preserve_.push_back(L"1evRESerp");
  }

  void CreateSubKeys(bool with_preserves) {
    ASSERT_FALSE(RegKey(root_, path_.c_str(), KEY_READ).Valid());
    // These subkeys are added such that 1) keys to preserve are intermixed with
    // other keys, and 2) the case of the keys to preserve doesn't match the
    // values in |to_preserve_|.
    ASSERT_EQ(ERROR_SUCCESS, RegKey(root_, path_.c_str(), KEY_WRITE)
                                 .CreateKey(L"0sub", KEY_WRITE));
    if (with_preserves) {
      ASSERT_EQ(ERROR_SUCCESS, RegKey(root_, path_.c_str(), KEY_WRITE)
                                   .CreateKey(L"1evreserp", KEY_WRITE));
    }
    ASSERT_EQ(ERROR_SUCCESS, RegKey(root_, path_.c_str(), KEY_WRITE)
                                 .CreateKey(L"asub", KEY_WRITE));
    if (with_preserves) {
      ASSERT_EQ(ERROR_SUCCESS, RegKey(root_, path_.c_str(), KEY_WRITE)
                                   .CreateKey(L"preserve1", KEY_WRITE));
    }
    ASSERT_EQ(ERROR_SUCCESS, RegKey(root_, path_.c_str(), KEY_WRITE)
                                 .CreateKey(L"sub1", KEY_WRITE));
  }

  const HKEY root_ = HKEY_CURRENT_USER;
  base::string16 path_ = L"key_path";
  std::vector<base::string16> to_preserve_;

 private:
  registry_util::RegistryOverrideManager _registry_override_manager;
};

TEST_F(DeleteRegistryKeyPartialTest, NoKey) {
  DeleteRegistryKeyPartial(root_, L"does_not_exist",
                           std::vector<base::string16>());
  DeleteRegistryKeyPartial(root_, L"does_not_exist", to_preserve_);
}

TEST_F(DeleteRegistryKeyPartialTest, EmptyKey) {
  ASSERT_FALSE(RegKey(root_, path_.c_str(), KEY_READ).Valid());
  ASSERT_TRUE(RegKey(root_, path_.c_str(), KEY_WRITE).Valid());
  DeleteRegistryKeyPartial(root_, path_.c_str(), std::vector<base::string16>());
  ASSERT_FALSE(RegKey(root_, path_.c_str(), KEY_READ).Valid());

  ASSERT_TRUE(RegKey(root_, path_.c_str(), KEY_WRITE).Valid());
  DeleteRegistryKeyPartial(root_, path_.c_str(), to_preserve_);
  ASSERT_FALSE(RegKey(root_, path_.c_str(), KEY_READ).Valid());
}

TEST_F(DeleteRegistryKeyPartialTest, NonEmptyKey) {
  CreateSubKeys(false); /* !with_preserves */
  DeleteRegistryKeyPartial(root_, path_.c_str(), std::vector<base::string16>());
  ASSERT_FALSE(RegKey(root_, path_.c_str(), KEY_READ).Valid());

  CreateSubKeys(false); /* !with_preserves */
  ASSERT_TRUE(RegKey(root_, path_.c_str(), KEY_WRITE).Valid());
  DeleteRegistryKeyPartial(root_, path_.c_str(), to_preserve_);
  ASSERT_FALSE(RegKey(root_, path_.c_str(), KEY_READ).Valid());
}

TEST_F(DeleteRegistryKeyPartialTest, NonEmptyKeyWithPreserve) {
  CreateSubKeys(true); /* with_preserves */

  // Put some values into the main key.
  {
    RegKey key(root_, path_.c_str(), KEY_SET_VALUE);
    ASSERT_TRUE(key.Valid());
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(nullptr, 5U));
    ASSERT_EQ(1u, base::win::RegistryValueIterator(root_, path_.c_str())
                      .ValueCount());
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"foo", L"bar"));
    ASSERT_EQ(2u, base::win::RegistryValueIterator(root_, path_.c_str())
                      .ValueCount());
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"baz", L"huh"));
    ASSERT_EQ(3u, base::win::RegistryValueIterator(root_, path_.c_str())
                      .ValueCount());
  }

  ASSERT_TRUE(RegKey(root_, path_.c_str(), KEY_WRITE).Valid());
  DeleteRegistryKeyPartial(root_, path_.c_str(), to_preserve_);
  ASSERT_TRUE(RegKey(root_, path_.c_str(), KEY_READ).Valid());

  // Ensure that the preserved subkeys are still present.
  {
    base::win::RegistryKeyIterator it(root_, path_.c_str());
    ASSERT_EQ(to_preserve_.size(), it.SubkeyCount());
    for (; it.Valid(); ++it) {
      ASSERT_NE(to_preserve_.end(),
                std::find_if(to_preserve_.begin(), to_preserve_.end(),
                             [&it](const base::string16& key_name) {
                               return base::ToLowerASCII(it.Name()) ==
                                      base::ToLowerASCII(key_name);
                             }))
          << it.Name();
    }
  }

  // Ensure that all values are absent.
  {
    base::win::RegistryValueIterator it(root_, path_.c_str());
    ASSERT_EQ(0u, it.ValueCount());
  }
}

class LegacyCleanupsTest : public ::testing::Test {
 protected:
  LegacyCleanupsTest() = default;
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    installer_state_ =
        std::make_unique<FakeInstallerState>(temp_dir_.GetPath());
    // Create the state to be cleared.
    ASSERT_TRUE(base::win::RegKey(HKEY_CURRENT_USER, kBinariesClientsKeyPath,
                                  KEY_WRITE | KEY_WOW64_32KEY)
                    .Valid());
    ASSERT_TRUE(base::win::RegKey(HKEY_CURRENT_USER, kCommandExecuteImplClsid,
                                  KEY_WRITE)
                    .Valid());
#if defined(GOOGLE_CHROME_BUILD)
    ASSERT_TRUE(base::win::RegKey(HKEY_CURRENT_USER, kGCFClientsKeyPath,
                                  KEY_WRITE | KEY_WOW64_32KEY)
                    .Valid());
    ASSERT_TRUE(base::win::RegKey(HKEY_CURRENT_USER, kAppLauncherClientsKeyPath,
                                  KEY_WRITE | KEY_WOW64_32KEY)
                    .Valid());
    ASSERT_GT(base::WriteFile(GetAppHostExePath(), "cha", 3), 0);
    ASSERT_TRUE(
        base::win::RegKey(HKEY_CURRENT_USER,
                          GetChromeAppCommandPath(L"install-extension").c_str(),
                          KEY_WRITE | KEY_WOW64_32KEY)
            .Valid());
#endif  // GOOGLE_CHROME_BUILD
  }

  const InstallerState& installer_state() const { return *installer_state_; }

  bool HasBinariesVersionKey() const {
    return base::win::RegKey(HKEY_CURRENT_USER, kBinariesClientsKeyPath,
                             KEY_QUERY_VALUE | KEY_WOW64_32KEY)
        .Valid();
  }

  bool HasCommandExecuteImplClassKey() const {
    return base::win::RegKey(HKEY_CURRENT_USER, kCommandExecuteImplClsid,
                             KEY_QUERY_VALUE)
        .Valid();
  }

#if defined(GOOGLE_CHROME_BUILD)
  bool HasMultiGCFVersionKey() const {
    return base::win::RegKey(HKEY_CURRENT_USER, kGCFClientsKeyPath,
                             KEY_QUERY_VALUE | KEY_WOW64_32KEY)
        .Valid();
  }

  bool HasAppLauncherVersionKey() const {
    return base::win::RegKey(HKEY_CURRENT_USER, kAppLauncherClientsKeyPath,
                             KEY_QUERY_VALUE | KEY_WOW64_32KEY)
        .Valid();
  }

  bool HasAppHostExe() const { return base::PathExists(GetAppHostExePath()); }

  bool HasInstallExtensionCommand() const {
    return base::win::RegKey(
               HKEY_CURRENT_USER,
               GetChromeAppCommandPath(L"install-extension").c_str(),
               KEY_QUERY_VALUE | KEY_WOW64_32KEY)
        .Valid();
  }
#endif  // GOOGLE_CHROME_BUILD

 private:
  // An InstallerState for a per-user install of Chrome in a given directory.
  class FakeInstallerState : public InstallerState {
   public:
    explicit FakeInstallerState(const base::FilePath& target_path) {
      BrowserDistribution* dist = BrowserDistribution::GetDistribution();
      operation_ = InstallerState::SINGLE_INSTALL_OR_UPDATE;
      target_path_ = target_path;
      state_key_ = dist->GetStateKey();
      product_ = std::make_unique<Product>(dist);
      level_ = InstallerState::USER_LEVEL;
      root_key_ = HKEY_CURRENT_USER;
    }
  };

#if defined(GOOGLE_CHROME_BUILD)
  base::FilePath GetAppHostExePath() const {
    return installer_state_->target_path().AppendASCII("app_host.exe");
  }

  base::string16 GetChromeAppCommandPath(const wchar_t* command) const {
    return base::string16(
               L"SOFTWARE\\Google\\Update\\Clients\\"
               L"{8A69D345-D564-463c-AFF1-A69D9E530F96}\\Commands\\") +
           command;
  }
#endif  // GOOGLE_CHROME_BUILD

  static const wchar_t kBinariesClientsKeyPath[];
  static const wchar_t kCommandExecuteImplClsid[];
#if defined(GOOGLE_CHROME_BUILD)
  static const wchar_t kGCFClientsKeyPath[];
  static const wchar_t kAppLauncherClientsKeyPath[];
#endif

  base::ScopedTempDir temp_dir_;
  registry_util::RegistryOverrideManager registry_override_manager_;
  std::unique_ptr<FakeInstallerState> installer_state_;
  DISALLOW_COPY_AND_ASSIGN(LegacyCleanupsTest);
};

#if defined(GOOGLE_CHROME_BUILD)
const wchar_t LegacyCleanupsTest::kBinariesClientsKeyPath[] =
    L"SOFTWARE\\Google\\Update\\Clients\\"
    L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";
const wchar_t LegacyCleanupsTest::kCommandExecuteImplClsid[] =
    L"Software\\Classes\\CLSID\\{5C65F4B0-3651-4514-B207-D10CB699B14B}";
const wchar_t LegacyCleanupsTest::kGCFClientsKeyPath[] =
    L"SOFTWARE\\Google\\Update\\Clients\\"
    L"{8BA986DA-5100-405E-AA35-86F34A02ACBF}";
const wchar_t LegacyCleanupsTest::kAppLauncherClientsKeyPath[] =
    L"SOFTWARE\\Google\\Update\\Clients\\"
    L"{FDA71E6F-AC4C-4a00-8B70-9958A68906BF}";
#else   // GOOGLE_CHROME_BUILD
const wchar_t LegacyCleanupsTest::kBinariesClientsKeyPath[] =
    L"SOFTWARE\\Chromium Binaries";
const wchar_t LegacyCleanupsTest::kCommandExecuteImplClsid[] =
    L"Software\\Classes\\CLSID\\{A2DF06F9-A21A-44A8-8A99-8B9C84F29160}";
#endif  // !GOOGLE_CHROME_BUILD

TEST_F(LegacyCleanupsTest, NoOpOnFailedUpdate) {
  DoLegacyCleanups(installer_state(), INSTALL_FAILED);
  EXPECT_TRUE(HasBinariesVersionKey());
  EXPECT_TRUE(HasCommandExecuteImplClassKey());
#if defined(GOOGLE_CHROME_BUILD)
  EXPECT_TRUE(HasMultiGCFVersionKey());
  EXPECT_TRUE(HasAppLauncherVersionKey());
  EXPECT_TRUE(HasAppHostExe());
  EXPECT_TRUE(HasInstallExtensionCommand());
#endif  // GOOGLE_CHROME_BUILD
}

TEST_F(LegacyCleanupsTest, Do) {
  DoLegacyCleanups(installer_state(), NEW_VERSION_UPDATED);
  EXPECT_FALSE(HasBinariesVersionKey());
  EXPECT_FALSE(HasCommandExecuteImplClassKey());
#if defined(GOOGLE_CHROME_BUILD)
  EXPECT_FALSE(HasMultiGCFVersionKey());
  EXPECT_FALSE(HasAppLauncherVersionKey());
  EXPECT_FALSE(HasAppHostExe());
  EXPECT_FALSE(HasInstallExtensionCommand());
#endif  // GOOGLE_CHROME_BUILD
}

}  // namespace installer
