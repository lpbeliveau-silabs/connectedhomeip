/*
 *
 *    Copyright (c) 2023-2024 Project CHIP Authors
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

#include "DeviceEnergyManagementDelegateImpl.h"
#include "DEMManufacturerDelegate.h"
#include <app/EventLogging.h>
#include <app/reporting/reporting.h>
#include <platform/CHIPDeviceLayer.h>
#include <protocols/interaction_model/StatusCode.h>
#include <system/SystemClock.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::DeviceEnergyManagement;
using namespace chip::app::Clusters::DeviceEnergyManagement::Attributes;

using chip::Protocols::InteractionModel::Status;

using chip::Optional;
using CostsList = DataModel::List<const Structs::CostStruct::Type>;

DeviceEnergyManagementDelegate::DeviceEnergyManagementDelegate() :
    mpDEMManufacturerDelegate(nullptr), mEsaType(ESATypeEnum::kEvse), mEsaCanGenerate(false), mEsaState(ESAStateEnum::kOffline),
    mAbsMinPowerMw(0), mAbsMaxPowerMw(0), mOptOutState(OptOutStateEnum::kNoOptOut), mPowerAdjustmentInProgress(false),
    mPowerAdjustmentStartTimeUtc(0), mPauseRequestInProgress(false)
{}

void DeviceEnergyManagementDelegate::SetDeviceEnergyManagementInstance(DeviceEnergyManagement::Instance & instance)
{
    mpDEMInstance = &instance;
}

uint32_t DeviceEnergyManagementDelegate::HasFeature(Feature feature) const
{
    bool hasFeature = false;

    if (mpDEMInstance != nullptr)
    {
        hasFeature = mpDEMInstance->HasFeature(feature);
    }

    return hasFeature;
}

void DeviceEnergyManagementDelegate::SetDEMManufacturerDelegate(
    DEMManufacturerDelegate & deviceEnergyManagementManufacturerDelegate)
{
    mpDEMManufacturerDelegate = &deviceEnergyManagementManufacturerDelegate;
}

chip::app::Clusters::DeviceEnergyManagement::DEMManufacturerDelegate * DeviceEnergyManagementDelegate::GetDEMManufacturerDelegate()
{
    return mpDEMManufacturerDelegate;
}

/**
 * @brief Delegate handler for PowerAdjustRequest
 *
 * Note: checking of the validity of the PowerAdjustRequest has been done by the lower layer
 *
 * This function needs to notify the appliance that it should apply a new power setting.
 * It should:
 *   1) notify the appliance - if the appliance hardware cannot be adjusted, then return Failure
 *   2) start a timer (or restart the existing PowerAdjust timer) for duration seconds
 *   3) if appropriate, update the forecast with the new expected end time
 *
 *  and when the timer expires:
 *   4) notify the appliance's that it can resume its intended power setting (or go idle)
 *   5) if necessary, update the forecast with new expected end time
 */
Status DeviceEnergyManagementDelegate::PowerAdjustRequest(const int64_t powerMw, const uint32_t durationS,
                                                          AdjustmentCauseEnum cause)
{
    //  Update the forecast with the new expected end time
    if (mpDEMManufacturerDelegate != nullptr)
    {
        CHIP_ERROR err = mpDEMManufacturerDelegate->HandleDeviceEnergyManagementPowerAdjustRequest(powerMw, durationS, cause);
        if (err != CHIP_NO_ERROR)
        {
            return Status::Failure;
        }
    }

    TEMPORARY_RETURN_IGNORED SetESAState(ESAStateEnum::kPowerAdjustActive);

    // mPowerAdjustCapabilityStruct is guaranteed to have a value as validated in Instance::HandlePowerAdjustRequest.
    // If it did not have a value, this method would not have been called.
    switch (cause)
    {
    case AdjustmentCauseEnum::kLocalOptimization:
        TEMPORARY_RETURN_IGNORED SetPowerAdjustmentCapabilityPowerAdjustReason(PowerAdjustReasonEnum::kLocalOptimizationAdjustment);
        break;

    case AdjustmentCauseEnum::kGridOptimization:
        TEMPORARY_RETURN_IGNORED SetPowerAdjustmentCapabilityPowerAdjustReason(PowerAdjustReasonEnum::kGridOptimizationAdjustment);
        break;

    default:
        HandlePowerAdjustRequestFailure();
        return Status::Failure;
    }

    mPowerAdjustmentInProgress = true;

    return Status::Success;
}

