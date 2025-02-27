// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/service_process/service_process_control.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/process/process_iterator.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/cloud_print.mojom.h"
#include "chrome/common/service_process_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

class ServiceProcessControlBrowserTest
    : public InProcessBrowserTest {
 public:
  ServiceProcessControlBrowserTest() {
  }
  ~ServiceProcessControlBrowserTest() override {}

  void HistogramsCallback(base::RepeatingClosure on_done) {
    MockHistogramsCallback();
    on_done.Run();
  }

  MOCK_METHOD0(MockHistogramsCallback, void());

 protected:
  void LaunchServiceProcessControl(base::RepeatingClosure on_launched) {
    // Launch the process asynchronously.
    ServiceProcessControl::GetInstance()->Launch(
        base::BindOnce(
            &ServiceProcessControlBrowserTest::ProcessControlLaunched,
            base::Unretained(this), on_launched),
        base::BindOnce(
            &ServiceProcessControlBrowserTest::ProcessControlLaunchFailed,
            base::Unretained(this), on_launched));
  }

  void LaunchServiceProcessControlAndWait() {
    base::RunLoop run_loop;
    LaunchServiceProcessControl(run_loop.QuitClosure());
    run_loop.Run();
  }

  void Disconnect() {
    // This will close the IPC connection.
    ServiceProcessControl::GetInstance()->Disconnect();
  }

  void SetUp() override {
#if defined(OS_MACOSX)
    // browser_tests and the child processes run as standalone executables,
    // rather than bundled apps. For this test, set up the CHILD_PROCESS_EXE to
    // point to a bundle so that the service process has an Info.plist.
    base::FilePath exe_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
    exe_path = exe_path.DirName()
                   .DirName()
                   .Append("Contents")
                   .Append("Versions")
                   .Append(chrome::kChromeVersion)
                   .Append(chrome::kHelperProcessExecutablePath);
    child_process_exe_override_ = std::make_unique<base::ScopedPathOverride>(
        content::CHILD_PROCESS_EXE, exe_path);
#endif

    InProcessBrowserTest::SetUp();

    // This should not be needed because TearDown() ends with a closed
    // service_process_, but HistogramsTimeout and Histograms fail without this
    // on Mac.
    service_process_.Close();
  }

  void TearDown() override {
    if (ServiceProcessControl::GetInstance()->IsConnected())
      EXPECT_TRUE(ServiceProcessControl::GetInstance()->Shutdown());

#if defined(OS_MACOSX)
    // ForceServiceProcessShutdown removes the process from launched on Mac.
    ForceServiceProcessShutdown("", 0);

    child_process_exe_override_.reset();
#endif  // OS_MACOSX

    if (service_process_.IsValid()) {
      int exit_code;
      EXPECT_TRUE(service_process_.WaitForExitWithTimeout(
          TestTimeouts::action_max_timeout(), &exit_code));
      EXPECT_EQ(0, exit_code);
      service_process_.Close();
    }

    InProcessBrowserTest::TearDown();
  }

  void ProcessControlLaunched(base::OnceClosure on_done) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ProcessId service_pid;
    EXPECT_TRUE(GetServiceProcessData(NULL, &service_pid));
    EXPECT_NE(static_cast<base::ProcessId>(0), service_pid);
#if defined(OS_WIN)
    service_process_ =
        base::Process::OpenWithAccess(service_pid,
                                      SYNCHRONIZE | PROCESS_QUERY_INFORMATION);
#else
    service_process_ = base::Process::Open(service_pid);
#endif
    EXPECT_TRUE(service_process_.IsValid());
    std::move(on_done).Run();
  }

  void ProcessControlLaunchFailed(base::OnceClosure on_done) {
    ADD_FAILURE();
    std::move(on_done).Run();
  }

 private:
#if defined(OS_MACOSX)
  std::unique_ptr<base::ScopedPathOverride> child_process_exe_override_;
#endif
  base::Process service_process_;
};

class RealServiceProcessControlBrowserTest
      : public ServiceProcessControlBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ServiceProcessControlBrowserTest::SetUpCommandLine(command_line);
    base::FilePath exe;
    base::PathService::Get(base::DIR_EXE, &exe);
#if defined(OS_MACOSX)
    exe = exe.DirName().DirName().DirName();
#endif
    exe = exe.Append(chrome::kHelperProcessExecutablePath);
    // Run chrome instead of browser_tests.exe.
    EXPECT_TRUE(base::PathExists(exe));
    command_line->AppendSwitchPath(switches::kBrowserSubprocessPath, exe);
  }
};

// TODO(vitalybuka): Fix crbug.com/340563
IN_PROC_BROWSER_TEST_F(RealServiceProcessControlBrowserTest,
                       DISABLED_LaunchAndIPC) {
  LaunchServiceProcessControlAndWait();

  // Make sure we are connected to the service process.
  ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  cloud_print::mojom::CloudPrintPtr cloud_print_proxy;
  ServiceProcessControl::GetInstance()->remote_interfaces().GetInterface(
      &cloud_print_proxy);
  base::RunLoop run_loop;
  cloud_print_proxy->GetCloudPrintProxyInfo(
      base::BindOnce([](base::OnceClosure done, bool, const std::string&,
                        const std::string&) { std::move(done).Run(); },
                     run_loop.QuitClosure()));
  run_loop.Run();

  // And then shutdown the service process.
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->Shutdown());
}

IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, LaunchAndIPC) {
  LaunchServiceProcessControlAndWait();

  // Make sure we are connected to the service process.
  ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  cloud_print::mojom::CloudPrintPtr cloud_print_proxy;
  ServiceProcessControl::GetInstance()->remote_interfaces().GetInterface(
      &cloud_print_proxy);
  base::RunLoop run_loop;
  cloud_print_proxy->GetCloudPrintProxyInfo(
      base::BindOnce([](base::OnceClosure done, bool, const std::string&,
                        const std::string&) { std::move(done).Run(); },
                     run_loop.QuitClosure()));
  run_loop.Run();

  // And then shutdown the service process.
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->Shutdown());
}

IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, LaunchAndReconnect) {
  LaunchServiceProcessControlAndWait();

  // Make sure we are connected to the service process.
  ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  // Send an IPC that will keep the service process alive after we disconnect.
  cloud_print::mojom::CloudPrintPtr cloud_print_proxy;
  ServiceProcessControl::GetInstance()->remote_interfaces().GetInterface(
      &cloud_print_proxy);
  cloud_print_proxy->EnableCloudPrintProxyWithRobot(
      "", "", "", base::Value(base::Value::Type::DICTIONARY));

  ServiceProcessControl::GetInstance()->remote_interfaces().GetInterface(
      &cloud_print_proxy);
  {
    base::RunLoop run_loop;
    cloud_print_proxy->GetCloudPrintProxyInfo(
        base::BindOnce([](base::OnceClosure done, bool, const std::string&,
                          const std::string&) { std::move(done).Run(); },
                       run_loop.QuitClosure()));
    run_loop.Run();
    Disconnect();
  }

  {
    base::RunLoop run_loop;
    LaunchServiceProcessControl(run_loop.QuitClosure());

    ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
    run_loop.Run();
  }

  ServiceProcessControl::GetInstance()->remote_interfaces().GetInterface(
      &cloud_print_proxy);
  {
    base::RunLoop run_loop;
    cloud_print_proxy->GetCloudPrintProxyInfo(
        base::BindOnce([](base::OnceClosure done, bool, const std::string&,
                          const std::string&) { std::move(done).Run(); },
                       run_loop.QuitClosure()));
    run_loop.Run();
  }

  // And then shutdown the service process.
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->Shutdown());
}

// This tests the case when a service process is launched when the browser
// starts but we try to launch it again while setting up Cloud Print.
// Flaky on Mac. http://crbug.com/517420
#if defined(OS_MACOSX)
#define MAYBE_LaunchTwice DISABLED_LaunchTwice
#else
#define MAYBE_LaunchTwice LaunchTwice
#endif
IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, MAYBE_LaunchTwice) {
  // Launch the service process the first time.
  LaunchServiceProcessControlAndWait();

  // Make sure we are connected to the service process.
  ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  cloud_print::mojom::CloudPrintPtr cloud_print_proxy;
  ServiceProcessControl::GetInstance()->remote_interfaces().GetInterface(
      &cloud_print_proxy);
  {
    base::RunLoop run_loop;
    cloud_print_proxy->GetCloudPrintProxyInfo(
        base::BindOnce([](base::OnceClosure done, bool, const std::string&,
                          const std::string&) { std::move(done).Run(); },
                       run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Launch the service process again.
  LaunchServiceProcessControlAndWait();
  ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  ServiceProcessControl::GetInstance()->remote_interfaces().GetInterface(
      &cloud_print_proxy);
  {
    base::RunLoop run_loop;
    cloud_print_proxy->GetCloudPrintProxyInfo(
        base::BindOnce([](base::OnceClosure done, bool, const std::string&,
                          const std::string&) { std::move(done).Run(); },
                       run_loop.QuitClosure()));
    run_loop.Run();
  }
}

// Flaky on Mac. http://crbug.com/517420
#if defined(OS_MACOSX)
#define MAYBE_MultipleLaunchTasks DISABLED_MultipleLaunchTasks
#else
#define MAYBE_MultipleLaunchTasks MultipleLaunchTasks
#endif
// Invoke multiple Launch calls in succession and ensure that all the tasks
// get invoked.
IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest,
                       MAYBE_MultipleLaunchTasks) {
  ServiceProcessControl* process = ServiceProcessControl::GetInstance();
  constexpr int kExpectedLaunchCount = 5;
  int success_count = 0;
  base::RunLoop run_loop;
  base::RepeatingClosure on_launch_attempted =
      base::BarrierClosure(kExpectedLaunchCount, run_loop.QuitClosure());
  for (int i = 0; i < kExpectedLaunchCount; i++) {
    // Launch the process asynchronously.
    process->Launch(base::BindOnce(
                        [](int* success_count, base::OnceClosure task) {
                          (*success_count)++;
                          std::move(task).Run();
                        },
                        &success_count, on_launch_attempted),
                    on_launch_attempted);
  }
  run_loop.Run();
  EXPECT_EQ(kExpectedLaunchCount, success_count);
}

// Flaky on Mac. http://crbug.com/517420
#if defined(OS_MACOSX)
#define MAYBE_SameLaunchTask DISABLED_SameLaunchTask
#else
#define MAYBE_SameLaunchTask SameLaunchTask
#endif
// Make sure using the same task for success and failure tasks works.
IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, MAYBE_SameLaunchTask) {
  ServiceProcessControl* process = ServiceProcessControl::GetInstance();
  constexpr int kExpectedLaunchCount = 5;
  base::RunLoop run_loop;
  base::RepeatingClosure task =
      base::BarrierClosure(kExpectedLaunchCount, run_loop.QuitClosure());
  for (int i = 0; i < kExpectedLaunchCount; i++) {
    // Launch the process asynchronously.
    process->Launch(task, task);
  }
  run_loop.Run();
}

// Tests whether disconnecting from the service IPC causes the service process
// to die.
// Flaky on Mac. http://crbug.com/517420
#if defined(OS_MACOSX)
#define MAYBE_DieOnDisconnect DISABLED_DieOnDisconnect
#else
#define MAYBE_DieOnDisconnect DieOnDisconnect
#endif
IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest,
                       MAYBE_DieOnDisconnect) {
  // Launch the service process.
  LaunchServiceProcessControlAndWait();
  // Make sure we are connected to the service process.
  ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  Disconnect();
}

