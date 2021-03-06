// Copyright 2016 Richard Tsai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/// @file
/// Defines a transport on plain TCP protocol.
#ifndef THESTRAL_TCP_TRANSPORT_H_
#define THESTRAL_TCP_TRANSPORT_H_

#include <memory>

#include <boost/asio.hpp>

#include "base.h"
#include "common.h"
#include "logging.h"

namespace thestral {

namespace testing {
class TestTcpTransportFactory;
}

/// Base class of transport on plain TCP protocol.
class TcpTransport : public TransportBase {
 public:
  /// Returns a reference to the underlying tcp socket.
  virtual boost::asio::ip::tcp::socket& GetUnderlyingSocket() = 0;
};

/// Base class of transport factory for TcpTransport.
class TcpTransportFactory
    : public TransportFactoryBase<boost::asio::ip::tcp::endpoint> {
 public:
  /// Synchronously tries connecting to a remote peer with a set of resolver
  /// results.
  /// @param iter An iterator reference to the resolver result set. It will be
  /// advanced to the first connectable result.
  /// @param error_code A reference to the error code object and will be set to
  /// the resulting error code.
  virtual std::shared_ptr<TransportBase> TryConnect(
      boost::asio::ip::tcp::resolver::iterator& iter, ec_type& error_code) = 0;

  static std::shared_ptr<TcpTransportFactory> New(
      const std::shared_ptr<boost::asio::io_service>& io_service_ptr);

 protected:
  friend class testing::TestTcpTransportFactory;

  /// A weak pointer to the last created acceptor **for testing purposes only**.
  std::weak_ptr<boost::asio::ip::tcp::acceptor> last_acceptor_;
};

namespace testing {
/// This class is a friend of TcpTransportFactory and tests that need to access
/// private/protected members of TcpTransportFactory objects should use it as a
/// fixture.
struct TestTcpTransportFactory {
  static std::weak_ptr<boost::asio::ip::tcp::acceptor> GetLastAcceptor(
      const std::shared_ptr<TcpTransportFactory>& transport_factory) {
    return transport_factory->last_acceptor_;
  }
};
}  // namespace testing

namespace impl {

/// Implementation of TcpTransport on plain tcp protocol.
class TcpTransportImpl : public TcpTransport {
 public:
  Address GetLocalAddress() const override;
  Address GetRemoteAddress() const override;
  void StartRead(const boost::asio::mutable_buffers_1& buf,
                 const ReadCallbackType& callback,
                 bool allow_short_read = false) override;
  void StartWrite(const boost::asio::const_buffers_1& buf,
                  const WriteCallbackType& callback) override;
  void StartClose(const CloseCallbackType& callback) override;
  using TransportBase::StartClose;

  boost::asio::ip::tcp::socket& GetUnderlyingSocket() override {
    return socket_;
  }

 private:
  friend class TcpTransportFactoryImpl;

  explicit TcpTransportImpl(boost::asio::io_service& io_service);

  boost::asio::ip::tcp::socket socket_;
};

/// Implementation of TcpTransportFactory on plain tcp protocol.
class TcpTransportFactoryImpl
    : public TcpTransportFactory,
      public std::enable_shared_from_this<TcpTransportFactoryImpl> {
 public:
  TcpTransportFactoryImpl(const TcpTransportFactoryImpl&) = delete;
  TcpTransportFactoryImpl& operator=(const TcpTransportFactoryImpl&) = delete;

  static std::shared_ptr<TcpTransportFactoryImpl> New(
      const std::shared_ptr<boost::asio::io_service>& io_service_ptr) {
    return std::shared_ptr<TcpTransportFactoryImpl>(
        new TcpTransportFactoryImpl(io_service_ptr));
  }

  void StartAccept(EndpointType endpoint,
                   const AcceptCallbackType& callback) override;
  void StartConnect(EndpointType endpoint,
                    const ConnectCallbackType& callback) override;
  std::shared_ptr<TransportBase> TryConnect(
      boost::asio::ip::tcp::resolver::iterator& iter,
      ec_type& error_code) override;

  std::shared_ptr<boost::asio::io_service> get_io_service_ptr() const override {
    return io_service_ptr_;
  }

 private:
  explicit TcpTransportFactoryImpl(
      const std::shared_ptr<boost::asio::io_service>& io_service_ptr)
      : io_service_ptr_(io_service_ptr) {}

  std::shared_ptr<TcpTransportImpl> NewTransport() const {
    return std::shared_ptr<TcpTransportImpl>(
        new TcpTransportImpl(*io_service_ptr_));
  }

  void DoAccept(const std::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
                const AcceptCallbackType& callback);

  const std::shared_ptr<boost::asio::io_service> io_service_ptr_;
  static logging::Logger LOG;
};

}  // namespace impl
}  // namespace thestral

#endif  // THESTRAL_TCP_TRANSPORT_H_