/**
 * @brief Handle a PowerAdjustRequest failing
 *
 *  Cleans up the PowerAdjust state should the request fail
 */
void DeviceEnergyManagementDelegate::HandlePowerAdjustRequestFailure()
{
    TEMPORARY_RETURN_IGNORED SetESAState(ESAStateEnum::kOnline);

    mPowerAdjustmentInProgress = false;

    TEMPORARY_RETURN_IGNORED SetPowerAdjustmentCapabilityPowerAdjustReason(PowerAdjustReasonEnum::kNoAdjustment);

    // TODO
    // Should we inform the mpDEMManufacturerDelegate that PowerAdjustRequest has failed?
}

/**
 * @brief Timer for handling the completion of a PowerAdjustRequest
 *
 *  When the timer expires:
 *   1) notify the appliance's that it can resume its intended power setting (or go idle)
 *   3) if necessary, update the forecast with new expected end time
 */
void DeviceEnergyManagementDelegate::HandlePowerAdjustCompletion()
{
    ChipLogError(AppServer, "DeviceEnergyManagementDelegate::HandlePowerAdjustTimerExpiry");

    // The PowerAdjustment is no longer in progress
    mPowerAdjustmentInProgress = false;

    TEMPORARY_RETURN_IGNORED SetESAState(ESAStateEnum::kOnline);

    TEMPORARY_RETURN_IGNORED SetPowerAdjustmentCapabilityPowerAdjustReason(PowerAdjustReasonEnum::kNoAdjustment);

    // Update the forecast with new expected end time
    if (mpDEMManufacturerDelegate != nullptr)
    {
        TEMPORARY_RETURN_IGNORED mpDEMManufacturerDelegate->HandleDeviceEnergyManagementPowerAdjustCompletion();
    }
}

/**
 * @brief Delegate handler for CancelPowerAdjustRequest
 *
 * Note: checking of the validity of the CancelPowerAdjustRequest has been done by the lower layer
 *
 * This function needs to notify the appliance that it should resume its intended power setting (or go idle).
 *
 * It should:
 *   1) notify the appliance's that it can resume its intended power setting (or go idle)
 *   2) if necessary, update the forecast with new expected end time
 */
Status DeviceEnergyManagementDelegate::CancelPowerAdjustRequest()
{
    Status status = Status::Success;

    TEMPORARY_RETURN_IGNORED SetESAState(ESAStateEnum::kOnline);

    mPowerAdjustmentInProgress = false;
    TEMPORARY_RETURN_IGNORED SetPowerAdjustmentCapabilityPowerAdjustReason(PowerAdjustReasonEnum::kNoAdjustment);

    // Notify the appliance's that it can resume its intended power setting (or go idle)
    if (mpDEMManufacturerDelegate != nullptr)
    {
        // It is expected the mpDEMManufacturerDelegate will update the forecast with new expected end time
        // as a consequence of the cancel request.
        VeriftOrReturnError(CHIP_NO_ERROR ==
                                mpDEMManufacturerDelegate->HandleDeviceEnergyManagementCancelPowerAdjustRequest(
                                    PowerAdjustReasonEnum::kNoAdjustment),
                            Status::Failure);
    }

    return status;
}

int64_t DeviceEnergyManagementDelegate::GetEnergyUseSinceLastPowerAdjustStart()
{
    return mpDEMManufacturerDelegate->GetApproxEnergyDuringSession();
}

/**
 * @brief Delegate handler for StartTimeAdjustRequest
 *
 * Note: checking of the validity of the StartTimeAdjustRequest has been done by the lower layer
 *
 * This function needs to notify the appliance that the forecast has been updated by a client.
 *
 * It should:
 *      1) update the forecast attribute with the revised start time
 *      2) send a callback notification to the appliance so it can refresh its internal schedule
 */
