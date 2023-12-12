/*
 *
 *    Copyright (c) 2023 Project CHIP Authors
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

#include <app-common/zap-generated/attributes/Accessors.h>
#include <app/InteractionModelEngine.h>
#include <app/SafeAttributePersistenceProvider.h>
#include <app/clusters/mode-base-server/mode-base-server.h>
#include <app/clusters/on-off-server/on-off-server.h>
#include <app/reporting/reporting.h>
#include <app/util/attribute-storage.h>
#include <platform/DiagnosticDataProvider.h>

#ifdef EMBER_AF_PLUGIN_SCENES
#include <app/clusters/scenes-server/scenes-server.h>
#endif // EMBER_AF_PLUGIN_SCENES

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using chip::Protocols::InteractionModel::Status;
using BootReasonType       = GeneralDiagnostics::BootReasonEnum;
using ModeOptionStructType = chip::app::Clusters::detail::Structs::ModeOptionStruct::Type;
using ModeTagStructType    = chip::app::Clusters::detail::Structs::ModeTagStruct::Type;

namespace chip {
namespace app {
namespace Clusters {
namespace ModeBase {

#if defined(EMBER_AF_PLUGIN_SCENES) && CHIP_CONFIG_SCENES_USE_DEFAULT_HANDLERS

static void TimerCallback(System::Layer * aLayer, void * aAppState)
{
    Instance::DefaultModeBaseSceneHandler * handler = static_cast<Instance::DefaultModeBaseSceneHandler *>(aAppState);
    handler->SceneCallback();
}

// Default function for the mode base cluster, only puts the mode base cluster ID in the span if supported on the given
// endpoint. This assumes a single handler for a mode base cluster per endpoint. If multiple handlers are needed, a custom
// scene handler should be implemented.
void Instance::DefaultModeBaseSceneHandler::GetSupportedClusters(EndpointId endpoint, Span<ClusterId> & clusterBuffer)
{
    ClusterId * buffer = clusterBuffer.data();

    if (mInstance != nullptr && endpoint == mInstance->GetEndpointId())
    {
        buffer[0] = mInstance->GetClusterId();
        clusterBuffer.reduce_size(1);
    }
    else
    {
        clusterBuffer.reduce_size(0);
    }
}

// Default function for mode base cluster, only checks if mode base is enabled on the endpoint
bool Instance::DefaultModeBaseSceneHandler::SupportsCluster(EndpointId endpoint, ClusterId cluster)
{
    return mInstance != nullptr && endpoint == mInstance->GetEndpointId() && cluster == mInstance->GetClusterId();
}

/// @brief Serialize the Cluster's EFS value
/// @param endpoint target endpoint
/// @param cluster  target cluster
/// @param serializedBytes data to serialize into EFS
/// @return CHIP_NO_ERROR if successfully serialized the data, CHIP_ERROR_INVALID_ARGUMENT otherwise
CHIP_ERROR Instance::DefaultModeBaseSceneHandler::SerializeSave(EndpointId endpoint, ClusterId cluster,
                                                                MutableByteSpan & serializedBytes)
{
    using AttributeValuePair = Scenes::Structs::AttributeValuePair::Type;

    if (mInstance == nullptr)
    {
        ChipLogError(Zcl, "ERR: Failed to serialize CurrentMode on Endpoint %x", endpoint);
        return CHIP_ERROR_INCORRECT_STATE;
    }

    uint8_t currentMode = mInstance->GetCurrentMode();

    AttributeValuePair pairs[scenableAttributeCount];

    pairs[0].attributeID    = Attributes::CurrentMode::Id;
    pairs[0].attributeValue = currentMode;

    app::DataModel::List<AttributeValuePair> attributeValueList(pairs);

    return EncodeAttributeValueList(attributeValueList, serializedBytes);
}

/// @brief Default EFS interaction when applying scene to the ModeBase Cluster
/// @param endpoint target endpoint
/// @param cluster  target cluster
/// @param serializedBytes Data from nvm
/// @param timeMs transition time in ms
/// @return CHIP_NO_ERROR if value as expected, CHIP_ERROR_INVALID_ARGUMENT otherwise
CHIP_ERROR Instance::DefaultModeBaseSceneHandler::ApplyScene(EndpointId endpoint, ClusterId cluster,
                                                             const ByteSpan & serializedBytes, scenes::TransitionTimeMs timeMs)
{
    app::DataModel::DecodableList<Scenes::Structs::AttributeValuePair::DecodableType> attributeValueList;

    VerifyOrReturnError(cluster == mInstance->GetClusterId(), CHIP_ERROR_INVALID_ARGUMENT);

    ReturnErrorOnFailure(DecodeAttributeValueList(serializedBytes, attributeValueList));

    size_t attributeCount = 0;
    ReturnErrorOnFailure(attributeValueList.ComputeSize(&attributeCount));
    VerifyOrReturnError(attributeCount <= scenableAttributeCount, CHIP_ERROR_BUFFER_TOO_SMALL);

    auto pair_iterator = attributeValueList.begin();
    while (pair_iterator.Next())
    {
        auto & decodePair = pair_iterator.GetValue();
        VerifyOrReturnError(decodePair.attributeID == Attributes::CurrentMode::Id, CHIP_ERROR_INVALID_ARGUMENT);
        mSceneNextMode = static_cast<uint8_t>(decodePair.attributeValue);
    }
    // Verify that the EFS was completely read
    ReturnErrorOnFailure(pair_iterator.GetStatus());

    StartTimer(chip::System::Clock::Milliseconds32(timeMs));

    return CHIP_NO_ERROR;
}

CHIP_ERROR Instance::DefaultModeBaseSceneHandler::StartTimer(chip::System::Clock::Milliseconds32 aTimeout)
{
    return DeviceLayer::SystemLayer().StartTimer(aTimeout, TimerCallback, this);
}

void Instance::DefaultModeBaseSceneHandler::CancelTimer()
{
    DeviceLayer::SystemLayer().CancelTimer(TimerCallback, this);
}

void Instance::DefaultModeBaseSceneHandler::SceneCallback()
{
    mInstance->UpdateCurrentMode(mSceneNextMode);
}
#endif // defined(EMBER_AF_PLUGIN_SCENES) && CHIP_CONFIG_SCENES_USE_DEFAULT_HANDLERS

Instance::Instance(Delegate * aDelegate, EndpointId aEndpointId, ClusterId aClusterId, uint32_t aFeature) :
    CommandHandlerInterface(Optional<EndpointId>(aEndpointId), aClusterId),
    AttributeAccessInterface(Optional<EndpointId>(aEndpointId), aClusterId), mDelegate(aDelegate), mEndpointId(aEndpointId),
    mClusterId(aClusterId),
    mCurrentMode(0), // This is a temporary value and may not be valid. We will change this to the value of the first
                     // mode in the list at the start of the Init function to ensure that it represents a valid mode.
    mFeature(aFeature)
{
    mDelegate->SetInstance(this);
}

Instance::~Instance()
{
    Shutdown();
}

void Instance::Shutdown()
{
    if (!IsInList())
    {
        return;
    }
    UnregisterThisInstance();
    chip::app::InteractionModelEngine::GetInstance()->UnregisterCommandHandler(this);
    unregisterAttributeAccessOverride(this);
#if defined(EMBER_AF_PLUGIN_SCENES) && CHIP_CONFIG_SCENES_USE_DEFAULT_HANDLERS
    mSceneHandler.CancelTimer();
    Scenes::ScenesServer::Instance().UnregisterSceneHandler(mEndpointId, &mSceneHandler);
#endif // defined(EMBER_AF_PLUGIN_SCENES) && CHIP_CONFIG_SCENES_USE_DEFAULT_HANDLERS
}

CHIP_ERROR Instance::Init()
{
    // Initialise the current mode with the value of the first mode. This ensures that it is representing a valid mode.
    ReturnErrorOnFailure(mDelegate->GetModeValueByIndex(0, mCurrentMode));

    // Check if the cluster has been selected in zap
    VerifyOrDie(emberAfContainsServer(mEndpointId, mClusterId) == true);

    LoadPersistentAttributes();

    ReturnErrorOnFailure(chip::app::InteractionModelEngine::GetInstance()->RegisterCommandHandler(this));
    VerifyOrReturnError(registerAttributeAccessOverride(this), CHIP_ERROR_INCORRECT_STATE);
    RegisterThisInstance();
    ReturnErrorOnFailure(mDelegate->Init());

    // If the StartUpMode is set, the CurrentMode attribute SHALL be set to the StartUpMode value, when the server is powered
    // up.
    if (!mStartUpMode.IsNull())
    {
        // This behavior does not apply to reboots associated with OTA.
        // After an OTA restart, the CurrentMode attribute SHALL return to its value prior to the restart.
        // todo this only works for matter OTAs. According to the spec, this should also work for general OTAs.
        BootReasonType bootReason = BootReasonType::kUnspecified;
        CHIP_ERROR error          = DeviceLayer::GetDiagnosticDataProvider().GetBootReason(bootReason);

        if (error != CHIP_NO_ERROR)
        {
            ChipLogError(
                Zcl, "Unable to retrieve boot reason: %" CHIP_ERROR_FORMAT ". Assuming that we did not reboot because of an OTA",
                error.Format());
            bootReason = BootReasonType::kUnspecified;
        }

        if (bootReason == BootReasonType::kSoftwareUpdateCompleted)
        {
            ChipLogDetail(Zcl, "ModeBase: StartUpMode is ignored for OTA reboot.");
        }
        else
        {
            // Set CurrentMode to StartUpMode
            if (mStartUpMode.Value() != mCurrentMode)
            {
                ChipLogProgress(Zcl, "ModeBase: Changing CurrentMode to the StartUpMode value.");
                Status status = UpdateCurrentMode(mStartUpMode.Value());
                if (status != Status::Success)
                {
                    ChipLogError(Zcl, "ModeBase: Failed to change the CurrentMode to the StartUpMode value: %u",
                                 to_underlying(status));
                    return StatusIB(status).ToChipError();
                }

                ChipLogProgress(Zcl, "ModeBase: Successfully initialized CurrentMode to the StartUpMode value %u",
                                mStartUpMode.Value());
            }
        }
    }

#ifdef EMBER_AF_PLUGIN_ON_OFF_SERVER
    // OnMode with Power Up
    // If the On/Off feature is supported and the On/Off cluster attribute StartUpOnOff is present, with a
    // value of On (turn on at power up), then the CurrentMode attribute SHALL be set to the OnMode attribute
    // value when the server is supplied with power, except if the OnMode attribute is null.
    if (emberAfContainsServer(mEndpointId, OnOff::Id) &&
        emberAfContainsAttribute(mEndpointId, OnOff::Id, OnOff::Attributes::StartUpOnOff::Id) &&
        emberAfContainsAttribute(mEndpointId, mClusterId, ModeBase::Attributes::OnMode::Id) &&
        HasFeature(ModeBase::Feature::kOnOff))
    {
        DataModel::Nullable<uint8_t> onMode = GetOnMode();
        bool onOffValueForStartUp           = false;
        if (!emberAfIsKnownVolatileAttribute(mEndpointId, OnOff::Id, OnOff::Attributes::StartUpOnOff::Id) &&
            OnOffServer::Instance().getOnOffValueForStartUp(mEndpointId, onOffValueForStartUp) == EMBER_ZCL_STATUS_SUCCESS)
        {
            if (onOffValueForStartUp && !onMode.IsNull())
            {
                // Set CurrentMode to OnMode
                if (mOnMode.Value() != mCurrentMode)
                {
                    ChipLogProgress(Zcl, "ModeBase: Changing CurrentMode to the OnMode value.");
                    Status status = UpdateCurrentMode(mOnMode.Value());
                    if (status != Status::Success)
                    {
                        ChipLogError(Zcl, "ModeBase: Failed to change the CurrentMode to the OnMode value: %u",
                                     to_underlying(status));
                        return StatusIB(status).ToChipError();
                    }

                    ChipLogProgress(Zcl, "ModeBase: Successfully initialized CurrentMode to the OnMode value %u", mOnMode.Value());
                }
            }
        }
    }
#endif // EMBER_AF_PLUGIN_ON_OFF_SERVER

#if defined(EMBER_AF_PLUGIN_SCENES) && CHIP_CONFIG_SCENES_USE_DEFAULT_HANDLERS
    // Registers Scene handlers for the color control cluster on the server
    Scenes::ScenesServer::Instance().RegisterSceneHandler(mEndpointId, &mSceneHandler);
#endif // defined(EMBER_AF_PLUGIN_SCENES) && CHIP_CONFIG_SCENES_USE_DEFAULT_HANDLERS

    return CHIP_NO_ERROR;
}

Status Instance::UpdateCurrentMode(uint8_t aNewMode)
{
    if (!IsSupportedMode(aNewMode))
    {
        return Protocols::InteractionModel::Status::ConstraintError;
    }
    uint8_t oldMode = mCurrentMode;
    mCurrentMode    = aNewMode;
    if (mCurrentMode != oldMode)
    {
        // Write new value to persistent storage.
        ConcreteAttributePath path = ConcreteAttributePath(mEndpointId, mClusterId, Attributes::CurrentMode::Id);
        GetSafeAttributePersistenceProvider()->WriteScalarValue(path, mCurrentMode);
        MatterReportingAttributeChangeCallback(path);
    }
    return Protocols::InteractionModel::Status::Success;
}

Status Instance::UpdateStartUpMode(DataModel::Nullable<uint8_t> aNewStartUpMode)
{
    if (!aNewStartUpMode.IsNull())
    {
        if (!IsSupportedMode(aNewStartUpMode.Value()))
        {
            return Protocols::InteractionModel::Status::ConstraintError;
        }
    }
    DataModel::Nullable<uint8_t> oldStartUpMode = mStartUpMode;
    mStartUpMode                                = aNewStartUpMode;
    if (mStartUpMode != oldStartUpMode)
    {
        // Write new value to persistent storage.
        ConcreteAttributePath path = ConcreteAttributePath(mEndpointId, mClusterId, Attributes::StartUpMode::Id);
        GetSafeAttributePersistenceProvider()->WriteScalarValue(path, mStartUpMode);
        MatterReportingAttributeChangeCallback(path);
    }
    return Protocols::InteractionModel::Status::Success;
}

Status Instance::UpdateOnMode(DataModel::Nullable<uint8_t> aNewOnMode)
{
    if (!aNewOnMode.IsNull())
    {
        if (!IsSupportedMode(aNewOnMode.Value()))
        {
            return Protocols::InteractionModel::Status::ConstraintError;
        }
    }
    DataModel::Nullable<uint8_t> oldOnMode = mOnMode;
    mOnMode                                = aNewOnMode;
    if (mOnMode != oldOnMode)
    {
        // Write new value to persistent storage.
        ConcreteAttributePath path = ConcreteAttributePath(mEndpointId, mClusterId, Attributes::OnMode::Id);
        GetSafeAttributePersistenceProvider()->WriteScalarValue(path, mOnMode);
        MatterReportingAttributeChangeCallback(path);
    }
    return Protocols::InteractionModel::Status::Success;
}

uint8_t Instance::GetCurrentMode() const
{
    return mCurrentMode;
}

DataModel::Nullable<uint8_t> Instance::GetStartUpMode() const
{
    return mStartUpMode;
}

DataModel::Nullable<uint8_t> Instance::GetOnMode() const
{
    return mOnMode;
}

void Instance::ReportSupportedModesChange()
{
    MatterReportingAttributeChangeCallback(ConcreteAttributePath(mEndpointId, mClusterId, Attributes::SupportedModes::Id));
}

bool Instance::HasFeature(Feature feature) const
{
    return (mFeature & to_underlying(feature)) != 0;
}

bool Instance::IsSupportedMode(uint8_t modeValue)
{
    uint8_t value;
    for (uint8_t i = 0; mDelegate->GetModeValueByIndex(i, value) != CHIP_ERROR_PROVIDER_LIST_EXHAUSTED; i++)
    {
        if (value == modeValue)
        {
            return true;
        }
    }
    ChipLogDetail(Zcl, "Cannot find a mode with value %u", modeValue);
    return false;
}

// private methods
template <typename RequestT, typename FuncT>
void Instance::HandleCommand(HandlerContext & handlerContext, FuncT func)
{
    if (!handlerContext.mCommandHandled && (handlerContext.mRequestPath.mCommandId == RequestT::GetCommandId()))
    {
        RequestT requestPayload;

        // If the command matches what the caller is looking for, let's mark this as being handled
        // even if errors happen after this. This ensures that we don't execute any fall-back strategies
        // to handle this command since at this point, the caller is taking responsibility for handling
        // the command in its entirety, warts and all.
        //
        handlerContext.SetCommandHandled();

        if (DataModel::Decode(handlerContext.mPayload, requestPayload) != CHIP_NO_ERROR)
        {
            handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath,
                                                     Protocols::InteractionModel::Status::InvalidCommand);
            return;
        }

        func(handlerContext, requestPayload);
    }
}

// This function is called by the interaction model engine when a command destined for this instance is received.
void Instance::InvokeCommand(HandlerContext & handlerContext)
{
    switch (handlerContext.mRequestPath.mCommandId)
    {
    case ModeBase::Commands::ChangeToMode::Id:
        ChipLogDetail(Zcl, "ModeBase: Entering handling ChangeToModeWithStatus");

        HandleCommand<Commands::ChangeToMode::DecodableType>(
            handlerContext, [this](HandlerContext & ctx, const auto & commandData) { HandleChangeToMode(ctx, commandData); });
    }
}

// Implements the read functionality for complex attributes.
CHIP_ERROR Instance::Read(const ConcreteReadAttributePath & aPath, AttributeValueEncoder & aEncoder)
{
    switch (aPath.mAttributeId)
    {
    case Attributes::CurrentMode::Id:
        ReturnErrorOnFailure(aEncoder.Encode(mCurrentMode));
        break;
    case Attributes::StartUpMode::Id:
        ReturnErrorOnFailure(aEncoder.Encode(mStartUpMode));
        break;
    case Attributes::OnMode::Id:
        ReturnErrorOnFailure(aEncoder.Encode(mOnMode));
        break;
    case Attributes::FeatureMap::Id:
        ReturnErrorOnFailure(aEncoder.Encode(mFeature));
        break;
    case Attributes::SupportedModes::Id:
        Instance * d   = this;
        CHIP_ERROR err = aEncoder.EncodeList([d](const auto & encoder) -> CHIP_ERROR { return d->EncodeSupportedModes(encoder); });
        return err;
    }
    return CHIP_NO_ERROR;
}

// Implements checking before attribute writes.
CHIP_ERROR Instance::Write(const ConcreteDataAttributePath & attributePath, AttributeValueDecoder & aDecoder)
{
    DataModel::Nullable<uint8_t> newMode;
    ReturnErrorOnFailure(aDecoder.Decode(newMode));
    Status status;

    switch (attributePath.mAttributeId)
    {
    case ModeBase::Attributes::StartUpMode::Id:
        status = UpdateStartUpMode(newMode);
        return StatusIB(status).ToChipError();
    case ModeBase::Attributes::OnMode::Id:
        status = UpdateOnMode(newMode);
        return StatusIB(status).ToChipError();
    }

    return CHIP_ERROR_INCORRECT_STATE;
}

void Instance::RegisterThisInstance()
{
    if (!gModeBaseAliasesInstances.Contains(this))
    {
        gModeBaseAliasesInstances.PushBack(this);
    }
}

void Instance::UnregisterThisInstance()
{
    gModeBaseAliasesInstances.Remove(this);
}

void Instance::HandleChangeToMode(HandlerContext & ctx, const Commands::ChangeToMode::DecodableType & commandData)
{
    uint8_t newMode = commandData.newMode;

    Commands::ChangeToModeResponse::Type response;

    // If the NewMode field doesn't match the Mode field of any entry of the SupportedModes list,
    // the ChangeToModeResponse command's Status field SHALL indicate UnsupportedMode and
    // the StatusText field SHALL be included and MAY be used to indicate the issue, with a human readable string,
    // or include an empty string.
    // We are leaving the StatusText empty since the Status is descriptive enough.
    if (!IsSupportedMode(newMode))
    {
        ChipLogError(Zcl, "ModeBase: Failed to find the option with mode %u", newMode);
        response.status = to_underlying(StatusCode::kUnsupportedMode);
        ctx.mCommandHandler.AddResponse(ctx.mRequestPath, response);
        return;
    }

    // If the NewMode field is the same as the value of the CurrentMode attribute
    // the ChangeToModeResponse command SHALL have the Status field set to Success and
    // the StatusText field MAY be supplied with a human readable string or include an empty string.
    // We are leaving the StatusText empty since the Status is descriptive enough.
    if (newMode == GetCurrentMode())
    {
        response.status = to_underlying(ModeBase::StatusCode::kSuccess);
        ctx.mCommandHandler.AddResponse(ctx.mRequestPath, response);
        return;
    }

    mDelegate->HandleChangeToMode(newMode, response);

    if (response.status == to_underlying(StatusCode::kSuccess))
    {
        UpdateCurrentMode(newMode);
        ChipLogProgress(Zcl, "ModeBase: HandleChangeToMode changed to mode %u", newMode);
#ifdef EMBER_AF_PLUGIN_SCENES
        //  the scene has been changed (the value of CurrentMode has changed) so
        //  the current scene as described in the scene table is invalid
        Scenes::ScenesServer::Instance().MakeSceneInvalidForAllFabrics(mEndpointId);
#endif // EMBER_AF_PLUGIN_SCENES
    }

    ctx.mCommandHandler.AddResponse(ctx.mRequestPath, response);
}

void Instance::LoadPersistentAttributes()
{
    // Load Current Mode
    uint8_t tempCurrentMode;
    CHIP_ERROR err = GetSafeAttributePersistenceProvider()->ReadScalarValue(
        ConcreteAttributePath(mEndpointId, mClusterId, Attributes::CurrentMode::Id), tempCurrentMode);
    if (err == CHIP_NO_ERROR)
    {
        Status status = UpdateCurrentMode(tempCurrentMode);
        if (status == Status::Success)
        {
            ChipLogDetail(Zcl, "ModeBase: Loaded CurrentMode as %u", GetCurrentMode());
        }
        else
        {
            ChipLogError(Zcl, "ModeBase: Could not update CurrentMode to %u: %u", tempCurrentMode, to_underlying(status));
        }
    }
    else
    {
        // If we cannot find the previous CurrentMode, we will assume it to be the first mode in the
        // list, as was initialised in the constructor.
        ChipLogDetail(Zcl, "ModeBase: Unable to load the CurrentMode from the KVS. Assuming %u", GetCurrentMode());
    }

    // Load Start-Up Mode
    DataModel::Nullable<uint8_t> tempStartUpMode;
    err = GetSafeAttributePersistenceProvider()->ReadScalarValue(
        ConcreteAttributePath(mEndpointId, mClusterId, Attributes::StartUpMode::Id), tempStartUpMode);
    if (err == CHIP_NO_ERROR)
    {
        Status status = UpdateStartUpMode(tempStartUpMode);
        if (status == Status::Success)
        {
            if (GetStartUpMode().IsNull())
            {
                ChipLogDetail(Zcl, "ModeBase: Loaded StartUpMode as null");
            }
            else
            {
                ChipLogDetail(Zcl, "ModeBase: Loaded StartUpMode as %u", GetStartUpMode().Value());
            }
        }
        else
        {
            ChipLogError(Zcl, "ModeBase: Could not update StartUpMode: %u", to_underlying(status));
        }
    }
    else
    {
        ChipLogDetail(Zcl, "ModeBase: Unable to load the StartUpMode from the KVS. Assuming null");
    }

    // Load On Mode
    DataModel::Nullable<uint8_t> tempOnMode;
    err = GetSafeAttributePersistenceProvider()->ReadScalarValue(
        ConcreteAttributePath(mEndpointId, mClusterId, Attributes::OnMode::Id), tempOnMode);
    if (err == CHIP_NO_ERROR)
    {
        Status status = UpdateOnMode(tempOnMode);
        if (status == Status::Success)
        {
            if (GetOnMode().IsNull())
            {
                ChipLogDetail(Zcl, "ModeBase: Loaded OnMode as null");
            }
            else
            {
                ChipLogDetail(Zcl, "ModeBase: Loaded OnMode as %u", GetOnMode().Value());
            }
        }
        else
        {
            ChipLogError(Zcl, "ModeBase: Could not update OnMode: %u", to_underlying(status));
        }
    }
    else
    {
        ChipLogDetail(Zcl, "ModeBase: Unable to load the OnMode from the KVS.      Assuming null");
    }
}

CHIP_ERROR Instance::EncodeSupportedModes(const AttributeValueEncoder::ListEncodeHelper & encoder)
{
    for (uint8_t i = 0; true; i++)
    {
        ModeOptionStructType mode;

        // Get the mode label
        char buffer[kMaxModeLabelSize];
        MutableCharSpan label(buffer);
        auto err = mDelegate->GetModeLabelByIndex(i, label);
        if (err == CHIP_ERROR_PROVIDER_LIST_EXHAUSTED)
        {
            return CHIP_NO_ERROR;
        }
        ReturnErrorOnFailure(err);

        mode.label = label;

        // Get the mode value
        ReturnErrorOnFailure(mDelegate->GetModeValueByIndex(i, mode.mode));

        // Get the mode tags
        ModeTagStructType tagsBuffer[kMaxNumOfModeTags];
        DataModel::List<ModeTagStructType> tags(tagsBuffer);
        ReturnErrorOnFailure(mDelegate->GetModeTagsByIndex(i, tags));
        mode.modeTags = tags;

        ReturnErrorOnFailure(encoder.Encode(mode));
    }
    return CHIP_NO_ERROR;
}

IntrusiveList<Instance> & GetModeBaseInstanceList()
{
    return gModeBaseAliasesInstances;
}

} // namespace ModeBase
} // namespace Clusters
} // namespace app
} // namespace chip
