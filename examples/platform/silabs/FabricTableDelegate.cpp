/*
 *
 *    Copyright (c) 2024 Project CHIP Authors
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
#include "FabricTableDelegate.h"
#include "BaseApplication.h"
#include "silabs_utils.h"
#include <app/server/Server.h>

using FabricTable = chip::FabricTable;
using FabricIndex = chip::FabricIndex;

void BaseAppFabricTableDelegate::OnFabricCommitted(const FabricTable & fabricTable, FabricIndex fabricIndex)
{
    // If we just comissioned the first fabric, send signal to base app to update the status LED
    if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 1)
    {
        SILABS_LOG("The second fabric has been added to the Fabric Table");
        BaseApplication::UpdateCommissioningStatus(true);
    }
}

void BaseAppFabricTableDelegate::OnFabricRemoved(const chip::FabricTable & fabricTable, chip::FabricIndex fabricIndex)
{
    if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0)
    {
        CHIP_ERROR err = chip::Server::GetInstance().GetCommissioningWindowManager().OpenBasicCommissioningWindow();

        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(AppServer, "Failed to open commissioning window: %s", chip::ErrorStr(err));
        }

        // Send signal to base app to update the status LED
        BaseApplication::UpdateCommissioningStatus(false);
    }
}
