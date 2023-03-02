/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#pragma once

#include "fboss/agent/platforms/sai/SaiBcmPlatform.h"

namespace facebook::fboss {

class IndusAsic;

class SaiYangraPlatform : public SaiBcmPlatform {
 public:
  SaiYangraPlatform(
      std::unique_ptr<PlatformProductInfo> productInfo,
      folly::MacAddress localMac,
      const std::string& platformMappingStr);
  ~SaiYangraPlatform() override;
  HwAsic* getAsic() const override;

  uint32_t numLanesPerCore() const override {
    return 4;
  }

  uint32_t numCellsAvailable() const override {
    return 130665;
  }

  bool isSerdesApiSupported() const override {
    return true;
  }

  void initLEDs() override;

  std::vector<PortID> getAllPortsInGroup(PortID /*portID*/) const override {
    return {};
  }

  std::vector<FlexPortMode> getSupportedFlexPortModes() const override {
    return {};
  }

  bool supportInterfaceType() const override {
    return true;
  }

  std::optional<sai_port_interface_type_t> getInterfaceType(
      TransmitterTechnology /*transmitterTech*/,
      cfg::PortSpeed /*speed*/) const override {
    return std::nullopt;
  }

 private:
  std::vector<sai_system_port_config_t> getInternalSystemPortConfig()
      const override;
  void setupAsic(
      cfg::SwitchType switchType,
      std::optional<int64_t> switchId,
      std::optional<cfg::Range64> systemPortRange) override;
  std::unique_ptr<IndusAsic> asic_;
};

} // namespace facebook::fboss