Status DeviceEnergyManagementDelegate::StartTimeAdjustRequest(const uint32_t requestedStartTimeUtc, AdjustmentCauseEnum cause)
{
    if (mForecast.IsNull())
    {
        return Status::Failure;
    }

    switch (cause)
    {
    case AdjustmentCauseEnum::kLocalOptimization:
        mForecast.Value().forecastUpdateReason = ForecastUpdateReasonEnum::kLocalOptimization;
        break;
    case AdjustmentCauseEnum::kGridOptimization:
        mForecast.Value().forecastUpdateReason = ForecastUpdateReasonEnum::kGridOptimization;
        break;
    default:
        ChipLogDetail(AppServer, "Bad cause %d", to_underlying(cause));
        return Status::Failure;
        break;
    }

    mForecast.Value().forecastID++;

    uint32_t durationS = mForecast.Value().endTime - mForecast.Value().startTime; // the current entire forecast duration

    // Save the start and end time in case there is an issue with the mpDEMManufacturerDelegate handling this
    // startTimeAdjustment request
    uint32_t savedStartTime = mForecast.Value().startTime;
    uint32_t savedEndTime   = mForecast.Value().endTime;

    /* Modify start time and end time */
    mForecast.Value().startTime = requestedStartTimeUtc;
    mForecast.Value().endTime   = requestedStartTimeUtc + durationS;

    if (mpDEMManufacturerDelegate != nullptr)
    {
        CHIP_ERROR err =
            mpDEMManufacturerDelegate->HandleDeviceEnergyManagementStartTimeAdjustRequest(requestedStartTimeUtc, cause);
        if (err != CHIP_NO_ERROR)
        {
            // Reset state
            mForecast.Value().forecastUpdateReason = ForecastUpdateReasonEnum::kInternalOptimization;
            mForecast.Value().startTime            = savedStartTime;
            mForecast.Value().endTime              = savedEndTime;

            MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, Forecast::Id);

            return Status::Failure;
        }
    }

    MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, Forecast::Id);

    return Status::Success;
}

/**
 * @brief Delegate handler for Pause Request
 *
 * Note: checking of the validity of the Pause Request has been done by the lower layer
 *
 * This function needs to notify the appliance that it should now pause.
 * It should:
 *   1) pause the appliance - if the appliance hardware cannot be paused, then return Failure
 *   2) start a timer for duration seconds
 *   3) update the forecast with the new expected end time
 *
 *  and when the timer expires:
 *   4) restore the appliance's operational state
 *   5) if necessary, update the forecast with new expected end time
 */
Status DeviceEnergyManagementDelegate::PauseRequest(const uint32_t durationS, AdjustmentCauseEnum cause)
{
    if (!mPauseRequestInProgress)
    {
        mPauseRequestInProgress = true;
    }

    // Pause the appliance
    if (mpDEMManufacturerDelegate != nullptr)
    {
        // It is expected that the mpDEMManufacturerDelegate will update the forecast with the new expected end time
        err = mpDEMManufacturerDelegate->HandleDeviceEnergyManagementPauseRequest(durationS, cause);
        if (err != CHIP_NO_ERROR)
        {
            HandlePauseRequestFailure();
            return Status::Failure;
        }
    }

    TEMPORARY_RETURN_IGNORED SetESAState(ESAStateEnum::kPaused);

    // Update the forecaseUpdateReason based on the AdjustmentCause
    if (cause == AdjustmentCauseEnum::kLocalOptimization)
    {
        mForecast.Value().forecastUpdateReason = ForecastUpdateReasonEnum::kLocalOptimization;

        MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, Forecast::Id);
    }
    else if (cause == AdjustmentCauseEnum::kGridOptimization)
    {
        mForecast.Value().forecastUpdateReason = ForecastUpdateReasonEnum::kGridOptimization;

        MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, Forecast::Id);
    }

    return Status::Success;
}

/**
 * @brief Handle a PauseRequest failing
 *
 *  Cleans up the state should the PauseRequest fail
 */
void DeviceEnergyManagementDelegate::HandlePauseRequestFailure()
{
    TEMPORARY_RETURN_IGNORED SetESAState(ESAStateEnum::kOnline);

    mPauseRequestInProgress = false;

    // TODO
    // Should we inform the mpDEMManufacturerDelegate that PauseRequest has failed?
}

