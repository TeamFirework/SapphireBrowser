// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/server_socket.h"
#include "net/socket/socket_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/mojo_socket_test_util.h"
#include "services/network/proxy_resolving_socket_factory_mojo.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/socket_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

// Message sent over the tcp connection.
const char kMsg[] = "please start tls!";
const size_t kMsgSize = strlen(kMsg);

// Message sent over the tls connection.
const char kSecretMsg[] = "here is secret.";
const size_t kSecretMsgSize = strlen(kSecretMsg);

class TLSClientSocketTestBase {
 public:
  enum Mode { kDirect, kProxyResolving };

  explicit TLSClientSocketTestBase(Mode mode)
      : mode_(mode),
        scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        url_request_context_(true) {}
  ~TLSClientSocketTestBase() {}

 protected:
  // One of the two fields will be set, depending on the mode.
  struct SocketHandle {
    mojom::TCPConnectedSocketPtr tcp_socket;
    mojom::ProxyResolvingSocketPtr proxy_socket;
  };

  struct SocketRequest {
    mojom::TCPConnectedSocketRequest tcp_socket_request;
    mojom::ProxyResolvingSocketRequest proxy_socket_request;
  };

  // Initializes the test fixture. If |use_mock_sockets|, mock client socket
  // factory will be used.
  void Init(bool use_mock_sockets, bool configure_proxy) {
    if (use_mock_sockets) {
      mock_client_socket_factory_.set_enable_read_if_ready(true);
      url_request_context_.set_client_socket_factory(
          &mock_client_socket_factory_);
    }
    if (configure_proxy) {
      proxy_resolution_service_ = net::ProxyResolutionService::CreateFixed(
          "http://proxy:8080", TRAFFIC_ANNOTATION_FOR_TESTS);
      url_request_context_.set_proxy_resolution_service(
          proxy_resolution_service_.get());
    }
    url_request_context_.Init();
    factory_ = std::make_unique<SocketFactory>(nullptr /*net_log*/,
                                               &url_request_context_);
    proxy_resolving_factory_ =
        std::make_unique<ProxyResolvingSocketFactoryMojo>(
            &url_request_context_);
  }

  // Reads |num_bytes| from |handle| or reads until an error occurs. Returns the
  // bytes read as a string.
  std::string Read(mojo::ScopedDataPipeConsumerHandle* handle,
                   size_t num_bytes) {
    std::string received_contents;
    while (received_contents.size() < num_bytes) {
      base::RunLoop().RunUntilIdle();
      std::vector<char> buffer(num_bytes);
      uint32_t read_size = static_cast<uint32_t>(num_bytes);
      MojoResult result = handle->get().ReadData(buffer.data(), &read_size,
                                                 MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT)
        continue;
      if (result != MOJO_RESULT_OK)
        return received_contents;
      received_contents.append(buffer.data(), read_size);
    }
    return received_contents;
  }

  SocketRequest MakeRequest(SocketHandle* handle) {
    SocketRequest result;
    if (mode_ == kDirect)
      result.tcp_socket_request = mojo::MakeRequest(&handle->tcp_socket);
    else
      result.proxy_socket_request = mojo::MakeRequest(&handle->proxy_socket);
    return result;
  }

  void ResetSocket(SocketHandle* handle) {
    if (mode_ == kDirect)
      handle->tcp_socket.reset();
    else
      handle->proxy_socket.reset();
  }

  int CreateSocketSync(SocketRequest request,
                       const net::IPEndPoint& remote_addr) {
    if (mode_ == kDirect) {
      return CreateTCPConnectedSocketSync(std::move(request.tcp_socket_request),
                                          remote_addr);
    } else {
      return CreateProxyResolvingSocketSync(
          std::move(request.proxy_socket_request), remote_addr);
    }
  }

  int CreateTCPConnectedSocketSync(mojom::TCPConnectedSocketRequest request,
                                   const net::IPEndPoint& remote_addr) {
    net::AddressList remote_addr_list(remote_addr);
    base::RunLoop run_loop;
    int net_error = net::ERR_FAILED;
    factory_->CreateTCPConnectedSocket(
        base::nullopt /* local_addr */, remote_addr_list,
        TRAFFIC_ANNOTATION_FOR_TESTS, std::move(request),
        pre_tls_observer()->GetObserverPtr(),
        base::BindLambdaForTesting(
            [&](int result,
                const base::Optional<net::IPEndPoint>& actual_local_addr,
                const base::Optional<net::IPEndPoint>& peer_addr,
                mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
                mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
              net_error = result;
              pre_tls_recv_handle_ = std::move(receive_pipe_handle);
              pre_tls_send_handle_ = std::move(send_pipe_handle);
              run_loop.Quit();
            }));
    run_loop.Run();
    return net_error;
  }

