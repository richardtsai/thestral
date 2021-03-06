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
/// Implements the upstream for direct access.
#include "direct_upstream.h"

namespace ip = boost::asio::ip;

namespace thestral {

logging::Logger DirectTcpUpstreamFactory::LOG("DirectTcpUpstreamFactory");

void DirectTcpUpstreamFactory::StartRequest(
    const Address& address, const RequestCallbackType& callback) {
  LOG.Info("sending request to %s", address.ToString().c_str());
  switch (address.type) {
    case AddressType::kDomainName: {
      ip::tcp::resolver::query query(
          address.host, std::to_string(address.port),
          ip::tcp::resolver::query::address_configured |
              ip::tcp::resolver::query::numeric_service);
      auto self = shared_from_this();
      LOG.Debug("resolving address %s", address.host.c_str());
      resolver_.async_resolve(
          query, [self, address, callback](const ec_type& ec,
                                           ip::tcp::resolver::iterator iter) {
            if (ec) {
              self->LOG.Error("failed to resolve address %s, reason: %s",
                              address.host.c_str(), ec.message().c_str());
              callback(ec, nullptr);
            } else {
              // more than one results?
              self->transport_factory_->StartConnect(*iter, callback);
            }
          });
      break;
    }
    case AddressType::kIPv4: {
      ip::address_v4::bytes_type address_bytes;
      std::copy(address.host.cbegin(), address.host.cend(),
                address_bytes.begin());
      ip::address_v4 asio_addr(address_bytes);
      transport_factory_->StartConnect(
          ip::tcp::endpoint(asio_addr, address.port), callback);
      break;
    }
    case AddressType::kIPv6: {
      ip::address_v6::bytes_type address_bytes;
      std::copy(address.host.cbegin(), address.host.cend(),
                address_bytes.begin());
      ip::address_v6 asio_addr(address_bytes);
      transport_factory_->StartConnect(
          ip::tcp::endpoint(asio_addr, address.port), callback);
      break;
    }
    default:
      // unknown address type
      // TODO(richardtsai): report error to the callback
      LOG.Error("unknown address type: %s", to_string(address.type).c_str());
      callback(ec_type(), nullptr);
      break;
  }
}

}  // namespace thestral