/**
 * @brief Timer for handling the completion of a PauseRequest
 *
 *  When the timer expires:
 *   1) restore the appliance's operational state
 *   3) if necessary, update the forecast with new expected end time
 */
void DeviceEnergyManagementDelegate::HandlePauseCompletion()
{
    // The PauseRequestment is no longer in progress
    mPauseRequestInProgress = false;

    TEMPORARY_RETURN_IGNORED SetESAState(ESAStateEnum::kOnline);

    // It is expected the mpDEMManufacturerDelegate will update the forecast with new expected end time
    if (mpDEMManufacturerDelegate != nullptr)
    {
        TEMPORARY_RETURN_IGNORED mpDEMManufacturerDelegate->HandleDeviceEnergyManagementPauseCompletion();
    }
}

/**
 * @brief Handles the cancelation of a pause operation
 *
 * This function needs to notify the appliance that it should resume its intended power setting (or go idle).
 *
 * It should:
 *   1) notify the appliance's that it can resume its intended power setting (or go idle)
 *   2) if necessary, update the forecast with new expected end time
 */
CHIP_ERROR DeviceEnergyManagementDelegate::CancelPauseRequest(CauseEnum cause)
{
    mPauseRequestInProgress = false;

    TEMPORARY_RETURN_IGNORED SetESAState(ESAStateEnum::kOnline);

    CHIP_ERROR err = CHIP_NO_ERROR;

    // Notify the appliance's that it can resume its intended power setting (or go idle)
    if (mpDEMManufacturerDelegate != nullptr)
    {
        // It is expected that the mpDEMManufacturerDelegate will update the forecast with new expected end time
        err = mpDEMManufacturerDelegate->HandleDeviceEnergyManagementCancelPauseRequest(cause);
    }

    return err;
}

/**
 * @brief Delegate handler for ResumeRequest
 *
 * Note: checking of the validity of the ResumeRequest has been done by the lower layer
 *
 * This function needs to notify the appliance that it should now resume operation
 *
 * It should:
 *   1) restore the appliance's operational state
 *   2) update the forecast with new expected end time (given that the pause duration was shorter than originally requested)
 *
 */
Status DeviceEnergyManagementDelegate::ResumeRequest()
{
    Status status = Status::Failure;

    if (mPauseRequestInProgress)
    {
        // Guard against mForecast being null
        if (!mForecast.IsNull())
        {
            // The PauseRequest has effectively been cancelled so as a result the device should
            // go back to InternalOptimisation
            mForecast.Value().forecastUpdateReason = ForecastUpdateReasonEnum::kInternalOptimization;

            MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, Forecast::Id);
        }

        CHIP_ERROR err = CancelPauseRequest(CauseEnum::kCancelled);
        if (err == CHIP_NO_ERROR)
        {
            status = Status::Success;
        }
    }

    return status;
}

/**
 * @brief Delegate handler for ModifyForecastRequest
 *
 * Note: Only basic checking of the validity of the ModifyForecastRequest has been
 * done by the lower layer. This is a more complex use-case and requires higher-level
 * work by the delegate.
 *
 * It should:
 *      1) determine if the new forecast adjustments are acceptable to the appliance
 *       - if not return Failure. For example, if it may cause the home to be too hot
 *         or too cold, or a battery to be insufficiently charged
 *      2) if the slot adjustments are acceptable, then update the forecast
 *      3) notify the appliance to follow the revised schedule
 */
Status DeviceEnergyManagementDelegate::ModifyForecastRequest(
    const uint32_t forecastID, const DataModel::DecodableList<Structs::SlotAdjustmentStruct::DecodableType> & slotAdjustments,
    AdjustmentCauseEnum cause)
{
    Status status = Status::Success;

    if (mForecast.IsNull())
    {
        status = Status::Failure;
    }
    else if (mForecast.Value().forecastID != forecastID)
    {
        status = Status::Failure;
    }
    else if (mpDEMManufacturerDelegate != nullptr)
    {
        // Determine if the new forecast adjustments are acceptable to the appliance
        CHIP_ERROR err = mpDEMManufacturerDelegate->HandleModifyForecastRequest(forecastID, slotAdjustments, cause);
        if (err != CHIP_NO_ERROR)
        {
            status = Status::Failure;
        }
    }

    if (status == Status::Success)
    {
        switch (cause)
        {
        case AdjustmentCauseEnum::kLocalOptimization:
            mForecast.Value().forecastUpdateReason = ForecastUpdateReasonEnum::kLocalOptimization;
            break;
        case AdjustmentCauseEnum::kGridOptimization:
            mForecast.Value().forecastUpdateReason = ForecastUpdateReasonEnum::kGridOptimization;
            break;
        default:
            // Already checked in chip::app::Clusters::DeviceEnergyManagement::Instance::HandleModifyForecastRequest
            break;
        }

        mForecast.Value().forecastID++;

        MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, Forecast::Id);
    }

    return status;
}

