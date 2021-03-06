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
/// Defines some common types and utilities.
#ifndef THESTRAL_COMMON_H_
#define THESTRAL_COMMON_H_

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

#include <boost/asio/ip/address.hpp>
#include <boost/preprocessor/seq.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/rem.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>

#define _THESTRAL_DEFINE_ENUM_ELEM(name, value) name = value,
#define _THESTRAL_DEFINE_ENUM_ELEMS(r, d, elm) _THESTRAL_DEFINE_ENUM_ELEM elm

#define _THESTRAL_DEFINE_ENUM_OUTPUT_1(type_name, name) \
  case type_name::name:                                 \
    return #type_name "::" BOOST_PP_STRINGIZE(name);    \
    break;
#define _THESTRAL_DEFINE_ENUM_OUTPUT(r, type_name, elm) \
  _THESTRAL_DEFINE_ENUM_OUTPUT_1(type_name, BOOST_PP_TUPLE_ELEM(0, elm))

#define THESTRAL_DEFINE_ENUM(type_name, base_type, ...)              \
  enum class type_name : base_type {                                 \
    BOOST_PP_SEQ_FOR_EACH(_THESTRAL_DEFINE_ENUM_ELEMS, $,            \
                          BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))     \
  };                                                                 \
  static inline std::string to_string(type_name val) {               \
    switch (val) {                                                   \
      BOOST_PP_SEQ_FOR_EACH(_THESTRAL_DEFINE_ENUM_OUTPUT, type_name, \
                            BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))   \
      default:                                                       \
        return #type_name " (INVALID VALUE: " +                      \
               std::to_string(static_cast<base_type>(val)) + ")";    \
    }                                                                \
  }                                                                  \
  template <typename C, typename T>                                  \
  std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>& os, \
                                       type_name val) {              \
    os << to_string(val);                                            \
    return os;                                                       \
  }

namespace thestral {

/// Types of addresses. Its values follow the definition of SOCKS protocol.
THESTRAL_DEFINE_ENUM(AddressType, uint8_t, (kIPv4, 0x1), (kDomainName, 0x3),
                     (kIPv6, 0x4));

/// Address type used across the program.
struct Address {
  AddressType type = AddressType::kIPv4;  ///< Type of the address
  std::string host{0, 0, 0, 0};           ///< Host string of the address
  uint16_t port = 0;                      ///< Port number of the address

  bool operator==(const Address& other) const {
    return type == other.type && host == other.host && port == other.port;
  }

  std::string ToString() const {
    std::ostringstream buf;
    switch (type) {
      case AddressType::kIPv4: {
        boost::asio::ip::address_v4::bytes_type bytes;
        host.copy(reinterpret_cast<char*>(bytes.data()), bytes.size());
        boost::asio::ip::address_v4 addr(bytes);
        buf << addr.to_string() << ':' << port;
        break;
      }
      case AddressType::kIPv6: {
        boost::asio::ip::address_v6::bytes_type bytes;
        host.copy(reinterpret_cast<char*>(bytes.data()), bytes.size());
        boost::asio::ip::address_v6 addr(bytes);
        buf << '[' << addr.to_string() << "]:" << port;
        break;
      }
      case AddressType::kDomainName: {
        buf << host << ':' << port;
        break;
      }
      default:
        buf << "UNKNOWN ADDRESS TYPE";
    }
    return buf.str();
  }

  /// Creates an Address from an asio endpoint. The type of the returned object
  /// will be set to `0xff` if the given endpoint is invalid.
  template <typename EndpointType>
  static Address FromAsioEndpoint(const EndpointType& endpoint) {
    Address address;
    auto asio_addr = endpoint.address();

    if (asio_addr.is_v4()) {
      auto asio_addr_bytes = asio_addr.to_v4().to_bytes();
      address.type = AddressType::kIPv4;
      address.host.assign(asio_addr_bytes.cbegin(), asio_addr_bytes.cend());
      address.port = endpoint.port();

    } else if (asio_addr.is_v6()) {
      auto asio_addr_bytes = asio_addr.to_v6().to_bytes();
      address.type = AddressType::kIPv6;
      address.host.assign(asio_addr_bytes.cbegin(), asio_addr_bytes.cend());
      address.port = endpoint.port();

    } else {
      address.type = static_cast<AddressType>(0xff);  // invalid address
    }

    return address;
  }
};

template <typename C, typename T>
std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>& os,
                                     const Address& address) {
  os << "Address (type: " << address.type << ", host: " << address.host
     << ", port: " << address.port << ")";
  return os;
}

}  // namespace thestral
#endif  // THESTRAL_COMMON_H_