  int CreateProxyResolvingSocketSync(mojom::ProxyResolvingSocketRequest request,
                                     const net::IPEndPoint& remote_addr) {
    GURL url("https://" + remote_addr.ToString());
    base::RunLoop run_loop;
    int net_error = net::ERR_FAILED;
    proxy_resolving_factory_->CreateProxyResolvingSocket(
        url, false /* use_tls */,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        std::move(request),
        base::BindLambdaForTesting(
            [&](int result,
                const base::Optional<net::IPEndPoint>& actual_local_addr,
                const base::Optional<net::IPEndPoint>& peer_addr,
                mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
                mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
              net_error = result;
              pre_tls_recv_handle_ = std::move(receive_pipe_handle);
              pre_tls_send_handle_ = std::move(send_pipe_handle);
              run_loop.Quit();
            }));
    run_loop.Run();
    return net_error;
  }

  void UpgradeToTLS(SocketHandle* handle,
                    const net::HostPortPair& host_port_pair,
                    mojom::TLSClientSocketRequest request,
                    net::CompletionOnceCallback callback) {
    if (mode_ == kDirect) {
      UpgradeTCPConnectedSocketToTLS(handle->tcp_socket.get(), host_port_pair,
                                     std::move(request), std::move(callback));
    } else {
      UpgradeProxyResolvingSocketToTLS(handle->proxy_socket.get(),
                                       host_port_pair, std::move(request),
                                       std::move(callback));
    }
  }

  void UpgradeTCPConnectedSocketToTLS(mojom::TCPConnectedSocket* client_socket,
                                      const net::HostPortPair& host_port_pair,
                                      mojom::TLSClientSocketRequest request,
                                      net::CompletionOnceCallback callback) {
    client_socket->UpgradeToTLS(
        host_port_pair, nullptr /* ssl_config_ptr */,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        std::move(request), post_tls_observer()->GetObserverPtr(),
        base::BindOnce(
            [](net::CompletionOnceCallback cb,
               mojo::ScopedDataPipeConsumerHandle* consumer_handle,
               mojo::ScopedDataPipeProducerHandle* producer_handle, int result,
               mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
               mojo::ScopedDataPipeProducerHandle send_pipe_handle,
               const base::Optional<net::SSLInfo>& ssl_info) {
              *consumer_handle = std::move(receive_pipe_handle);
              *producer_handle = std::move(send_pipe_handle);
              std::move(cb).Run(result);
            },
            std::move(callback), &post_tls_recv_handle_,
            &post_tls_send_handle_));
  }

  void UpgradeProxyResolvingSocketToTLS(
      mojom::ProxyResolvingSocket* client_socket,
      const net::HostPortPair& host_port_pair,
      mojom::TLSClientSocketRequest request,
      net::CompletionOnceCallback callback) {
    client_socket->UpgradeToTLS(
        host_port_pair,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        std::move(request), post_tls_observer()->GetObserverPtr(),
        base::BindOnce(
            [](net::CompletionOnceCallback cb,
               mojo::ScopedDataPipeConsumerHandle* consumer_handle,
               mojo::ScopedDataPipeProducerHandle* producer_handle, int result,
               mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
               mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
              *consumer_handle = std::move(receive_pipe_handle);
              *producer_handle = std::move(send_pipe_handle);
              std::move(cb).Run(result);
            },
            std::move(callback), &post_tls_recv_handle_,
            &post_tls_send_handle_));
  }

  TestSocketObserver* pre_tls_observer() { return &pre_tls_observer_; }
  TestSocketObserver* post_tls_observer() { return &post_tls_observer_; }

  mojo::ScopedDataPipeConsumerHandle* pre_tls_recv_handle() {
    return &pre_tls_recv_handle_;
  }

  mojo::ScopedDataPipeProducerHandle* pre_tls_send_handle() {
    return &pre_tls_send_handle_;
  }

  mojo::ScopedDataPipeConsumerHandle* post_tls_recv_handle() {
    return &post_tls_recv_handle_;
  }

  mojo::ScopedDataPipeProducerHandle* post_tls_send_handle() {
    return &post_tls_send_handle_;
  }

  net::MockClientSocketFactory* mock_client_socket_factory() {
    return &mock_client_socket_factory_;
  }

  Mode mode() const { return mode_; }

 private:
  Mode mode_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // Mojo data handles obtained from CreateTCPConnectedSocket.
  mojo::ScopedDataPipeConsumerHandle pre_tls_recv_handle_;
  mojo::ScopedDataPipeProducerHandle pre_tls_send_handle_;

  // Mojo data handles obtained from UpgradeToTLS.
  mojo::ScopedDataPipeConsumerHandle post_tls_recv_handle_;
  mojo::ScopedDataPipeProducerHandle post_tls_send_handle_;

  std::unique_ptr<net::ProxyResolutionService> proxy_resolution_service_;
  net::TestURLRequestContext url_request_context_;
  net::MockClientSocketFactory mock_client_socket_factory_;
  std::unique_ptr<SocketFactory> factory_;
  std::unique_ptr<ProxyResolvingSocketFactoryMojo> proxy_resolving_factory_;
  TestSocketObserver pre_tls_observer_;
  TestSocketObserver post_tls_observer_;
  mojo::StrongBindingSet<mojom::TCPServerSocket> tcp_server_socket_bindings_;
  mojo::StrongBindingSet<mojom::TCPConnectedSocket>
      tcp_connected_socket_bindings_;

