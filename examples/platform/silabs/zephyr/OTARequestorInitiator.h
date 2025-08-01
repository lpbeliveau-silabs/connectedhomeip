/*
 *
 *    Copyright (c) 2025 Project CHIP Authors
 *    Copyright 2023-2025 NXP
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

#pragma once

#include "app/clusters/ota-requestor/BDXDownloader.h"
#include "app/clusters/ota-requestor/DefaultOTARequestor.h"
#include "app/clusters/ota-requestor/DefaultOTARequestorDriver.h"
#include "app/clusters/ota-requestor/DefaultOTARequestorStorage.h"

#ifndef CONFIG_APP_FREERTOS_OS
#include <platform/nxp/zephyr/ota/OTAImageProcessorImpl.h>
#else
#include "platform/nxp/common/ota/OTAImageProcessorImpl.h"
#endif /* CONFIG_APP_FREERTOS_OS */

#include <stdint.h>

using namespace chip;
using namespace chip::DeviceLayer;

namespace chip {
namespace NXP {
namespace App {
class OTARequestorInitiator
{
public:
    static OTARequestorInitiator & Instance(void)
    {
        static OTARequestorInitiator gOTARequestorInitiator;
        return gOTARequestorInitiator;
    }
    /* Initialize OTA components */
    static void InitOTA(intptr_t context);
    /* Handle update under test */
    static void HandleSelfTest();

    /* OTA components */
    DefaultOTARequestor gRequestorCore;
    DefaultOTARequestorStorage gRequestorStorage;
    DeviceLayer::DefaultOTARequestorDriver gRequestorUser;
    BDXDownloader gDownloader;
};
} // namespace App
} // namespace NXP
} // namespace chip
