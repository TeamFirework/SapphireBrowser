// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PROXY_RESOLVING_SOCKET_MOJO_H_
#define SERVICES_NETWORK_PROXY_RESOLVING_SOCKET_MOJO_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/proxy_resolving_client_socket.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "services/network/socket_data_pump.h"
#include "services/network/tls_socket_factory.h"

namespace network {

class SocketDataPump;

class COMPONENT_EXPORT(NETWORK_SERVICE) ProxyResolvingSocketMojo
    : public mojom::ProxyResolvingSocket,
      public SocketDataPump::Delegate,
      public TLSSocketFactory::Delegate {
 public:
  ProxyResolvingSocketMojo(
      std::unique_ptr<ProxyResolvingClientSocket> socket,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      TLSSocketFactory* tls_socket_factory);
  ~ProxyResolvingSocketMojo() override;
  void Connect(
      mojom::ProxyResolvingSocketFactory::CreateProxyResolvingSocketCallback
          callback);

  // mojom::ProxyResolvingSocket implementation.
  void UpgradeToTLS(
      const net::HostPortPair& host_port_pair,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojom::TLSClientSocketRequest request,
      mojom::SocketObserverPtr observer,
      mojom::ProxyResolvingSocket::UpgradeToTLSCallback callback) override;

 private:
  void OnConnectCompleted(int net_result);

  // SocketDataPump::Delegate implementation.
  void OnNetworkReadError(int net_error) override;
  void OnNetworkWriteError(int net_error) override;
  void OnShutdown() override;

  // TLSSocketFactory::Delegate implementation.
  const net::StreamSocket* BorrowSocket() override;
  std::unique_ptr<net::StreamSocket> TakeSocket() override;

  TLSSocketFactory* tls_socket_factory_;
  std::unique_ptr<ProxyResolvingClientSocket> socket_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;
  mojom::ProxyResolvingSocketFactory::CreateProxyResolvingSocketCallback
      connect_callback_;
  base::OnceClosure pending_upgrade_to_tls_callback_;
  std::unique_ptr<SocketDataPump> socket_data_pump_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolvingSocketMojo);
};

}  // namespace network

#endif  // SERVICES_NETWORK_PROXY_RESOLVING_SOCKET_MOJO_H_