  DISALLOW_COPY_AND_ASSIGN(TLSClientSocketTestBase);
};

}  // namespace
class TLSClientSocketTest
    : public ::testing::TestWithParam<TLSClientSocketTestBase::Mode>,
      public TLSClientSocketTestBase {
 public:
  TLSClientSocketTest() : TLSClientSocketTestBase(GetParam()) {
    Init(true /* use_mock_sockets */, false /* configure_proxy */);
  }

  ~TLSClientSocketTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TLSClientSocketTest);
};

// Basic test to call UpgradeToTLS, and then read/write after UpgradeToTLS is
// successful.
TEST_P(TLSClientSocketTest, UpgradeToTLS) {
  const net::MockRead kReads[] = {net::MockRead(net::ASYNC, kMsg, kMsgSize, 1),
                                  net::MockRead(net::SYNCHRONOUS, net::OK, 2)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, kMsg, kMsgSize, 0)};
  net::SequencedSocketData data_provider(kReads, kWrites);
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  SocketHandle client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  EXPECT_EQ(net::OK,
            CreateSocketSync(MakeRequest(&client_socket), server_addr));

  net::HostPortPair host_port_pair("example.org", 443);
  pre_tls_recv_handle()->reset();
  pre_tls_send_handle()->reset();
  net::TestCompletionCallback callback;
  mojom::TLSClientSocketPtr tls_socket;
  UpgradeToTLS(&client_socket, host_port_pair, mojo::MakeRequest(&tls_socket),
               callback.callback());
  ASSERT_EQ(net::OK, callback.WaitForResult());
  ResetSocket(&client_socket);

  uint32_t num_bytes = strlen(kMsg);
  EXPECT_EQ(MOJO_RESULT_OK, post_tls_send_handle()->get().WriteData(
                                &kMsg, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(kMsg, Read(post_tls_recv_handle(), kMsgSize));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// Same as the UpgradeToTLS test above, except this test calls
// base::RunLoop().RunUntilIdle() after destroying the pre-tls data pipes.
TEST_P(TLSClientSocketTest, ClosePipesRunUntilIdleAndUpgradeToTLS) {
  const net::MockRead kReads[] = {net::MockRead(net::ASYNC, kMsg, kMsgSize, 1),
                                  net::MockRead(net::SYNCHRONOUS, net::OK, 2)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, kMsg, kMsgSize, 0)};
  net::SequencedSocketData data_provider(kReads, kWrites);
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  SocketHandle client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  EXPECT_EQ(net::OK,
            CreateSocketSync(MakeRequest(&client_socket), server_addr));

  net::HostPortPair host_port_pair("example.org", 443);

  // Call RunUntilIdle() to test the case that pipes are closed before
  // UpgradeToTLS.
  pre_tls_recv_handle()->reset();
  pre_tls_send_handle()->reset();
  base::RunLoop().RunUntilIdle();

  net::TestCompletionCallback callback;
  mojom::TLSClientSocketPtr tls_socket;
  UpgradeToTLS(&client_socket, host_port_pair, mojo::MakeRequest(&tls_socket),
               callback.callback());
  ASSERT_EQ(net::OK, callback.WaitForResult());
  ResetSocket(&client_socket);

  uint32_t num_bytes = strlen(kMsg);
  EXPECT_EQ(MOJO_RESULT_OK, post_tls_send_handle()->get().WriteData(
                                &kMsg, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(kMsg, Read(post_tls_recv_handle(), kMsgSize));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// Calling UpgradeToTLS on the same TCPConnectedSocketPtr is illegal and should
// receive an error.
TEST_P(TLSClientSocketTest, UpgradeToTLSTwice) {
  const net::MockRead kReads[] = {net::MockRead(net::ASYNC, net::OK, 0)};
  net::SequencedSocketData data_provider(kReads, base::span<net::MockWrite>());
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  SocketHandle client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  EXPECT_EQ(net::OK,
            CreateSocketSync(MakeRequest(&client_socket), server_addr));

  net::HostPortPair host_port_pair("example.org", 443);
  pre_tls_recv_handle()->reset();
  pre_tls_send_handle()->reset();

  // First UpgradeToTLS should complete successfully.
  net::TestCompletionCallback callback;
  mojom::TLSClientSocketPtr tls_socket;
  UpgradeToTLS(&client_socket, host_port_pair, mojo::MakeRequest(&tls_socket),
               callback.callback());
  ASSERT_EQ(net::OK, callback.WaitForResult());

  // Second time UpgradeToTLS is called, it should fail.
  mojom::TLSClientSocketPtr tls_socket2;
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  if (mode() == kDirect) {
    auto upgrade2_callback = base::BindLambdaForTesting(
        [&](int result, mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
            mojo::ScopedDataPipeProducerHandle send_pipe_handle,
            const base::Optional<net::SSLInfo>& ssl_info) {
          net_error = result;
          run_loop.Quit();
        });
    client_socket.tcp_socket->UpgradeToTLS(
        host_port_pair, nullptr /* ssl_config_ptr */,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        mojo::MakeRequest(&tls_socket2), nullptr /*observer */,
        std::move(upgrade2_callback));
  } else {
    auto upgrade2_callback = base::BindLambdaForTesting(
        [&](int result, mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
            mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
          net_error = result;
          run_loop.Quit();
        });
    client_socket.proxy_socket->UpgradeToTLS(
        host_port_pair,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        mojo::MakeRequest(&tls_socket2), nullptr /*observer */,
        std::move(upgrade2_callback));
  }
  run_loop.Run();
  ASSERT_EQ(net::ERR_SOCKET_NOT_CONNECTED, net_error);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(TLSClientSocketTest, UpgradeToTLSWithCustomSSLConfig) {
  // No custom options in the proxy-resolving case.
  if (mode() != kDirect)
    return;
  const net::MockRead kReads[] = {net::MockRead(net::ASYNC, net::OK, 0)};
  net::SequencedSocketData data_provider(kReads, base::span<net::MockWrite>());
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  ssl_socket.expected_ssl_version_min = net::SSL_PROTOCOL_VERSION_TLS1_1;
  ssl_socket.expected_ssl_version_max = net::SSL_PROTOCOL_VERSION_TLS1_2;
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  SocketHandle client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  EXPECT_EQ(net::OK,
            CreateSocketSync(MakeRequest(&client_socket), server_addr));

  net::HostPortPair host_port_pair("example.org", 443);
  pre_tls_recv_handle()->reset();
  pre_tls_send_handle()->reset();

  mojom::TLSClientSocketPtr tls_socket;
  base::RunLoop run_loop;
  mojom::TLSClientSocketOptionsPtr options =
      mojom::TLSClientSocketOptions::New();
  options->version_min = mojom::SSLVersion::kTLS11;
  options->version_max = mojom::SSLVersion::kTLS12;
  int net_error = net::ERR_FAILED;
  auto upgrade_callback = base::BindLambdaForTesting(
      [&](int result, mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
          mojo::ScopedDataPipeProducerHandle send_pipe_handle,
          const base::Optional<net::SSLInfo>& ssl_info) {
        net_error = result;
        run_loop.Quit();
      });
  client_socket.tcp_socket->UpgradeToTLS(
      host_port_pair, std::move(options),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      mojo::MakeRequest(&tls_socket), nullptr /*observer */,
      std::move(upgrade_callback));
  run_loop.Run();
  ASSERT_EQ(net::OK, net_error);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// Same as the UpgradeToTLS test, except this also reads and writes to the tcp
// connection before UpgradeToTLS is called.
TEST_P(TLSClientSocketTest, ReadWriteBeforeUpgradeToTLS) {
  const net::MockRead kReads[] = {
      net::MockRead(net::SYNCHRONOUS, kMsg, kMsgSize, 0),
      net::MockRead(net::ASYNC, kSecretMsg, kSecretMsgSize, 3),
      net::MockRead(net::SYNCHRONOUS, net::OK, 4)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, kMsg, kMsgSize, 1),
      net::MockWrite(net::SYNCHRONOUS, kSecretMsg, kSecretMsgSize, 2),
  };
  net::SequencedSocketData data_provider(kReads, kWrites);
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  SocketHandle client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  EXPECT_EQ(net::OK,
            CreateSocketSync(MakeRequest(&client_socket), server_addr));

  EXPECT_EQ(kMsg, Read(pre_tls_recv_handle(), kMsgSize));

  uint32_t num_bytes = kMsgSize;
  EXPECT_EQ(MOJO_RESULT_OK, pre_tls_send_handle()->get().WriteData(
                                &kMsg, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));

  net::HostPortPair host_port_pair("example.org", 443);
  pre_tls_recv_handle()->reset();
  pre_tls_send_handle()->reset();
  net::TestCompletionCallback callback;
  mojom::TLSClientSocketPtr tls_socket;
  UpgradeToTLS(&client_socket, host_port_pair, mojo::MakeRequest(&tls_socket),
               callback.callback());
  ASSERT_EQ(net::OK, callback.WaitForResult());
  ResetSocket(&client_socket);

  num_bytes = strlen(kSecretMsg);
  EXPECT_EQ(MOJO_RESULT_OK,
            post_tls_send_handle()->get().WriteData(&kSecretMsg, &num_bytes,
                                                    MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(kSecretMsg, Read(post_tls_recv_handle(), kSecretMsgSize));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// Tests that a read error is encountered after UpgradeToTLS completes
// successfully.
TEST_P(TLSClientSocketTest, ReadErrorAfterUpgradeToTLS) {
  const net::MockRead kReads[] = {
      net::MockRead(net::ASYNC, kSecretMsg, kSecretMsgSize, 1),
      net::MockRead(net::SYNCHRONOUS, net::ERR_CONNECTION_CLOSED, 2)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, kSecretMsg, kSecretMsgSize, 0)};
  net::SequencedSocketData data_provider(kReads, kWrites);
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  SocketHandle client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  EXPECT_EQ(net::OK,
            CreateSocketSync(MakeRequest(&client_socket), server_addr));

  net::HostPortPair host_port_pair("example.org", 443);
  pre_tls_recv_handle()->reset();
  pre_tls_send_handle()->reset();
  net::TestCompletionCallback callback;
  mojom::TLSClientSocketPtr tls_socket;
  UpgradeToTLS(&client_socket, host_port_pair, mojo::MakeRequest(&tls_socket),
               callback.callback());
  ASSERT_EQ(net::OK, callback.WaitForResult());
  ResetSocket(&client_socket);

  uint32_t num_bytes = strlen(kSecretMsg);
  EXPECT_EQ(MOJO_RESULT_OK,
            post_tls_send_handle()->get().WriteData(&kSecretMsg, &num_bytes,
                                                    MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(kSecretMsg, Read(post_tls_recv_handle(), kSecretMsgSize));
  EXPECT_EQ(net::ERR_CONNECTION_CLOSED,
            post_tls_observer()->WaitForReadError());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// Tests that a read error is encountered after UpgradeToTLS completes
// successfully.
TEST_P(TLSClientSocketTest, WriteErrorAfterUpgradeToTLS) {
  const net::MockRead kReads[] = {net::MockRead(net::ASYNC, net::OK, 0)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, net::ERR_CONNECTION_CLOSED, 1)};
  net::SequencedSocketData data_provider(kReads, kWrites);
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  SocketHandle client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  EXPECT_EQ(net::OK,
            CreateSocketSync(MakeRequest(&client_socket), server_addr));

  net::HostPortPair host_port_pair("example.org", 443);
  pre_tls_recv_handle()->reset();
  pre_tls_send_handle()->reset();
  net::TestCompletionCallback callback;
  mojom::TLSClientSocketPtr tls_socket;
  UpgradeToTLS(&client_socket, host_port_pair, mojo::MakeRequest(&tls_socket),
               callback.callback());
  ASSERT_EQ(net::OK, callback.WaitForResult());
  ResetSocket(&client_socket);

  uint32_t num_bytes = strlen(kSecretMsg);
  EXPECT_EQ(MOJO_RESULT_OK,
            post_tls_send_handle()->get().WriteData(&kSecretMsg, &num_bytes,
                                                    MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(net::ERR_CONNECTION_CLOSED,
            post_tls_observer()->WaitForWriteError());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// Tests that reading from the pre-tls data pipe is okay even after UpgradeToTLS
// is called.
TEST_P(TLSClientSocketTest, ReadFromPreTlsDataPipeAfterUpgradeToTLS) {
  const net::MockRead kReads[] = {
      net::MockRead(net::ASYNC, kMsg, kMsgSize, 0),
      net::MockRead(net::ASYNC, kSecretMsg, kSecretMsgSize, 2),
      net::MockRead(net::SYNCHRONOUS, net::OK, 3)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, kSecretMsg, kSecretMsgSize, 1)};
  net::SequencedSocketData data_provider(kReads, kWrites);
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  SocketHandle client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  EXPECT_EQ(net::OK,
            CreateSocketSync(MakeRequest(&client_socket), server_addr));

  net::HostPortPair host_port_pair("example.org", 443);
  pre_tls_send_handle()->reset();
  net::TestCompletionCallback callback;
  mojom::TLSClientSocketPtr tls_socket;
  UpgradeToTLS(&client_socket, host_port_pair, mojo::MakeRequest(&tls_socket),
               callback.callback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kMsg, Read(pre_tls_recv_handle(), kMsgSize));

  // Reset pre-tls receive pipe now and UpgradeToTLS should complete.
  pre_tls_recv_handle()->reset();
  ASSERT_EQ(net::OK, callback.WaitForResult());
  ResetSocket(&client_socket);

  uint32_t num_bytes = strlen(kSecretMsg);
  EXPECT_EQ(MOJO_RESULT_OK,
            post_tls_send_handle()->get().WriteData(&kSecretMsg, &num_bytes,
                                                    MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(kSecretMsg, Read(post_tls_recv_handle(), kSecretMsgSize));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// Tests that writing to the pre-tls data pipe is okay even after UpgradeToTLS
// is called.
TEST_P(TLSClientSocketTest, WriteToPreTlsDataPipeAfterUpgradeToTLS) {
  const net::MockRead kReads[] = {
      net::MockRead(net::ASYNC, kSecretMsg, kSecretMsgSize, 2),
      net::MockRead(net::SYNCHRONOUS, net::OK, 3)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, kMsg, kMsgSize, 0),
      net::MockWrite(net::SYNCHRONOUS, kSecretMsg, kSecretMsgSize, 1)};
  net::SequencedSocketData data_provider(kReads, kWrites);
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  SocketHandle client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  EXPECT_EQ(net::OK,
            CreateSocketSync(MakeRequest(&client_socket), server_addr));

  net::HostPortPair host_port_pair("example.org", 443);
  pre_tls_recv_handle()->reset();
  net::TestCompletionCallback callback;
  mojom::TLSClientSocketPtr tls_socket;
  UpgradeToTLS(&client_socket, host_port_pair, mojo::MakeRequest(&tls_socket),
               callback.callback());
  base::RunLoop().RunUntilIdle();

  uint32_t num_bytes = strlen(kMsg);
  EXPECT_EQ(MOJO_RESULT_OK, pre_tls_send_handle()->get().WriteData(
                                &kMsg, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));

  // Reset pre-tls send pipe now and UpgradeToTLS should complete.
  pre_tls_send_handle()->reset();
  ASSERT_EQ(net::OK, callback.WaitForResult());
  ResetSocket(&client_socket);

  num_bytes = strlen(kSecretMsg);
  EXPECT_EQ(MOJO_RESULT_OK,
            post_tls_send_handle()->get().WriteData(&kSecretMsg, &num_bytes,
                                                    MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(kSecretMsg, Read(post_tls_recv_handle(), kSecretMsgSize));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// Tests that reading from and writing to pre-tls data pipe is okay even after
// UpgradeToTLS is called.
TEST_P(TLSClientSocketTest, ReadAndWritePreTlsDataPipeAfterUpgradeToTLS) {
  const net::MockRead kReads[] = {
      net::MockRead(net::ASYNC, kMsg, kMsgSize, 0),
      net::MockRead(net::ASYNC, kSecretMsg, kSecretMsgSize, 3),
      net::MockRead(net::SYNCHRONOUS, net::OK, 4)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, kMsg, kMsgSize, 1),
      net::MockWrite(net::SYNCHRONOUS, kSecretMsg, kSecretMsgSize, 2)};
  net::SequencedSocketData data_provider(kReads, kWrites);
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  SocketHandle client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  EXPECT_EQ(net::OK,
            CreateSocketSync(MakeRequest(&client_socket), server_addr));

  net::HostPortPair host_port_pair("example.org", 443);
  base::RunLoop run_loop;
  net::TestCompletionCallback callback;
  mojom::TLSClientSocketPtr tls_socket;
  UpgradeToTLS(&client_socket, host_port_pair, mojo::MakeRequest(&tls_socket),
               callback.callback());
  EXPECT_EQ(kMsg, Read(pre_tls_recv_handle(), kMsgSize));
  uint32_t num_bytes = strlen(kMsg);
  EXPECT_EQ(MOJO_RESULT_OK, pre_tls_send_handle()->get().WriteData(
                                &kMsg, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));

  // Reset pre-tls pipes now and UpgradeToTLS should complete.
  pre_tls_recv_handle()->reset();
  pre_tls_send_handle()->reset();
  ASSERT_EQ(net::OK, callback.WaitForResult());
  ResetSocket(&client_socket);

  num_bytes = strlen(kSecretMsg);
  EXPECT_EQ(MOJO_RESULT_OK,
            post_tls_send_handle()->get().WriteData(&kSecretMsg, &num_bytes,
                                                    MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(kSecretMsg, Read(post_tls_recv_handle(), kSecretMsgSize));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// Tests that a read error is encountered before UpgradeToTLS completes.
TEST_P(TLSClientSocketTest, ReadErrorBeforeUpgradeToTLS) {
  // This requires pre_tls_observer(), which is not provided by proxy resolving
  // sockets.
  if (mode() != kDirect)
    return;
  const net::MockRead kReads[] = {
      net::MockRead(net::ASYNC, kMsg, kMsgSize, 0),
      net::MockRead(net::SYNCHRONOUS, net::ERR_CONNECTION_CLOSED, 1)};
  net::SequencedSocketData data_provider(kReads, base::span<net::MockWrite>());
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);

  SocketHandle client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  EXPECT_EQ(net::OK,
            CreateSocketSync(MakeRequest(&client_socket), server_addr));

  net::HostPortPair host_port_pair("example.org", 443);
  pre_tls_send_handle()->reset();
  net::TestCompletionCallback callback;
  mojom::TLSClientSocketPtr tls_socket;
  UpgradeToTLS(&client_socket, host_port_pair, mojo::MakeRequest(&tls_socket),
               callback.callback());

  EXPECT_EQ(kMsg, Read(pre_tls_recv_handle(), kMsgSize));
  EXPECT_EQ(net::ERR_CONNECTION_CLOSED, pre_tls_observer()->WaitForReadError());

  // Reset pre-tls receive pipe now and UpgradeToTLS should complete.
  pre_tls_recv_handle()->reset();
  ASSERT_EQ(net::ERR_SOCKET_NOT_CONNECTED, callback.WaitForResult());
  ResetSocket(&client_socket);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

// Tests that a write error is encountered before UpgradeToTLS completes.
TEST_P(TLSClientSocketTest, WriteErrorBeforeUpgradeToTLS) {
  // This requires pre_tls_observer(), which is not provided by proxy resolving
  // sockets.
  if (mode() != kDirect)
    return;

  const net::MockRead kReads[] = {net::MockRead(net::ASYNC, net::OK, 1)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, net::ERR_CONNECTION_CLOSED, 0)};
  net::SequencedSocketData data_provider(kReads, kWrites);
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);

  SocketHandle client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  EXPECT_EQ(net::OK,
            CreateSocketSync(MakeRequest(&client_socket), server_addr));

  net::HostPortPair host_port_pair("example.org", 443);
  pre_tls_recv_handle()->reset();
  net::TestCompletionCallback callback;
  mojom::TLSClientSocketPtr tls_socket;
  UpgradeToTLS(&client_socket, host_port_pair, mojo::MakeRequest(&tls_socket),
               callback.callback());
  uint32_t num_bytes = strlen(kMsg);
  EXPECT_EQ(MOJO_RESULT_OK, pre_tls_send_handle()->get().WriteData(
                                &kMsg, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));

  EXPECT_EQ(net::ERR_CONNECTION_CLOSED,
            pre_tls_observer()->WaitForWriteError());
  // Reset pre-tls send pipe now and UpgradeToTLS should complete.
  pre_tls_send_handle()->reset();
  ASSERT_EQ(net::ERR_SOCKET_NOT_CONNECTED, callback.WaitForResult());
  ResetSocket(&client_socket);

  base::RunLoop().RunUntilIdle();
  // Write failed before the mock read can be consumed.
  EXPECT_FALSE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    TLSClientSocketTest,
    ::testing::Values(TLSClientSocketTestBase::kDirect,
                      TLSClientSocketTestBase::kProxyResolving));

// Tests with proxy resolving socket and a proxy actually configured.
class TLSCLientSocketProxyTest : public ::testing::Test,
                                 public TLSClientSocketTestBase {
 public:
  TLSCLientSocketProxyTest()
      : TLSClientSocketTestBase(TLSClientSocketTestBase::kProxyResolving) {
    Init(true /* use_mock_sockets*/, true /* configure_proxy */);
  }

  ~TLSCLientSocketProxyTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TLSCLientSocketProxyTest);
};

TEST_F(TLSCLientSocketProxyTest, UpgradeToTLS) {
  const char kConnectRequest[] =
      "CONNECT 127.0.0.1:1234 HTTP/1.1\r\n"
      "Host: 127.0.0.1:1234\r\n"
      "Proxy-Connection: keep-alive\r\n\r\n";
  const char kConnectResponse[] = "HTTP/1.1 200 OK\r\n\r\n";

  const net::MockRead kReads[] = {
      net::MockRead(net::ASYNC, kConnectResponse, strlen(kConnectResponse), 1),
      net::MockRead(net::ASYNC, kMsg, kMsgSize, 3),
      net::MockRead(net::SYNCHRONOUS, net::OK, 4)};
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::ASYNC, kConnectRequest, strlen(kConnectRequest), 0),
      net::MockWrite(net::SYNCHRONOUS, kMsg, kMsgSize, 2)};
  net::SequencedSocketData data_provider(kReads, kWrites);
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  SocketHandle client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  EXPECT_EQ(net::OK,
            CreateSocketSync(MakeRequest(&client_socket), server_addr));

  net::HostPortPair host_port_pair("example.org", 443);
  pre_tls_recv_handle()->reset();
  pre_tls_send_handle()->reset();
  net::TestCompletionCallback callback;
  mojom::TLSClientSocketPtr tls_socket;
  UpgradeToTLS(&client_socket, host_port_pair, mojo::MakeRequest(&tls_socket),
               callback.callback());
  ASSERT_EQ(net::OK, callback.WaitForResult());
  ResetSocket(&client_socket);

  uint32_t num_bytes = strlen(kMsg);
  EXPECT_EQ(MOJO_RESULT_OK, post_tls_send_handle()->get().WriteData(
                                &kMsg, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(kMsg, Read(post_tls_recv_handle(), kMsgSize));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

class TLSClientSocketIoModeTest : public TLSClientSocketTestBase,
                                  public testing::TestWithParam<net::IoMode> {
 public:
  TLSClientSocketIoModeTest()
      : TLSClientSocketTestBase(TLSClientSocketTestBase::kDirect) {
    Init(true /* use_mock_sockets*/, false /* configure_proxy */);
  }

  ~TLSClientSocketIoModeTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TLSClientSocketIoModeTest);
};

INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        TLSClientSocketIoModeTest,
                        testing::Values(net::SYNCHRONOUS, net::ASYNC));

TEST_P(TLSClientSocketIoModeTest, MultipleWriteToTLSSocket) {
  const int kNumIterations = 3;
  std::vector<net::MockRead> reads;
  std::vector<net::MockWrite> writes;
  int sequence_number = 0;
  net::IoMode mode = GetParam();
  for (int j = 0; j < kNumIterations; ++j) {
    for (size_t i = 0; i < kSecretMsgSize; ++i) {
      writes.push_back(
          net::MockWrite(mode, &kSecretMsg[i], 1, sequence_number++));
    }
    for (size_t i = 0; i < kSecretMsgSize; ++i) {
      reads.push_back(
          net::MockRead(net::ASYNC, &kSecretMsg[i], 1, sequence_number++));
    }
    if (j == kNumIterations - 1) {
      reads.push_back(net::MockRead(mode, net::OK, sequence_number++));
    }
  }
  net::SequencedSocketData data_provider(reads, writes);
  data_provider.set_connect_data(net::MockConnect(net::SYNCHRONOUS, net::OK));
  mock_client_socket_factory()->AddSocketDataProvider(&data_provider);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  mock_client_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

  mojom::TCPConnectedSocketPtr client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 1234);
  EXPECT_EQ(net::OK, CreateTCPConnectedSocketSync(
                         mojo::MakeRequest(&client_socket), server_addr));

  net::HostPortPair host_port_pair("example.org", 443);
  pre_tls_recv_handle()->reset();
  pre_tls_send_handle()->reset();
  net::TestCompletionCallback callback;
  mojom::TLSClientSocketPtr tls_socket;
  UpgradeTCPConnectedSocketToTLS(client_socket.get(), host_port_pair,
                                 mojo::MakeRequest(&tls_socket),
                                 callback.callback());
  ASSERT_EQ(net::OK, callback.WaitForResult());
  client_socket.reset();

  // Loop kNumIterations times to test that writes can follow reads, and reads
  // can follow writes.
  for (int j = 0; j < kNumIterations; ++j) {
    // Write multiple times.
    for (size_t i = 0; i < kSecretMsgSize; ++i) {
      uint32_t num_bytes = 1;
      EXPECT_EQ(MOJO_RESULT_OK,
                post_tls_send_handle()->get().WriteData(
                    &kSecretMsg[i], &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));
      // Flush the 1 byte write.
      base::RunLoop().RunUntilIdle();
    }
    // Reading kSecretMsgSize should coalesce the 1-byte mock reads.
    EXPECT_EQ(kSecretMsg, Read(post_tls_recv_handle(), kSecretMsgSize));
  }
  EXPECT_TRUE(ssl_socket.ConnectDataConsumed());
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

class TLSClientSocketTestWithEmbeddedTestServer
    : public ::testing::TestWithParam<TLSClientSocketTestBase::Mode>,
      public TLSClientSocketTestBase {
 public:
  TLSClientSocketTestWithEmbeddedTestServer()
      : TLSClientSocketTestBase(GetParam()) {
    Init(false /* use_mock_sockets */, false /* configure_proxy */);
  }

  ~TLSClientSocketTestWithEmbeddedTestServer() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TLSClientSocketTestWithEmbeddedTestServer);
};

TEST_P(TLSClientSocketTestWithEmbeddedTestServer, Basic) {
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  server.RegisterRequestHandler(
      base::BindRepeating([](const net::test_server::HttpRequest& request) {
        if (base::StartsWith(request.relative_url, "/secret",
                             base::CompareCase::INSENSITIVE_ASCII)) {
          return std::unique_ptr<net::test_server::HttpResponse>(
              new net::test_server::RawHttpResponse("HTTP/1.1 200 OK",
                                                    "Hello There!"));
        }
        return std::unique_ptr<net::test_server::HttpResponse>();
      }));
  server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  ASSERT_TRUE(server.Start());

  SocketHandle client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), server.port());
  EXPECT_EQ(net::OK,
            CreateSocketSync(MakeRequest(&client_socket), server_addr));

  pre_tls_recv_handle()->reset();
  pre_tls_send_handle()->reset();
  net::TestCompletionCallback callback;
  mojom::TLSClientSocketPtr tls_socket;
  UpgradeToTLS(&client_socket, server.host_port_pair(),
               mojo::MakeRequest(&tls_socket), callback.callback());
  ASSERT_EQ(net::OK, callback.WaitForResult());
  ResetSocket(&client_socket);

  const char kTestMsg[] = "GET /secret HTTP/1.1\r\n\r\n";
  uint32_t num_bytes = strlen(kTestMsg);
  const char kResponse[] = "HTTP/1.1 200 OK\n\n";
  EXPECT_EQ(MOJO_RESULT_OK,
            post_tls_send_handle()->get().WriteData(&kTestMsg, &num_bytes,
                                                    MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(kResponse, Read(post_tls_recv_handle(), strlen(kResponse)));
}

TEST_P(TLSClientSocketTestWithEmbeddedTestServer, ServerCertError) {
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  ASSERT_TRUE(server.Start());

  SocketHandle client_socket;
  net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), server.port());
  EXPECT_EQ(net::OK,
            CreateSocketSync(MakeRequest(&client_socket), server_addr));

  pre_tls_recv_handle()->reset();
  pre_tls_send_handle()->reset();
  net::TestCompletionCallback callback;
  mojom::TLSClientSocketPtr tls_socket;
  UpgradeToTLS(&client_socket, server.host_port_pair(),
               mojo::MakeRequest(&tls_socket), callback.callback());
  ASSERT_EQ(net::ERR_CERT_COMMON_NAME_INVALID, callback.WaitForResult());
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    TLSClientSocketTestWithEmbeddedTestServer,
    ::testing::Values(TLSClientSocketTestBase::kDirect,
                      TLSClientSocketTestBase::kProxyResolving));

}  // namespace network
