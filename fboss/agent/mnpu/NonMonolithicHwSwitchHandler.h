#pragma once

#include "fboss/agent/HwSwitchHandler.h"
#include "fboss/agent/if/gen-cpp2/MultiSwitchCtrl.h"

namespace facebook::fboss {

class SwSwitch;

class NonMonolithicHwSwitchHandler : public HwSwitchHandler {
 public:
  NonMonolithicHwSwitchHandler(
      const SwitchID& switchId,
      const cfg::SwitchInfo& info,
      SwSwitch* sw);

  virtual ~NonMonolithicHwSwitchHandler() override;

  void exitFatal() const override;

  std::unique_ptr<TxPacket> allocatePacket(uint32_t size) const override;

  bool sendPacketOutOfPortAsync(
      std::unique_ptr<TxPacket> pkt,
      PortID portID,
      std::optional<uint8_t> queue = std::nullopt) noexcept override;

  bool sendPacketSwitchedSync(std::unique_ptr<TxPacket> pkt) noexcept override;

  bool sendPacketSwitchedAsync(std::unique_ptr<TxPacket> pkt) noexcept override;

  bool isValidStateUpdate(const StateDelta& delta) const override;

  void unregisterCallbacks() override;

  void gracefulExit() override;

  bool getAndClearNeighborHit(RouterID vrf, folly::IPAddress& ip) override;

  folly::dynamic toFollyDynamic() const override;

  std::optional<uint32_t> getHwLogicalPortId(PortID portID) const override;

  bool transactionsSupported(
      std::optional<cfg::SdkVersion> sdkVersion) const override;

  folly::F14FastMap<std::string, HwPortStats> getPortStats() const override;

  std::map<std::string, HwSysPortStats> getSysPortStats() const override;
  HwSwitchDropStats getSwitchDropStats() const override;

  void updateStats() override;

  std::map<PortID, phy::PhyInfo> updateAllPhyInfo() override;

  uint64_t getDeviceWatermarkBytes() const override;

  HwSwitchFb303Stats* getSwitchStats() const override;

  void clearPortStats(
      const std::unique_ptr<std::vector<int32_t>>& ports) override;

  std::vector<phy::PrbsLaneStats> getPortAsicPrbsStats(int32_t portId) override;

  void clearPortAsicPrbsStats(int32_t portId) override;

  std::vector<prbs::PrbsPolynomial> getPortPrbsPolynomials(
      int32_t portId) override;

  prbs::InterfacePrbsState getPortPrbsState(PortID portId) override;

  void switchRunStateChanged(SwitchRunState newState) override;

  // platform access apis
  void onHwInitialized(HwSwitchCallback* callback) override;

  void onInitialConfigApplied(HwSwitchCallback* sw) override;

  void platformStop() override;

  std::shared_ptr<SwitchState> stateChanged(
      const StateDelta& delta,
      bool transaction) override;

  std::pair<fsdb::OperDelta, HwSwitchStateUpdateStatus> stateChanged(
      const fsdb::OperDelta& delta,
      bool transaction) override;

  CpuPortStats getCpuPortStats() const override;

  std::map<PortID, FabricEndpoint> getFabricConnectivity() const override;

  FabricReachabilityStats getFabricReachabilityStats() const override;

  std::vector<PortID> getSwitchReachability(SwitchID switchId) const override;

  std::string getDebugDump() const override;

  void fetchL2Table(std::vector<L2EntryThrift>* l2Table) const override;

  std::string listObjects(const std::vector<HwObjectType>& types, bool cached)
      const override;

  bool needL2EntryForNeighbor(const cfg::SwitchConfig* config) const override;

  multiswitch::StateOperDelta getNextStateOperDelta(
      std::unique_ptr<multiswitch::StateOperDelta> prevOperResult,
      bool initialSync) override;

  void notifyHwSwitchDisconnected() override;
  void cancelOperDeltaSync();
  HwSwitchOperDeltaSyncState getHwSwitchOperDeltaSyncState() override;

  bool sendPacketOutViaThriftStream(
      std::unique_ptr<TxPacket> pkt,
      std::optional<PortID> portID = std::nullopt,
      std::optional<uint8_t> queue = std::nullopt);

  SwitchRunState getHwSwitchRunState() override;

 private:
  bool isOperSyncState(HwSwitchOperDeltaSyncState state) const;
  void setOperSyncState(HwSwitchOperDeltaSyncState state) {
    operDeltaSyncState_ = state;
  }
  HwSwitchOperDeltaSyncState getOperSyncState() {
    return operDeltaSyncState_;
  }

  SwSwitch* sw_;
  std::condition_variable stateUpdateCV_;
  std::mutex stateUpdateMutex_;
  multiswitch::StateOperDelta* nextOperDelta_{nullptr};
  multiswitch::StateOperDelta* prevOperDeltaResult_{nullptr};
  HwSwitchOperDeltaSyncState operDeltaSyncState_{DISCONNECTED};
  bool deltaReady_{false};
  bool ackReceived_{false};
};

} // namespace facebook::fboss
