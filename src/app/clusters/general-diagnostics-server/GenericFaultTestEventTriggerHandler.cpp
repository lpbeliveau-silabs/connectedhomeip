/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "GenericFaultTestEventTriggerHandler.h"

#include <app/clusters/general-diagnostics-server/CodegenIntegration.h>
#include <lib/support/CodeUtils.h>

using namespace ::chip::DeviceLayer;

namespace chip {

CHIP_ERROR GenericFaultTestEventTriggerHandler::HandleEventTrigger(uint64_t eventTrigger)
{
    eventTrigger = clearEndpointInEventTrigger(eventTrigger);
    if (eventTrigger == kGenericFaultQueryTrigger)
    {
        // Fault injection
        GeneralFaults<kMaxHardwareFaults> hwFaultsPrevious;
        GeneralFaults<kMaxHardwareFaults> hwFaultsCurrent;
        using app::Clusters::GeneralDiagnostics::HardwareFaultEnum;
        ReturnErrorOnFailure(hwFaultsPrevious.add(to_underlying(HardwareFaultEnum::kRadio)));
        ReturnErrorOnFailure(hwFaultsPrevious.add(to_underlying(HardwareFaultEnum::kPowerSource)));

        ReturnErrorOnFailure(hwFaultsCurrent.add(to_underlying(HardwareFaultEnum::kRadio)));
        ReturnErrorOnFailure(hwFaultsCurrent.add(to_underlying(HardwareFaultEnum::kSensor)));
        ReturnErrorOnFailure(hwFaultsCurrent.add(to_underlying(HardwareFaultEnum::kPowerSource)));
        ReturnErrorOnFailure(hwFaultsCurrent.add(to_underlying(HardwareFaultEnum::kUserInterfaceFault)));

        app::Clusters::GeneralDiagnostics::GlobalNotifyHardwareFaultsDetect(hwFaultsPrevious, hwFaultsCurrent);

        // Radio faults injection
        GeneralFaults<kMaxRadioFaults> radioFaultsPrevious;
        GeneralFaults<kMaxRadioFaults> radioFaultsCurrent;

        using app::Clusters::GeneralDiagnostics::RadioFaultEnum;
        ReturnErrorOnFailure(radioFaultsPrevious.add(to_underlying(RadioFaultEnum::kWiFiFault)));
        ReturnErrorOnFailure(radioFaultsPrevious.add(to_underlying(RadioFaultEnum::kThreadFault)));

        ReturnErrorOnFailure(radioFaultsCurrent.add(to_underlying(RadioFaultEnum::kWiFiFault)));
        ReturnErrorOnFailure(radioFaultsCurrent.add(to_underlying(RadioFaultEnum::kCellularFault)));
        ReturnErrorOnFailure(radioFaultsCurrent.add(to_underlying(RadioFaultEnum::kThreadFault)));
        ReturnErrorOnFailure(radioFaultsCurrent.add(to_underlying(RadioFaultEnum::kNFCFault)));

        app::Clusters::GeneralDiagnostics::GlobalNotifyRadioFaultsDetect(radioFaultsPrevious, radioFaultsCurrent);

        GeneralFaults<kMaxNetworkFaults> networkFaultsPrevious;
        GeneralFaults<kMaxNetworkFaults> networkFaultsCurrent;

        // Network faults injections
        using app::Clusters::GeneralDiagnostics::NetworkFaultEnum;
        ReturnErrorOnFailure(networkFaultsPrevious.add(to_underlying(NetworkFaultEnum::kHardwareFailure)));
        ReturnErrorOnFailure(networkFaultsPrevious.add(to_underlying(NetworkFaultEnum::kNetworkJammed)));

        ReturnErrorOnFailure(networkFaultsCurrent.add(to_underlying(NetworkFaultEnum::kHardwareFailure)));
        ReturnErrorOnFailure(networkFaultsCurrent.add(to_underlying(NetworkFaultEnum::kNetworkJammed)));
        ReturnErrorOnFailure(networkFaultsCurrent.add(to_underlying(NetworkFaultEnum::kConnectionFailed)));

        app::Clusters::GeneralDiagnostics::GlobalNotifyNetworkFaultsDetect(networkFaultsPrevious, networkFaultsCurrent);
    }

    return CHIP_ERROR_INVALID_ARGUMENT;
}

} // namespace chip
