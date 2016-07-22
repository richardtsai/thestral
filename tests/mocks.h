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
/// Defines mock classes for testing.
#ifndef THESTRAL_TESTS_MOCKS_H_
#define THESTRAL_TESTS_MOCKS_H_

#include <memory>
#include <string>

#include "base.h"
#include "common.h"

namespace thestral {
namespace testing {

struct MockTransport : public TransportBase {
  static std::shared_ptr<MockTransport> New(const std::string& read_buf = "");

  Address GetLocalAddress() const override { return local_address; }

  void StartRead(const boost::asio::mutable_buffers_1& buf,
                 ReadCallbackType callback,
                 bool allow_short_read = false) override;
  void StartWrite(const boost::asio::const_buffers_1& buf,
                  WriteCallbackType callback) override;
  void StartClose(CloseCallbackType callback) override;

  using TransportBase::StartRead;
  using TransportBase::StartWrite;
  using TransportBase::StartClose;

  Address local_address;
  std::string read_buf;
  std::string write_buf;
  ec_type ec;
  bool closed = false;
};

}  // namespace testing
}  // namespace thestral
#endif  // THESTRAL_TESTS_MOCKS_H_