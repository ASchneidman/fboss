// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#include <fb303/ServiceData.h>
#include "fboss/agent/SwitchStats.h"
#include "fboss/agent/Utils.h"
#include "fboss/agent/hw/HwSwitchFb303Stats.h"
#include "fboss/agent/hw/test/ConfigFactory.h"
#include "fboss/agent/hw/test/HwLinkStateDependentTest.h"
#include "fboss/lib/CommonUtils.h"
#include "folly/experimental/TestUtil.h"

namespace facebook::fboss {
class HwVoqSwitchInterruptTest : public HwLinkStateDependentTest {
 public:
  HwSwitchEnsemble::Features featuresDesired() const override {
    return {HwSwitchEnsemble::LINKSCAN, HwSwitchEnsemble::TAM_NOTIFY};
  }
  cfg::SwitchConfig initialConfig() const override {
    return utility::onePortPerInterfaceConfig(
        getHwSwitch(),
        masterLogicalPortIds(),
        getAsic()->desiredLoopbackModes(),
        true /*interfaceHasSubnet*/);
  }

 protected:
  void runCint(const std::string cintStr) {
    folly::test::TemporaryFile file;
    XLOG(INFO) << " Cint file " << file.path().c_str();
    folly::writeFull(file.fd(), cintStr.c_str(), cintStr.size());
    std::string out;
    getHwSwitchEnsemble()->runDiagCommand(
        folly::sformat("cint {}\n", file.path().c_str()), out);
    getHwSwitchEnsemble()->runDiagCommand("quit\n", out);
  }
};

TEST_F(HwVoqSwitchInterruptTest, ireError) {
  auto verify = [=, this]() {
    constexpr auto kIreErrorIncjectorCintStr = R"(
  cint_reset();
  bcm_switch_event_control_t event_ctrl;
  event_ctrl.event_id = 2034;
  event_ctrl.index = 0; /* core ID */
  event_ctrl.action = bcmSwitchEventForce;
  print bcm_switch_event_control_set(0, BCM_SWITCH_EVENT_DEVICE_INTERRUPT, event_ctrl, 1);
  )";
    runCint(kIreErrorIncjectorCintStr);
    WITH_RETRIES({
      getHwSwitch()->updateStats();
      fb303::ThreadCachedServiceData::get()->publishStats();
      auto ireErrors = getHwSwitch()
                           ->getSwitchStats()
                           ->getHwAsicErrors()
                           .ingressReceiveEditorErrors()
                           .value_or(0);
      XLOG(INFO) << " IRE Errors: " << ireErrors;
      EXPECT_EVENTUALLY_GT(ireErrors, 0);
      EXPECT_EVENTUALLY_GT(getHwSwitch()->getSwitchStats()->getIreErrors(), 0);
    });
  };
  verifyAcrossWarmBoots([]() {}, verify);
}

TEST_F(HwVoqSwitchInterruptTest, itppError) {
  auto verify = [=, this]() {
    std::string out;
    getHwSwitchEnsemble()->runDiagCommand(
        "s itpp_interrupt_mask_register 0x3f\n", out);
    getHwSwitchEnsemble()->runDiagCommand(
        "s itpp_interrupt_register_test 0x2\n", out);
    getHwSwitchEnsemble()->runDiagCommand(
        "s itppd_interrupt_mask_register 0x3f\n", out);
    getHwSwitchEnsemble()->runDiagCommand(
        "s itppd_interrupt_register_test 0x2\n", out);
    getHwSwitchEnsemble()->runDiagCommand("quit\n", out);
    WITH_RETRIES({
      getHwSwitch()->updateStats();
      fb303::ThreadCachedServiceData::get()->publishStats();
      auto itppErrors = getHwSwitch()
                            ->getSwitchStats()
                            ->getHwAsicErrors()
                            .ingressTransmitPipelineErrors()
                            .value_or(0);
      XLOG(INFO) << " ITPP Errors: " << itppErrors;
      EXPECT_EVENTUALLY_GE(itppErrors, 2);
      EXPECT_EVENTUALLY_GE(getHwSwitch()->getSwitchStats()->getItppErrors(), 2);
    });
  };
  verifyAcrossWarmBoots([]() {}, verify);
}

