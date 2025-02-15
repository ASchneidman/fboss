// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#include "fboss/platform/platform_manager/PlatformExplorer.h"

#include <chrono>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

#include <folly/logging/xlog.h>

#include "fboss/platform/platform_manager/Utils.h"
#include "fboss/platform/platform_manager/gen-cpp2/platform_manager_config_constants.h"

namespace {
constexpr auto kRootSlotPath = "/";
constexpr auto kIdprom = "IDPROM";
const re2::RE2 kValidHwmonDirName{"hwmon[0-9]+"};

std::string getSlotPath(
    const std::string& parentSlotPath,
    const std::string& slotName) {
  if (parentSlotPath == kRootSlotPath) {
    return fmt::format("{}{}", kRootSlotPath, slotName);
  } else {
    return fmt::format("{}/{}", parentSlotPath, slotName);
  }
}
} // namespace

namespace facebook::fboss::platform::platform_manager {

using constants = platform_manager_config_constants;

PlatformExplorer::PlatformExplorer(
    std::chrono::seconds exploreInterval,
    const PlatformConfig& config,
    bool runOnce)
    : platformConfig_(config) {
  if (runOnce) {
    explore();
    return;
  }
  scheduler_.addFunction(
      [this, exploreInterval]() {
        try {
          explore();
        } catch (const std::exception& ex) {
          XLOG(ERR) << fmt::format(
              "Exception while exploring platform: {}. Will retry after {} seconds.",
              folly::exceptionStr(ex),
              exploreInterval.count());
        }
      },
      exploreInterval);
  scheduler_.start();
}

void PlatformExplorer::explore() {
  XLOG(INFO) << "Exploring the platform";
  for (const auto& [busName, busNum] :
       i2cExplorer_.getBusNums(*platformConfig_.i2cAdaptersFromCpu())) {
    dataStore_.updateI2cBusNum(std::nullopt, busName, busNum);
  }
  const PmUnitConfig& rootPmUnitConfig =
      platformConfig_.pmUnitConfigs()->at(*platformConfig_.rootPmUnitName());
  auto pmUnitName = getPmUnitNameFromSlot(
      *rootPmUnitConfig.pluggedInSlotType(), kRootSlotPath);
  CHECK(pmUnitName == *platformConfig_.rootPmUnitName());
  explorePmUnit(kRootSlotPath, *platformConfig_.rootPmUnitName());
  XLOG(INFO) << "Creating symbolic links ...";
  for (const auto& [linkPath, devicePath] :
       *platformConfig_.symbolicLinkToDevicePath()) {
    createDeviceSymLink(linkPath, devicePath);
  }
  XLOG(INFO) << "SUCCESS. Completed setting up all the devices.";
}

void PlatformExplorer::explorePmUnit(
    const std::string& slotPath,
    const std::string& pmUnitName) {
  auto pmUnitConfig = platformConfig_.pmUnitConfigs()->at(pmUnitName);
  XLOG(INFO) << fmt::format("Exploring PmUnit {} at {}", pmUnitName, slotPath);

  dataStore_.updatePmUnitName(slotPath, pmUnitName);

  XLOG(INFO) << fmt::format(
      "Exploring PCI Devices for PmUnit {} at SlotPath {}. Count {}",
      pmUnitName,
      slotPath,
      pmUnitConfig.pciDeviceConfigs()->size());
  explorePciDevices(slotPath, *pmUnitConfig.pciDeviceConfigs());

  XLOG(INFO) << fmt::format(
      "Exploring I2C Devices for PmUnit {} at SlotPath {}. Count {}",
      pmUnitName,
      slotPath,
      pmUnitConfig.i2cDeviceConfigs()->size());
  exploreI2cDevices(slotPath, *pmUnitConfig.i2cDeviceConfigs());

  XLOG(INFO) << fmt::format(
      "Exploring Slots for PmUnit {} at SlotPath {}. Count {}",
      pmUnitName,
      slotPath,
      pmUnitConfig.outgoingSlotConfigs_ref()->size());
  for (const auto& [slotName, slotConfig] :
       *pmUnitConfig.outgoingSlotConfigs()) {
    exploreSlot(slotPath, slotName, slotConfig);
  }
}

void PlatformExplorer::exploreSlot(
    const std::string& parentSlotPath,
    const std::string& slotName,
    const SlotConfig& slotConfig) {
  std::string childSlotPath = getSlotPath(parentSlotPath, slotName);
  XLOG(INFO) << fmt::format("Exploring SlotPath {}", childSlotPath);

  if (slotConfig.presenceDetection() &&
      !presenceDetector_.isPresent(*slotConfig.presenceDetection())) {
    XLOG(INFO) << fmt::format(
        "No device could be detected in SlotPath {}", childSlotPath);
  }

  int i = 0;
  for (const auto& busName : *slotConfig.outgoingI2cBusNames()) {
    auto busNum = dataStore_.getI2cBusNum(parentSlotPath, busName);
    dataStore_.updateI2cBusNum(
        childSlotPath, fmt::format("INCOMING@{}", i++), busNum);
  }
  auto childPmUnitName =
      getPmUnitNameFromSlot(*slotConfig.slotType(), childSlotPath);

  if (!childPmUnitName) {
    XLOG(INFO) << fmt::format(
        "No device could be read in Slot {}", childSlotPath);
    return;
  }

  explorePmUnit(childSlotPath, *childPmUnitName);
}

std::optional<std::string> PlatformExplorer::getPmUnitNameFromSlot(
    const std::string& slotType,
    const std::string& slotPath) {
  auto slotTypeConfig = platformConfig_.slotTypeConfigs_ref()->at(slotType);
  CHECK(slotTypeConfig.idpromConfig() || slotTypeConfig.pmUnitName());
  std::optional<std::string> pmUnitNameInEeprom{std::nullopt};
  if (slotTypeConfig.idpromConfig_ref()) {
    auto idpromConfig = *slotTypeConfig.idpromConfig_ref();
    auto eepromI2cBusNum =
        dataStore_.getI2cBusNum(slotPath, *idpromConfig.busName());
    i2cExplorer_.createI2cDevice(
        "IDPROM",
        *idpromConfig.kernelDeviceName(),
        eepromI2cBusNum,
        I2cAddr(*idpromConfig.address()));
    auto eepromPath = i2cExplorer_.getDeviceI2cPath(
        eepromI2cBusNum, I2cAddr(*idpromConfig.address()));
    try {
      // TODO: One eeprom parsing library is implemented, get the
      // Product Name from eeprom contents of eepromPath and use it here.
      pmUnitNameInEeprom = std::nullopt;
    } catch (const std::exception& e) {
      XLOG(ERR) << fmt::format(
          "Could not fetch contents of IDPROM {}. {}", eepromPath, e.what());
    }
  }
  if (slotTypeConfig.pmUnitName()) {
    if (pmUnitNameInEeprom &&
        *pmUnitNameInEeprom != *slotTypeConfig.pmUnitName()) {
      XLOG(WARNING) << fmt::format(
          "The PmUnit name in eeprom `{}` is different from the one in config `{}`",
          *pmUnitNameInEeprom,
          *slotTypeConfig.pmUnitName());
    }
    return *slotTypeConfig.pmUnitName();
  }
  return pmUnitNameInEeprom;
}

void PlatformExplorer::exploreI2cDevices(
    const std::string& slotPath,
    const std::vector<I2cDeviceConfig>& i2cDeviceConfigs) {
  for (const auto& i2cDeviceConfig : i2cDeviceConfigs) {
    i2cExplorer_.createI2cDevice(
        *i2cDeviceConfig.pmUnitScopedName(),
        *i2cDeviceConfig.kernelDeviceName(),
        dataStore_.getI2cBusNum(slotPath, *i2cDeviceConfig.busName()),
        I2cAddr(*i2cDeviceConfig.address()));
    if (i2cDeviceConfig.numOutgoingChannels()) {
      auto channelToBusNums = i2cExplorer_.getMuxChannelI2CBuses(
          dataStore_.getI2cBusNum(slotPath, *i2cDeviceConfig.busName()),
          I2cAddr(*i2cDeviceConfig.address()));
      assert(channelToBusNums.size() == i2cDeviceConfig.numOutgoingChannels());
      for (const auto& [channelNum, busNum] : channelToBusNums) {
        dataStore_.updateI2cBusNum(
            slotPath,
            fmt::format(
                "{}@{}", *i2cDeviceConfig.pmUnitScopedName(), channelNum),
            busNum);
      }
    }
  }
}

void PlatformExplorer::explorePciDevices(
    const std::string& slotPath,
    const std::vector<PciDeviceConfig>& pciDeviceConfigs) {
  for (const auto& pciDeviceConfig : pciDeviceConfigs) {
    auto pciDevice = PciDevice(
        *pciDeviceConfig.pmUnitScopedName(),
        *pciDeviceConfig.vendorId(),
        *pciDeviceConfig.deviceId(),
        *pciDeviceConfig.subSystemVendorId(),
        *pciDeviceConfig.subSystemDeviceId());
    auto charDevPath = pciDevice.charDevPath();
    auto instId =
        getFpgaInstanceId(slotPath, *pciDeviceConfig.pmUnitScopedName());
    for (const auto& i2cAdapterConfig : *pciDeviceConfig.i2cAdapterConfigs()) {
      auto busNums =
          pciExplorer_.createI2cAdapter(pciDevice, i2cAdapterConfig, instId++);
      if (*i2cAdapterConfig.numberOfAdapters() > 1) {
        CHECK_EQ(busNums.size(), *i2cAdapterConfig.numberOfAdapters());
        for (auto i = 0; i < busNums.size(); i++) {
          dataStore_.updateI2cBusNum(
              slotPath,
              fmt::format(
                  "{}@{}",
                  *i2cAdapterConfig.fpgaIpBlockConfig()->pmUnitScopedName(),
                  i),
              busNums[i]);
        }
      } else {
        CHECK_EQ(busNums.size(), 1);
        dataStore_.updateI2cBusNum(
            slotPath,
            *i2cAdapterConfig.fpgaIpBlockConfig()->pmUnitScopedName(),
            busNums[0]);
      }
    }
    for (const auto& spiMasterConfig : *pciDeviceConfig.spiMasterConfigs()) {
      pciExplorer_.createSpiMaster(charDevPath, spiMasterConfig, instId++);
    }
    for (const auto& fpgaIpBlockConfig : *pciDeviceConfig.gpioChipConfigs()) {
      auto gpioNum =
          pciExplorer_.createGpioChip(pciDevice, fpgaIpBlockConfig, instId++);
      dataStore_.updateGpioChipNum(
          slotPath, *fpgaIpBlockConfig.pmUnitScopedName(), gpioNum);
    }
    for (const auto& fpgaIpBlockConfig : *pciDeviceConfig.watchdogConfigs()) {
      pciExplorer_.createFpgaIpBlock(charDevPath, fpgaIpBlockConfig, instId++);
    }
    for (const auto& fpgaIpBlockConfig :
         *pciDeviceConfig.fanTachoPwmConfigs()) {
      pciExplorer_.createFpgaIpBlock(charDevPath, fpgaIpBlockConfig, instId++);
    }
    for (const auto& fpgaIpBlockConfig : *pciDeviceConfig.ledCtrlConfigs()) {
      pciExplorer_.createLedCtrl(charDevPath, fpgaIpBlockConfig, instId++);
    }
    for (const auto& xcvrCtrlConfig : *pciDeviceConfig.xcvrCtrlConfigs()) {
      pciExplorer_.createXcvrCtrl(charDevPath, xcvrCtrlConfig, instId++);
    }
    for (const auto& fpgaIpBlockConfig : *pciDeviceConfig.miscCtrlConfigs()) {
      pciExplorer_.createFpgaIpBlock(charDevPath, fpgaIpBlockConfig, instId++);
    }
  }
}

uint32_t PlatformExplorer::getFpgaInstanceId(
    const std::string& slotPath,
    const std::string& fpgaName) {
  auto key = std::make_pair(slotPath, fpgaName);
  auto it = fpgaInstanceIds_.find(key);
  if (it == fpgaInstanceIds_.end()) {
    fpgaInstanceIds_[key] = 1000 * (fpgaInstanceIds_.size() + 1);
  }
  return fpgaInstanceIds_[key];
}

void PlatformExplorer::createDeviceSymLink(
    const std::string& linkPath,
    const std::string& devicePath) {
  auto linkParentPath = std::filesystem::path(linkPath).parent_path();
  if (!Utils().createDirectories(linkParentPath.string())) {
    XLOG(ERR) << fmt::format(
        "Failed to create the parent path ({})", linkParentPath.string());
    return;
  }

  const auto [slotPath, deviceName] = Utils().parseDevicePath(devicePath);
  if (!dataStore_.hasPmUnit(slotPath)) {
    XLOG(ERR) << fmt::format("No PmUnit exists at {}", slotPath);
    return;
  }
  auto pmUnitName = dataStore_.getPmUnitName(slotPath);
  auto pmUnitConfig = platformConfig_.pmUnitConfigs()->at(pmUnitName);

  auto idpromConfig = platformConfig_.slotTypeConfigs()
                          ->at(*pmUnitConfig.pluggedInSlotType())
                          .idpromConfig();
  auto i2cDeviceConfig = std::find_if(
      pmUnitConfig.i2cDeviceConfigs()->begin(),
      pmUnitConfig.i2cDeviceConfigs()->end(),
      [deviceNameCopy = deviceName](auto i2cDeviceConfig) {
        return *i2cDeviceConfig.pmUnitScopedName() == deviceNameCopy;
      });
  auto pciDeviceConfig = std::find_if(
      pmUnitConfig.pciDeviceConfigs()->begin(),
      pmUnitConfig.pciDeviceConfigs()->end(),
      [deviceNameCopy = deviceName](auto pciDeviceConfig) {
        return *pciDeviceConfig.pmUnitScopedName() == deviceNameCopy;
      });

  std::optional<std::filesystem::path> targetPath = std::nullopt;
  if (linkParentPath.string() == "/run/devmap/eeproms") {
    if (deviceName == kIdprom) {
      CHECK(idpromConfig);
      targetPath = std::filesystem::path(i2cExplorer_.getDeviceI2cPath(
          dataStore_.getI2cBusNum(slotPath, *idpromConfig->busName()),
          I2cAddr(*idpromConfig->address())));
    } else {
      if (i2cDeviceConfig == pmUnitConfig.i2cDeviceConfigs()->end()) {
        XLOG(ERR) << fmt::format(
            "Couldn't find i2c device config for ({})", deviceName);
      }
      auto busNum =
          dataStore_.getI2cBusNum(slotPath, *i2cDeviceConfig->busName());
      auto i2cAddr = I2cAddr(*i2cDeviceConfig->address());
      if (!i2cExplorer_.isI2cDevicePresent(busNum, i2cAddr)) {
        XLOG(ERR) << fmt::format(
            "{} is not plugged-in to the platform", deviceName);
        return;
      }
      targetPath =
          std::filesystem::path(i2cExplorer_.getDeviceI2cPath(busNum, i2cAddr));
    }
    targetPath = *targetPath / "eeprom";
  } else if (linkParentPath.string() == "/run/devmap/sensors") {
    if (i2cDeviceConfig == pmUnitConfig.i2cDeviceConfigs()->end()) {
      XLOG(ERR) << fmt::format(
          "Couldn't find i2c device config for ({})", deviceName);
    }
    auto busNum =
        dataStore_.getI2cBusNum(slotPath, *i2cDeviceConfig->busName());
    auto i2cAddr = I2cAddr(*i2cDeviceConfig->address());
    if (!i2cExplorer_.isI2cDevicePresent(busNum, i2cAddr)) {
      XLOG(ERR) << fmt::format(
          "{} is not plugged-in to the platform", deviceName);
      return;
    }
    targetPath =
        std::filesystem::path(i2cExplorer_.getDeviceI2cPath(busNum, i2cAddr)) /
        "hwmon";
    std::string hwmonSubDir = "";
    for (const auto& dirEntry :
         std::filesystem::directory_iterator(*targetPath)) {
      auto dirName = dirEntry.path().filename();
      if (re2::RE2::FullMatch(dirName.string(), kValidHwmonDirName)) {
        hwmonSubDir = dirName.string();
        break;
      }
    }
    if (hwmonSubDir.empty()) {
      XLOG(ERR) << fmt::format(
          "Couldn't find hwmon[num] folder within ({})", targetPath->string());
      return;
    }
    targetPath = *targetPath / hwmonSubDir;
  } else if (linkParentPath.string() == "/run/devmap/cplds") {
    if (i2cDeviceConfig != pmUnitConfig.i2cDeviceConfigs()->end()) {
      auto busNum =
          dataStore_.getI2cBusNum(slotPath, *i2cDeviceConfig->busName());
      auto i2cAddr = I2cAddr(*i2cDeviceConfig->address());
      if (!i2cExplorer_.isI2cDevicePresent(busNum, i2cAddr)) {
        XLOG(ERR) << fmt::format(
            "{} is not plugged-in to the platform", deviceName);
        return;
      }
      targetPath =
          std::filesystem::path(i2cExplorer_.getDeviceI2cPath(busNum, i2cAddr));
    } else if (pciDeviceConfig != pmUnitConfig.pciDeviceConfigs()->end()) {
      auto pciDevice = PciDevice(
          *pciDeviceConfig->pmUnitScopedName(),
          *pciDeviceConfig->vendorId(),
          *pciDeviceConfig->deviceId(),
          *pciDeviceConfig->subSystemVendorId(),
          *pciDeviceConfig->subSystemDeviceId());
      targetPath = std::filesystem::path(pciDevice.sysfsPath());
    } else {
      XLOG(ERR) << fmt::format(
          "Couldn't resolve target path for ({})", deviceName);
      return;
    }
  } else if (linkParentPath.string() == "/run/devmap/fpgas") {
    if (pciDeviceConfig == pmUnitConfig.pciDeviceConfigs()->end()) {
      XLOG(ERR) << fmt::format(
          "Couldn't find PCI device config for ({})", deviceName);
      return;
    }
    auto pciDevice = PciDevice(
        *pciDeviceConfig->pmUnitScopedName(),
        *pciDeviceConfig->vendorId(),
        *pciDeviceConfig->deviceId(),
        *pciDeviceConfig->subSystemVendorId(),
        *pciDeviceConfig->subSystemDeviceId());
    targetPath = std::filesystem::path(pciDevice.sysfsPath());
  } else if (linkParentPath.string() == "/run/devmap/i2c-busses") {
    targetPath = std::filesystem::path(fmt::format(
        "/dev/i2c-{}", dataStore_.getI2cBusNum(slotPath, deviceName)));
  } else if (linkParentPath.string() == "/run/devmap/gpiochips") {
    targetPath = std::filesystem::path(fmt::format(
        "/dev/gpiochip{}", dataStore_.getGpioChipNum(slotPath, deviceName)));
  } else {
    XLOG(ERR) << fmt::format("Symbolic link {} is not supported.", linkPath);
    return;
  }

  XLOG(INFO) << fmt::format(
      "Creating symlink from {} to {}. DevicePath: {}",
      linkPath,
      targetPath->string(),
      devicePath);
  auto cmd = fmt::format("ln -sfnv {} {}", targetPath->string(), linkPath);
  auto [exitStatus, standardOut] = PlatformUtils().execCommand(cmd);
  if (exitStatus != 0) {
    XLOG(ERR) << fmt::format("Failed to run command ({})", cmd);
    return;
  }
}

} // namespace facebook::fboss::platform::platform_manager