/**
 * @brief Delegate handler for RequestConstraintBasedForecast
 *
 * Note: Only basic checking of the validity of the RequestConstraintBasedForecast has been
 * done by the lower layer. This is a more complex use-case and requires higher-level
 * work by the delegate.
 *
 * It should:
 *      1) perform a higher level optimization (e.g. using tariff information, and user preferences)
 *      2) if a solution can be found, then update the forecast, else return Failure
 *      3) notify the appliance to follow the revised schedule
 */
Status DeviceEnergyManagementDelegate::RequestConstraintBasedForecast(
    const DataModel::DecodableList<Structs::ConstraintsStruct::DecodableType> & constraints, AdjustmentCauseEnum cause)
{
    Status status = Status::Success;

    if (mForecast.IsNull())
    {
        status = Status::Failure;
    }
    else if (mpDEMManufacturerDelegate != nullptr)
    {
        // Determine if the new forecast adjustments are acceptable to the appliance
        CHIP_ERROR err = mpDEMManufacturerDelegate->RequestConstraintBasedForecast(constraints, cause);
        if (err != CHIP_NO_ERROR)
        {
            status = Status::Failure;
        }
    }

    if (status == Status::Success)
    {
        switch (cause)
        {
        case AdjustmentCauseEnum::kLocalOptimization:
            mForecast.Value().forecastUpdateReason = ForecastUpdateReasonEnum::kLocalOptimization;
            break;
        case AdjustmentCauseEnum::kGridOptimization:
            mForecast.Value().forecastUpdateReason = ForecastUpdateReasonEnum::kGridOptimization;
            break;
        default:
            // Already checked in chip::app::Clusters::DeviceEnergyManagement::Instance::HandleModifyForecastRequest
            break;
        }

        mForecast.Value().forecastID++;

        MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, Forecast::Id);

        status = Status::Success;
    }

    return status;
}

/**
 * @brief Delegate handler for CancelRequest
 *
 * Note: This is a more complex use-case and requires higher-level work by the delegate.
 *
 * It SHALL:
 *      1) Check if the forecastUpdateReason was already InternalOptimization (and reject the command)
 *      2) Update its forecast (based on its optimization strategy) ignoring previous requests
 *      3) Update its Forecast attribute to match its new intended operation, and update the
 *         ForecastStruct.ForecastUpdateReason to `Internal Optimization`.
 */
Status DeviceEnergyManagementDelegate::CancelRequest()
{
    Status status = Status::Success;

    mForecast.Value().forecastUpdateReason = ForecastUpdateReasonEnum::kInternalOptimization;

    MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, Forecast::Id);

    /* It is expected the mpDEMManufacturerDelegate will cancel the effects of any previous adjustment
     * request commands, and re-evaluate its forecast for intended operation ignoring those previous
     * requests.
     */
    if (mpDEMManufacturerDelegate != nullptr)
    {
        CHIP_ERROR error = mpDEMManufacturerDelegate->HandleDeviceEnergyManagementCancelRequest();
        if (error != CHIP_NO_ERROR)
        {
            status = Status::Failure;
        }
    }

    return status;
}

// ------------------------------------------------------------------
// Get attribute methods
ESATypeEnum DeviceEnergyManagementDelegate::GetESAType()
{
    return mEsaType;
}

bool DeviceEnergyManagementDelegate::GetESACanGenerate()
{
    return mEsaCanGenerate;
}

ESAStateEnum DeviceEnergyManagementDelegate::GetESAState()
{
    return mEsaState;
}

