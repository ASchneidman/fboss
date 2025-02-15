// Copyright 2004-present Facebook. All Rights Reserved.

#include "fboss/agent/hw/test/HwTestPacketSnooper.h"
#include "fboss/agent/RxPacket.h"
#include "fboss/agent/packet/PktUtil.h"

#include <folly/logging/xlog.h>

namespace facebook::fboss {

HwTestPacketSnooper::HwTestPacketSnooper(
    HwSwitchEnsemble* ensemble,
    std::optional<PortID> port,
    std::optional<utility::EthFrame> expectedFrame)
    : ensemble_(ensemble), port_(port), expectedFrame_(expectedFrame) {
  ensemble_->addHwEventObserver(this);
}

HwTestPacketSnooper::~HwTestPacketSnooper() {
  ensemble_->removeHwEventObserver(this);
}

void HwTestPacketSnooper::packetReceived(RxPacket* pkt) noexcept {
  XLOG(DBG2) << "pkt received on port " << pkt->getSrcPort();
  if (port_.has_value() && port_.value() != pkt->getSrcPort()) {
    // packet arrived on port not of interest.
    return;
  }
  auto data = pkt->buf()->clone();
  folly::io::Cursor cursor{data.get()};
  auto frame = std::make_unique<utility::EthFrame>(cursor);
  if (expectedFrame_.has_value() && *expectedFrame_ != *frame) {
    XLOG(DBG2) << " Unexpected packet received "
               << " expected: " << *expectedFrame_ << std::endl
               << " got: " << *frame;
    return;
  }
  std::lock_guard<std::mutex> lock(mtx_);
  receivedFrame_ = std::move(frame);
  cv_.notify_all();
}

std::optional<utility::EthFrame> HwTestPacketSnooper::waitForPacket(
    uint32_t timeout_s) {
  std::unique_lock<std::mutex> lock(mtx_);
  while (!receivedFrame_) {
    if (timeout_s > 0) {
      if (cv_.wait_for(lock, std::chrono::seconds(timeout_s)) ==
          std::cv_status::timeout) {
        return {};
      }
    } else {
      cv_.wait(lock);
    }
  }
  return *receivedFrame_;
}

} // namespace facebook::fboss
