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

#pragma once
#include <app/util/af-types.h>

class BaseAppFabricTableDelegate : public chip::FabricTable::Delegate
{
private:
    /** OnFabricCommitted
     * @brief Checks if the Committed fabric is the first one, notifies the app if it is.
     *
     */
    void OnFabricCommitted(const chip::FabricTable & fabricTable, chip::FabricIndex fabricIndex);

    /** OnFabricRemoved
     * @brief Checks if the Removed fabric is the last one, open the Commissioning Window if it is.
     */
    void OnFabricRemoved(const chip::FabricTable & fabricTable, chip::FabricIndex fabricIndex);
};