int64_t DeviceEnergyManagementDelegate::GetAbsMinPower()
{
    return mAbsMinPowerMw;
}

int64_t DeviceEnergyManagementDelegate::GetAbsMaxPower()
{
    return mAbsMaxPowerMw;
}

const DataModel::Nullable<Structs::PowerAdjustCapabilityStruct::Type> &
DeviceEnergyManagementDelegate::GetPowerAdjustmentCapability()
{
    return mPowerAdjustCapabilityStruct;
}

const DataModel::Nullable<Structs::ForecastStruct::Type> & DeviceEnergyManagementDelegate::GetForecast()
{
    ChipLogDetail(Zcl, "DeviceEnergyManagementDelegate::GetForecast");

    return mForecast;
}

OptOutStateEnum DeviceEnergyManagementDelegate::GetOptOutState()
{
    ChipLogDetail(AppServer, "mOptOutState %d", to_underlying(mOptOutState));
    return mOptOutState;
}

// ------------------------------------------------------------------
// Set attribute methods

CHIP_ERROR DeviceEnergyManagementDelegate::SetESAType(ESATypeEnum newValue)
{
    ESATypeEnum oldValue = mEsaType;

    if (newValue >= ESATypeEnum::kUnknownEnumValue)
    {
        return CHIP_IM_GLOBAL_STATUS(ConstraintError);
    }

    mEsaType = newValue;
    if (oldValue != newValue)
    {
        ChipLogDetail(AppServer, "mEsaType updated to %d", static_cast<int>(mEsaType));
        MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, ESAType::Id);
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR DeviceEnergyManagementDelegate::SetESACanGenerate(bool newValue)
{
    bool oldValue = mEsaCanGenerate;

    mEsaCanGenerate = newValue;
    if (oldValue != newValue)
    {
        ChipLogDetail(AppServer, "mEsaCanGenerate updated to %d", static_cast<int>(mEsaCanGenerate));
        MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, ESACanGenerate::Id);
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR DeviceEnergyManagementDelegate::SetESAState(ESAStateEnum newValue)
{
    ESAStateEnum oldValue = mEsaState;

    if (newValue >= ESAStateEnum::kUnknownEnumValue)
    {
        return CHIP_IM_GLOBAL_STATUS(ConstraintError);
    }

    mEsaState = newValue;
    if (oldValue != newValue)
    {
        ChipLogDetail(AppServer, "mEsaState updated to %d", static_cast<int>(mEsaState));
        MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, ESAState::Id);
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR DeviceEnergyManagementDelegate::SetAbsMinPower(int64_t newValueMw)
{
    int64_t oldValueMw = mAbsMinPowerMw;

    mAbsMinPowerMw = newValueMw;
    if (oldValueMw != newValueMw)
    {
        ChipLogDetail(AppServer, "mAbsMinPower updated to " ChipLogFormatX64, ChipLogValueX64(mAbsMinPowerMw));
        MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, AbsMinPower::Id);
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR DeviceEnergyManagementDelegate::SetAbsMaxPower(int64_t newValueMw)
{
    int64_t oldValueMw = mAbsMaxPowerMw;

    mAbsMaxPowerMw = newValueMw;
    if (oldValueMw != newValueMw)
    {
        ChipLogDetail(AppServer, "mAbsMaxPower updated to " ChipLogFormatX64, ChipLogValueX64(mAbsMaxPowerMw));
        MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, AbsMaxPower::Id);
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR
DeviceEnergyManagementDelegate::SetPowerAdjustmentCapability(
    const DataModel::Nullable<Structs::PowerAdjustCapabilityStruct::Type> & powerAdjustCapabilityStruct)
{
    assertChipStackLockedByCurrentThread();

    mPowerAdjustCapabilityStruct = powerAdjustCapabilityStruct;

    MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, PowerAdjustmentCapability::Id);

    return CHIP_NO_ERROR;
}

CHIP_ERROR
DeviceEnergyManagementDelegate::SetPowerAdjustmentCapabilityPowerAdjustReason(PowerAdjustReasonEnum powerAdjustReason)
{
    assertChipStackLockedByCurrentThread();

    mPowerAdjustCapabilityStruct.Value().cause = powerAdjustReason;

    MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, PowerAdjustmentCapability::Id);

    return CHIP_NO_ERROR;
}

CHIP_ERROR DeviceEnergyManagementDelegate::SetForecast(const DataModel::Nullable<Structs::ForecastStruct::Type> & forecast)
{
    assertChipStackLockedByCurrentThread();

    // TODO see Issue #31147
    mForecast = forecast;

    MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, Forecast::Id);

    return CHIP_NO_ERROR;
}

CHIP_ERROR DeviceEnergyManagementDelegate::SetOptOutState(OptOutStateEnum newValue)
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    OptOutStateEnum oldValue = mOptOutState;

    // The OptOutState is cumulative
    if ((oldValue == OptOutStateEnum::kGridOptOut && newValue == OptOutStateEnum::kLocalOptOut) ||
        (oldValue == OptOutStateEnum::kLocalOptOut && newValue == OptOutStateEnum::kGridOptOut))
    {
        mOptOutState = OptOutStateEnum::kOptOut;
    }
    else
    {
        mOptOutState = newValue;
    }

    if (oldValue != newValue)
    {
        ChipLogDetail(AppServer, "mOptOutState updated to %d mPowerAdjustmentInProgress %d", to_underlying(mOptOutState),
                      mPowerAdjustmentInProgress);
        MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, OptOutState::Id);
    }

    // Cancel any outstanding PowerAdjustment if necessary
    if (mPowerAdjustmentInProgress)
    {
        if ((newValue == OptOutStateEnum::kLocalOptOut &&
             GetPowerAdjustmentCapability().Value().cause == PowerAdjustReasonEnum::kLocalOptimizationAdjustment) ||
            (newValue == OptOutStateEnum::kGridOptOut &&
             GetPowerAdjustmentCapability().Value().cause == PowerAdjustReasonEnum::kGridOptimizationAdjustment) ||
            newValue == OptOutStateEnum::kOptOut)
        {
            err = CancelPowerAdjustRequest(DeviceEnergyManagement::CauseEnum::kUserOptOut);
        }
    }

    // Cancel any outstanding PauseRequest if necessary
    if (mPauseRequestInProgress)
    {
        // Cancel any outstanding PauseRequest
        if ((newValue == OptOutStateEnum::kLocalOptOut &&
             mForecast.Value().forecastUpdateReason == ForecastUpdateReasonEnum::kLocalOptimization) ||
            (newValue == OptOutStateEnum::kGridOptOut &&
             mForecast.Value().forecastUpdateReason == ForecastUpdateReasonEnum::kGridOptimization) ||
            newValue == OptOutStateEnum::kOptOut)
        {
            err = CancelPauseRequest(DeviceEnergyManagement::CauseEnum::kUserOptOut);
        }
    }

    if (!mForecast.IsNull())
    {
        switch (mForecast.Value().forecastUpdateReason)
        {
        case ForecastUpdateReasonEnum::kInternalOptimization:
            // We don't need to redo a forecast since its internal already
            break;
        case ForecastUpdateReasonEnum::kLocalOptimization:
            if ((mOptOutState == OptOutStateEnum::kOptOut) || (mOptOutState == OptOutStateEnum::kLocalOptOut))
            {
                mForecast.Value().forecastUpdateReason = ForecastUpdateReasonEnum::kInternalOptimization;

                MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, Forecast::Id);
                // Generate a new forecast with Internal Optimization
                // TODO
            }
            break;
        case ForecastUpdateReasonEnum::kGridOptimization:
            if ((mOptOutState == OptOutStateEnum::kOptOut) || (mOptOutState == OptOutStateEnum::kGridOptOut))
            {
                mForecast.Value().forecastUpdateReason = ForecastUpdateReasonEnum::kInternalOptimization;

                MatterReportingAttributeChangeCallback(mEndpointId, DeviceEnergyManagement::Id, Forecast::Id);
                // Generate a new forecast with Internal Optimization
                // TODO
            }
            break;
        default:
            ChipLogDetail(AppServer, "Bad ForecastUpdateReasonEnum value of %d",
                          to_underlying(mForecast.Value().forecastUpdateReason));
            return CHIP_ERROR_BAD_REQUEST;
            break;
        }
    }

    return err;
}
