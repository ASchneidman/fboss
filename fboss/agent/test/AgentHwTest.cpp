// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "fboss/agent/test/AgentHwTest.h"
#include "fboss/agent/HwAsicTable.h"
#include "fboss/agent/hw/test/ConfigFactory.h"

DEFINE_bool(run_forever, false, "run the test forever");
DEFINE_bool(run_forever_on_failure, false, "run the test forever on failure");
DEFINE_bool(
    list_asic_feature,
    false,
    "list asic feature needed for every single test");

namespace {
int kArgc;
char** kArgv;
} // namespace

namespace facebook::fboss {
void AgentHwTest::SetUp() {
  gflags::ParseCommandLineFlags(&kArgc, &kArgv, false);
  if (FLAGS_list_asic_feature) {
    printAsicFeatures();
    return;
  }
  fbossCommonInit(kArgc, kArgv);
  FLAGS_verify_apply_oper_delta = true;
  FLAGS_hide_fabric_ports = hideFabricPorts();
  // Reset any global state being tracked in singletons
  // Each test then sets up its own state as needed.
  folly::SingletonVault::singleton()->destroyInstances();
  folly::SingletonVault::singleton()->reenableInstances();
  // Set watermark stats update interval to 0 so we always refresh BST stats
  // in each updateStats call
  FLAGS_update_watermark_stats_interval_s = 0;

  AgentEnsembleSwitchConfigFn initialConfigFn =
      [this](const AgentEnsemble& ensemble) { return initialConfig(ensemble); };
  agentEnsemble_ = createAgentEnsemble(initialConfigFn);
}

void AgentHwTest::TearDown() {
  if (FLAGS_run_forever ||
      (::testing::Test::HasFailure() && FLAGS_run_forever_on_failure)) {
    runForever();
  }
  tearDownAgentEnsemble();
}

void AgentHwTest::tearDownAgentEnsemble(bool doWarmboot) {
  if (!agentEnsemble_) {
    return;
  }

  if (::testing::Test::HasFailure()) {
    // TODO: Collect Info and dump counters
  }
  if (doWarmboot) {
    agentEnsemble_->gracefulExit();
  }
  agentEnsemble_.reset();
}

void AgentHwTest::runForever() const {
  XLOG(DBG2) << "AgentHwTest run forever...";
  while (true) {
    sleep(1);
    XLOG_EVERY_MS(DBG2, 5000) << "AgentHwTest running forever";
  }
}

std::shared_ptr<SwitchState> AgentHwTest::applyNewConfig(
    const cfg::SwitchConfig& config) {
  return agentEnsemble_->applyNewConfig(config);
}

SwSwitch* AgentHwTest::getSw() const {
  return agentEnsemble_->getSw();
}

const std::map<SwitchID, const HwAsic*> AgentHwTest::getAsics() const {
  return agentEnsemble_->getSw()->getHwAsicTable()->getHwAsics();
}

bool AgentHwTest::isSupportedOnAllAsics(HwAsic::Feature feature) const {
  // All Asics supporting the feature
  return agentEnsemble_->getSw()->getHwAsicTable()->isFeatureSupportedOnAllAsic(
      feature);
}

AgentEnsemble* AgentHwTest::getAgentEnsemble() const {
  return agentEnsemble_.get();
}

const std::shared_ptr<SwitchState> AgentHwTest::getProgrammedState() const {
  return getAgentEnsemble()->getProgrammedState();
}

std::vector<PortID> AgentHwTest::masterLogicalPortIds() const {
  return getAgentEnsemble()->masterLogicalPortIds();
}

std::vector<PortID> AgentHwTest::masterLogicalPortIds(
    const std::set<cfg::PortType>& portTypes) const {
  return getAgentEnsemble()->masterLogicalPortIds(portTypes);
}

void AgentHwTest::setSwitchDrainState(
    const cfg::SwitchConfig& curConfig,
    cfg::SwitchDrainState drainState) {
  auto newCfg = curConfig;
  *newCfg.switchSettings()->switchDrainState() = drainState;
  applyNewConfig(newCfg);
}

bool AgentHwTest::hideFabricPorts() const {
  // Due to the speedup in test run time (6m->21s on meru400biu)
  // we want to skip over fabric ports in a overwhelming
  // majority of test cases. Make this the default HwTest mode
  return true;
}

cfg::SwitchConfig AgentHwTest::initialConfig(
    const AgentEnsemble& ensemble) const {
  // Before m-mpu agent test, use first Asic for initialization.
  auto switchIds = ensemble.getSw()->getHwAsicTable()->getSwitchIDs();
  CHECK_GE(switchIds.size(), 1);
  auto asic =
      ensemble.getSw()->getHwAsicTable()->getHwAsic(*switchIds.cbegin());
  return utility::onePortPerInterfaceConfig(
      ensemble.getSw()->getPlatformMapping(),
      asic,
      ensemble.masterLogicalPortIds(),
      asic->desiredLoopbackModes(),
      true /*interfaceHasSubnet*/);
}

void AgentHwTest::printAsicFeatures() const {
  std::vector<std::string> asicFeatures;
  for (const auto& feature : getProductionFeaturesVerified()) {
    asicFeatures.push_back(apache::thrift::util::enumNameSafe(feature));
  }
  std::cout << "Feature List: " << folly::join(",", asicFeatures) << "\n";
  GTEST_SKIP();
}

void initAgentHwTest(int argc, char* argv[], PlatformInitFn initPlatform) {
  initEnsemble(initPlatform);
  kArgc = argc;
  kArgv = argv;
}

} // namespace facebook::fboss