// Flaky on Mac. http://crbug.com/517420
#if defined(OS_MACOSX)
#define MAYBE_ForceShutdown DISABLED_ForceShutdown
#else
#define MAYBE_ForceShutdown ForceShutdown
#endif
IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, MAYBE_ForceShutdown) {
  // Launch the service process.
  LaunchServiceProcessControlAndWait();
  // Make sure we are connected to the service process.
  ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  base::ProcessId service_pid;
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(GetServiceProcessData(NULL, &service_pid));
  EXPECT_NE(static_cast<base::ProcessId>(0), service_pid);
  ForceServiceProcessShutdown(version_info::GetVersionNumber(), service_pid);
}

// Flaky on Mac. http://crbug.com/517420
#if defined(OS_MACOSX)
#define MAYBE_CheckPid DISABLED_CheckPid
#else
#define MAYBE_CheckPid CheckPid
#endif
IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, MAYBE_CheckPid) {
  base::ProcessId service_pid;
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(GetServiceProcessData(NULL, &service_pid));
  // Launch the service process.
  LaunchServiceProcessControlAndWait();
  EXPECT_TRUE(GetServiceProcessData(NULL, &service_pid));
  EXPECT_NE(static_cast<base::ProcessId>(0), service_pid);
  // Disconnect from service process.
  Disconnect();
}

IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, HistogramsNoService) {
  ASSERT_FALSE(ServiceProcessControl::GetInstance()->IsConnected());
  EXPECT_CALL(*this, MockHistogramsCallback()).Times(0);
  EXPECT_FALSE(ServiceProcessControl::GetInstance()->GetHistograms(
      base::BindRepeating(&ServiceProcessControlBrowserTest::HistogramsCallback,
                          base::Unretained(this), base::DoNothing()),
      base::TimeDelta()));
}

// Histograms disabled on OSX http://crbug.com/406227
#if defined(OS_MACOSX)
#define MAYBE_HistogramsTimeout DISABLED_HistogramsTimeout
#define MAYBE_Histograms DISABLED_Histograms
#else
#define MAYBE_HistogramsTimeout HistogramsTimeout
#define MAYBE_Histograms Histograms
#endif
IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest,
                       MAYBE_HistogramsTimeout) {
  LaunchServiceProcessControlAndWait();
  ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  // Callback should not be called during GetHistograms call.
  EXPECT_CALL(*this, MockHistogramsCallback()).Times(0);
  base::RunLoop run_loop;
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->GetHistograms(
      base::BindRepeating(&ServiceProcessControlBrowserTest::HistogramsCallback,
                          base::Unretained(this), run_loop.QuitClosure()),
      base::TimeDelta::FromMilliseconds(100)));
  EXPECT_CALL(*this, MockHistogramsCallback()).Times(1);
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->Shutdown());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceProcessControlBrowserTest, MAYBE_Histograms) {
  LaunchServiceProcessControlAndWait();
  ASSERT_TRUE(ServiceProcessControl::GetInstance()->IsConnected());
  // Callback should not be called during GetHistograms call.
  EXPECT_CALL(*this, MockHistogramsCallback()).Times(0);
  // Wait for real callback by providing large timeout value.
  base::RunLoop run_loop;
  EXPECT_TRUE(ServiceProcessControl::GetInstance()->GetHistograms(
      base::BindRepeating(&ServiceProcessControlBrowserTest::HistogramsCallback,
                          base::Unretained(this), run_loop.QuitClosure()),
      base::TimeDelta::FromHours(1)));
  EXPECT_CALL(*this, MockHistogramsCallback()).Times(1);
  run_loop.Run();
}
