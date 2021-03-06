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
/// Implements classes for ssl.
#include "ssl.h"

namespace thestral {
namespace ssl {

namespace ip = boost::asio::ip;

namespace impl {

SslTransportImpl::SslTransportImpl(boost::asio::io_service& io_service,
                                   boost::asio::ssl::context& ssl_ctx)
    : ssl_sock_(io_service, ssl_ctx) {}

void SslTransportImpl::StartRead(const boost::asio::mutable_buffers_1& buf,
                                 const ReadCallbackType& callback,
                                 bool allow_short_read) {
  if (allow_short_read) {
    ssl_sock_.async_read_some(buf, callback);
  } else {
    boost::asio::async_read(ssl_sock_, buf, callback);
  }
}

void SslTransportImpl::StartWrite(const boost::asio::const_buffers_1& buf,
                                  const WriteCallbackType& callback) {
  boost::asio::async_write(ssl_sock_, buf, callback);
}

void SslTransportImpl::StartClose(const CloseCallbackType& callback) {
  // openssl will crash if the socket is destroyed before shutdown operation
  // completes
  auto self = shared_from_this();
  ssl_sock_.async_shutdown(
      [self, callback](const ec_type& ec) { callback(ec); });
}

logging::Logger SslTransportFactoryImpl::LOG("SslTransportFactoryImpl");

void SslTransportFactoryImpl::StartAccept(EndpointType endpoint,
                                          const AcceptCallbackType& callback) {
  LOG.Debug("start accepting");
  auto acceptor =
      std::make_shared<ip::tcp::acceptor>(*io_service_ptr_, endpoint);
  acceptor->set_option(ip::tcp::no_delay(true));
  acceptor->set_option(ip::tcp::socket::reuse_address(true));
  last_acceptor_ = acceptor;
  DoAccept(acceptor, callback);
}

void SslTransportFactoryImpl::DoAccept(
    const std::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
    const AcceptCallbackType& callback) {
  std::shared_ptr<SslTransportImpl> transport(
      new SslTransportImpl(*io_service_ptr_, ssl_ctx_));

  auto self = shared_from_this();
  LOG.Debug("[%llX] waiting for one connection", transport->GetId());
  acceptor->async_accept(
      transport->ssl_sock_.lowest_layer(),
      [self, acceptor, callback, transport](const ec_type& ec) {
        if (ec) {
          self->LOG.Debug(
              "[%llX] acceptor returning an error: %s, stop accepting",
              transport->GetId(), ec.message().c_str());
          // accept() call failed. impossible to proceed.
          transport->StartClose();
          callback(ec, nullptr);
          return;
        } else {
          self->LOG.Debug(
              "[%llX] one connection accepted, start performing ssl handshake",
              transport->GetId());
        }

        transport->ssl_sock_.async_handshake(
            boost::asio::ssl::stream_base::server,
            [self, acceptor, callback, transport](const ec_type& ec) {
              if (ec) {
                self->LOG.Debug(
                    "[%llX] ssl transport returning an error on handshake: %s,"
                    " remote endpoint: %s",
                    transport->GetId(), ec.message().c_str(),
                    transport->GetRemoteAddress().ToString().c_str());
                transport->StartClose();
              } else {
                self->LOG.Debug(
                    "[%llX] ssl handshake succeeded, remote endpoint: %s",
                    transport->GetId(),
                    transport->GetRemoteAddress().ToString().c_str());
              }
              if (callback(ec, ec ? nullptr : transport)) {
                self->DoAccept(acceptor, callback);
              } else {
                self->LOG.Debug(
                    "[%llX] upper layer gave up accepting more connections",
                    transport->GetId());
              }
            });
      });
}

void SslTransportFactoryImpl::StartConnect(
    EndpointType endpoint, const ConnectCallbackType& callback) {
  std::shared_ptr<SslTransportImpl> transport(
      new SslTransportImpl(*io_service_ptr_, ssl_ctx_));
  auto self = shared_from_this();
  LOG.Debug("[%llX] start connecting", transport->GetId());
  transport->ssl_sock_.lowest_layer().async_connect(
      endpoint, [self, transport, callback](const ec_type& ec) {
        if (ec) {
          self->LOG.Debug("[%llX] ssl transport returning an error: %s",
                          transport->GetId(), ec.message().c_str());
          transport->StartClose();
          callback(ec, nullptr);
        } else {
          self->LOG.Debug(
              "[%llX] connection established, start performing ssl handshake",
              transport->GetId());
        }
        transport->ssl_sock_.lowest_layer().set_option(ip::tcp::no_delay(true));
        transport->ssl_sock_.async_handshake(
            boost::asio::ssl::stream_base::client,
            [self, callback, transport](const ec_type& ec) {
              if (ec) {
                self->LOG.Debug(
                    "[%llX] ssl transport returning an error on handshake: %s",
                    transport->GetId(), ec.message().c_str());
                transport->StartClose();
                callback(ec, nullptr);
              } else {
                self->LOG.Debug("[%llX] ssl handshake succeeded",
                                transport->GetId());
                callback(ec, transport);
              }
            });
      });
}

std::shared_ptr<TransportBase> SslTransportFactoryImpl::TryConnect(
    boost::asio::ip::tcp::resolver::iterator& iter, ec_type& error_code) {
  std::shared_ptr<SslTransportImpl> transport(
      new SslTransportImpl(*io_service_ptr_, ssl_ctx_));
  LOG.Debug("[%llX] start connecting", transport->GetId());
  iter = boost::asio::connect(transport->ssl_sock_.lowest_layer(), iter,
                              error_code);
  if (!error_code) {
    LOG.Debug("[%llX] connection established, start performing ssl handshake",
              transport->GetId());
    transport->ssl_sock_.lowest_layer().set_option(ip::tcp::no_delay(true));
    transport->ssl_sock_.handshake(boost::asio::ssl::stream_base::client,
                                   error_code);
  }

  if (error_code) {
    LOG.Debug("[%llX] ssl transport returning an error: %s", transport->GetId(),
              error_code.message().c_str());
    transport->StartClose();
    return nullptr;
  }

  LOG.Debug("[%llX] ssl handshake succeeded", transport->GetId());
  return transport;
}

}  // namespace impl

SslTransportFactoryBuilder::SslTransportFactoryBuilder()
    : ssl_ctx_(boost::asio::ssl::context::sslv23), used_(false) {
  ssl_ctx_.set_options(boost::asio::ssl::context::no_sslv2 |
                       boost::asio::ssl::context::no_sslv3 |
                       boost::asio::ssl::context::no_tlsv1 |
                       boost::asio::ssl::context::single_dh_use |
                       boost::asio::ssl::context::default_workarounds);
}

std::shared_ptr<TcpTransportFactory> SslTransportFactoryBuilder::Build(
    const std::shared_ptr<boost::asio::io_service>& io_service_ptr) {
  if (used_) {
    return nullptr;
  }
  used_ = true;
  return std::shared_ptr<TcpTransportFactory>(
      new impl::SslTransportFactoryImpl(io_service_ptr, std::move(ssl_ctx_)));
}

SslTransportFactoryBuilder& SslTransportFactoryBuilder::AddCaPath(
    const std::string& path) {
  ssl_ctx_.add_verify_path(path);
  return *this;
}

SslTransportFactoryBuilder& SslTransportFactoryBuilder::LoadCaFile(
    const std::string& pem_file) {
  ssl_ctx_.load_verify_file(pem_file);
  return *this;
}

SslTransportFactoryBuilder& SslTransportFactoryBuilder::LoadCert(
    const std::string& pem_file) {
  ssl_ctx_.use_certificate_file(pem_file, boost::asio::ssl::context::pem);
  return *this;
}

SslTransportFactoryBuilder& SslTransportFactoryBuilder::LoadCertChain(
    const std::string& pem_file) {
  ssl_ctx_.use_certificate_chain_file(pem_file);
  return *this;
}

SslTransportFactoryBuilder& SslTransportFactoryBuilder::LoadPrivateKey(
    const std::string& pem_file) {
  ssl_ctx_.use_private_key_file(pem_file, boost::asio::ssl::context::pem);
  return *this;
}

SslTransportFactoryBuilder& SslTransportFactoryBuilder::LoadDhParams(
    const std::string& file) {
  ssl_ctx_.use_tmp_dh_file(file);
  return *this;
}

SslTransportFactoryBuilder& SslTransportFactoryBuilder::SetVerifyDepth(
    int depth) {
  ssl_ctx_.set_verify_depth(depth);
  return *this;
}

SslTransportFactoryBuilder& SslTransportFactoryBuilder::SetVerifyPeer(
    bool verify) {
  if (verify) {
    ssl_ctx_.set_verify_mode(boost::asio::ssl::verify_peer |
                             boost::asio::ssl::verify_fail_if_no_peer_cert |
                             boost::asio::ssl::verify_client_once);
  } else {
    ssl_ctx_.set_verify_mode(boost::asio::ssl::verify_none);
  }
  return *this;
}

SslTransportFactoryBuilder& SslTransportFactoryBuilder::SetVerifyHost(
    const std::string& host) {
  ssl_ctx_.set_verify_callback(boost::asio::ssl::rfc2818_verification(host));
  return *this;
}

}  // namespace ssl
}  // namespace thestral