TEST_F(HwVoqSwitchInterruptTest, epniError) {
  auto verify = [=, this]() {
    constexpr auto kEpniErrorIncjectorCintStr = R"(
  cint_reset();
  bcm_switch_event_control_t event_ctrl;
  event_ctrl.event_id = 717;  // JR3_INT_EPNI_FIFO_OVERFLOW_INT
  event_ctrl.index = 0; /* core ID */
  event_ctrl.action = bcmSwitchEventForce;
  print bcm_switch_event_control_set(0, BCM_SWITCH_EVENT_DEVICE_INTERRUPT, event_ctrl, 1);
  )";
    runCint(kEpniErrorIncjectorCintStr);
    WITH_RETRIES({
      getHwSwitch()->updateStats();
      fb303::ThreadCachedServiceData::get()->publishStats();
      auto epniErrors = getHwSwitch()
                            ->getSwitchStats()
                            ->getHwAsicErrors()
                            .egressPacketNetworkInterfaceErrors()
                            .value_or(0);
      XLOG(INFO) << " EPNI Errors: " << epniErrors;
      EXPECT_EVENTUALLY_GT(epniErrors, 0);
      EXPECT_EVENTUALLY_GT(getHwSwitch()->getSwitchStats()->getEpniErrors(), 0);
    });
  };
  verifyAcrossWarmBoots([]() {}, verify);
}

TEST_F(HwVoqSwitchInterruptTest, alignerError) {
  auto verify = [=, this]() {
    constexpr auto kAlignerErrorIncjectorCintStr = R"(
  cint_reset();
  bcm_switch_event_control_t event_ctrl;
  event_ctrl.event_id = 8;  // JR3_INT_ALIGNER_PKT_SIZE_EOP_MISMATCH_INT
  event_ctrl.index = 0; /* core ID */
  event_ctrl.action = bcmSwitchEventForce;
  print bcm_switch_event_control_set(0, BCM_SWITCH_EVENT_DEVICE_INTERRUPT, event_ctrl, 1);
  )";
    runCint(kAlignerErrorIncjectorCintStr);
    WITH_RETRIES({
      getHwSwitch()->updateStats();
      fb303::ThreadCachedServiceData::get()->publishStats();
      auto alignerErrors = getHwSwitch()
                               ->getSwitchStats()
                               ->getHwAsicErrors()
                               .alignerErrors()
                               .value_or(0);
      XLOG(INFO) << " Aligner Errors: " << alignerErrors;
      EXPECT_EVENTUALLY_GT(alignerErrors, 0);
      EXPECT_EVENTUALLY_GT(
          getHwSwitch()->getSwitchStats()->getAlignerErrors(), 0);
    });
  };
  verifyAcrossWarmBoots([]() {}, verify);
}

TEST_F(HwVoqSwitchInterruptTest, fqpError) {
  auto verify = [=, this]() {
    constexpr auto kFqpErrorIncjectorCintStr = R"(
  cint_reset();
  bcm_switch_event_control_t event_ctrl;
  event_ctrl.event_id = 1294;  // JR3_INT_FQP_ECC_ECC_1B_ERR_INT
  event_ctrl.index = 0; /* core ID */
  event_ctrl.action = bcmSwitchEventForce;
  print bcm_switch_event_control_set(0, BCM_SWITCH_EVENT_DEVICE_INTERRUPT, event_ctrl, 1);
  )";
    runCint(kFqpErrorIncjectorCintStr);
    WITH_RETRIES({
      getHwSwitch()->updateStats();
      fb303::ThreadCachedServiceData::get()->publishStats();
      auto fqpErrors = getHwSwitch()
                           ->getSwitchStats()
                           ->getHwAsicErrors()
                           .forwardingQueueProcessorErrors()
                           .value_or(0);
      XLOG(INFO) << " FQP Errors: " << fqpErrors;
      EXPECT_EVENTUALLY_GT(fqpErrors, 0);
      EXPECT_EVENTUALLY_GT(
          getHwSwitch()->getSwitchStats()->getForwardingQueueProcessorErrors(),
          0);
    });
  };
  verifyAcrossWarmBoots([]() {}, verify);
}
} // namespace facebook::fboss
