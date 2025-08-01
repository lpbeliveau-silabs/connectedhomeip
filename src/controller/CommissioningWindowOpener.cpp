/*
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

#include <app-common/zap-generated/cluster-objects.h>
#include <controller/CommissioningWindowOpener.h>
#include <lib/core/CHIPSafeCasts.h>
#include <lib/support/CHIPMem.h>
#include <protocols/secure_channel/PASESession.h>
#include <setup_payload/ManualSetupPayloadGenerator.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>

using namespace chip::app::Clusters;
using namespace chip::System::Clock;
using namespace chip::Crypto;

namespace {
// TODO: What should the timed invoke timeout here be?
constexpr uint16_t kTimedInvokeTimeoutMs = 10000;
} // anonymous namespace

namespace chip {
namespace Controller {

CHIP_ERROR CommissioningWindowOpener::OpenBasicCommissioningWindow(NodeId deviceId, Seconds16 timeout,
                                                                   Callback::Callback<OnOpenBasicCommissioningWindow> * callback)
{
    VerifyOrReturnError(mNextStep == Step::kAcceptCommissioningStart, CHIP_ERROR_INCORRECT_STATE);
    mSetupPayload = SetupPayload();

    // Basic commissioning does not use the setup payload.

    mCommissioningWindowOption           = CommissioningWindowOption::kOriginalSetupCode;
    mBasicCommissioningWindowCallback    = callback;
    mCommissioningWindowCallback         = nullptr;
    mCommissioningWindowVerifierCallback = nullptr;
    mNodeId                              = deviceId;
    mCommissioningWindowTimeout          = timeout;

    mNextStep = Step::kOpenCommissioningWindow;
    return mController->GetConnectedDevice(mNodeId, &mDeviceConnected, &mDeviceConnectionFailure);
}

CHIP_ERROR CommissioningWindowOpener::OpenCommissioningWindow(NodeId deviceId, Seconds16 timeout, uint32_t iteration,
                                                              uint16_t discriminator, Optional<uint32_t> setupPIN,
                                                              Optional<ByteSpan> salt,
                                                              Callback::Callback<OnOpenCommissioningWindow> * callback,
                                                              SetupPayload & payload, bool readVIDPIDAttributes)
{
    return OpenCommissioningWindow(CommissioningWindowPasscodeParams()
                                       .SetNodeId(deviceId)
                                       .SetTimeout(timeout)
                                       .SetIteration(iteration)
                                       .SetDiscriminator(discriminator)
                                       .SetSetupPIN(setupPIN)
                                       .SetSalt(salt)
                                       .SetReadVIDPIDAttributes(readVIDPIDAttributes)
                                       .SetCallback(callback),
                                   payload);
}

CHIP_ERROR CommissioningWindowOpener::OpenCommissioningWindow(const CommissioningWindowPasscodeParams & params,
                                                              SetupPayload & payload)
{
    VerifyOrReturnError(mNextStep == Step::kAcceptCommissioningStart, CHIP_ERROR_INCORRECT_STATE);
    VerifyOrReturnError(params.HasNodeId(), CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(params.HasDiscriminator(), CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(kSpake2p_Min_PBKDF_Iterations <= params.GetIteration() &&
                            params.GetIteration() <= kSpake2p_Max_PBKDF_Iterations,
                        CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(!params.HasSalt() ||
                            (params.GetSalt().size() >= kSpake2p_Min_PBKDF_Salt_Length &&
                             params.GetSalt().size() <= kSpake2p_Max_PBKDF_Salt_Length),
                        CHIP_ERROR_INVALID_ARGUMENT);

    mSetupPayload = SetupPayload();

    if (params.HasSetupPIN())
    {
        VerifyOrReturnError(SetupPayload::IsValidSetupPIN(params.GetSetupPIN()), CHIP_ERROR_INVALID_ARGUMENT);
        mCommissioningWindowOption = CommissioningWindowOption::kTokenWithProvidedPIN;
        mSetupPayload.setUpPINCode = params.GetSetupPIN();
    }
    else
    {
        mCommissioningWindowOption = CommissioningWindowOption::kTokenWithRandomPIN;
    }

    mSetupPayload.version = 0;
    mDiscriminator.SetLongValue(params.GetDiscriminator());
    mSetupPayload.discriminator = mDiscriminator;
    mSetupPayload.rendezvousInformation.SetValue(RendezvousInformationFlag::kOnNetwork);

    if (params.HasSalt())
    {
        memcpy(mPBKDFSaltBuffer, params.GetSalt().data(), params.GetSalt().size());
        mPBKDFSalt = ByteSpan(mPBKDFSaltBuffer, params.GetSalt().size());
    }
    else
    {
        ReturnErrorOnFailure(DRBG_get_bytes(mPBKDFSaltBuffer, sizeof(mPBKDFSaltBuffer)));
        mPBKDFSalt = ByteSpan(mPBKDFSaltBuffer);
    }
    mPBKDFIterations = params.GetIteration();

    bool randomSetupPIN = !params.HasSetupPIN();
    ReturnErrorOnFailure(
        PASESession::GeneratePASEVerifier(mVerifier, mPBKDFIterations, mPBKDFSalt, randomSetupPIN, mSetupPayload.setUpPINCode));

    payload                              = mSetupPayload;
    mCommissioningWindowCallback         = params.GetCallback();
    mBasicCommissioningWindowCallback    = nullptr;
    mCommissioningWindowVerifierCallback = nullptr;
    mNodeId                              = params.GetNodeId();
    mCommissioningWindowTimeout          = params.GetTimeout();
    mTargetEndpointId                    = params.GetEndpointId();

    if (params.GetReadVIDPIDAttributes())
    {
        mNextStep = Step::kReadVID;
    }
    else
    {
        mNextStep = Step::kOpenCommissioningWindow;
    }

    return mController->GetConnectedDevice(mNodeId, &mDeviceConnected, &mDeviceConnectionFailure);
}

#if CHIP_DEVICE_CONFIG_ENABLE_JOINT_FABRIC
CHIP_ERROR CommissioningWindowOpener::OpenJointCommissioningWindow(const CommissioningWindowPasscodeParams & params,
                                                                   SetupPayload & payload)
{
    mJointCommissioning = true;
    return OpenCommissioningWindow(params, payload);
}
#endif // CHIP_DEVICE_CONFIG_ENABLE_JOINT_FABRIC

CHIP_ERROR CommissioningWindowOpener::OpenCommissioningWindow(const CommissioningWindowVerifierParams & params)
{
    VerifyOrReturnError(mNextStep == Step::kAcceptCommissioningStart, CHIP_ERROR_INCORRECT_STATE);
    VerifyOrReturnError(params.HasNodeId(), CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(params.HasDiscriminator(), CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(kSpake2p_Min_PBKDF_Iterations <= params.GetIteration() &&
                            params.GetIteration() <= kSpake2p_Max_PBKDF_Iterations,
                        CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(params.GetSalt().size() >= kSpake2p_Min_PBKDF_Salt_Length &&
                            params.GetSalt().size() <= kSpake2p_Max_PBKDF_Salt_Length,
                        CHIP_ERROR_INVALID_ARGUMENT);
    memcpy(mPBKDFSaltBuffer, params.GetSalt().data(), params.GetSalt().size());
    mPBKDFSalt = ByteSpan(mPBKDFSaltBuffer, params.GetSalt().size());

    ReturnErrorOnFailure(mVerifier.Deserialize(params.GetVerifier()));
    mCommissioningWindowVerifierCallback = params.GetCallback();
    mBasicCommissioningWindowCallback    = nullptr;
    mCommissioningWindowCallback         = nullptr;
    mNodeId                              = params.GetNodeId();
    mCommissioningWindowTimeout          = params.GetTimeout();
    mPBKDFIterations                     = params.GetIteration();
    mCommissioningWindowOption           = CommissioningWindowOption::kTokenWithProvidedPIN;
    mDiscriminator.SetLongValue(params.GetDiscriminator());
    mTargetEndpointId = params.GetEndpointId();

    mNextStep = Step::kOpenCommissioningWindow;

    return mController->GetConnectedDevice(mNodeId, &mDeviceConnected, &mDeviceConnectionFailure);
}

CHIP_ERROR CommissioningWindowOpener::OpenCommissioningWindowInternal(Messaging::ExchangeManager & exchangeMgr,
                                                                      const SessionHandle & sessionHandle)
{
    ChipLogProgress(Controller, "OpenCommissioningWindow for device ID 0x" ChipLogFormatX64, ChipLogValueX64(mNodeId));

    ClusterBase cluster(exchangeMgr, sessionHandle, mTargetEndpointId);

    if (mCommissioningWindowOption != CommissioningWindowOption::kOriginalSetupCode)
    {
        Spake2pVerifierSerialized serializedVerifier;
        MutableByteSpan serializedVerifierSpan(serializedVerifier);
        ReturnErrorOnFailure(mVerifier.Serialize(serializedVerifierSpan));

#if CHIP_DEVICE_CONFIG_ENABLE_JOINT_FABRIC
        if (mJointCommissioning == false)
        {
#endif // CHIP_DEVICE_CONFIG_ENABLE_JOINT_FABRIC
            AdministratorCommissioning::Commands::OpenCommissioningWindow::Type request;
            request.commissioningTimeout = mCommissioningWindowTimeout.count();
            request.PAKEPasscodeVerifier = serializedVerifierSpan;
            request.discriminator        = mDiscriminator.GetLongValue();
            request.iterations           = mPBKDFIterations;
            request.salt                 = mPBKDFSalt;

            ReturnErrorOnFailure(cluster.InvokeCommand(request, this, OnOpenCommissioningWindowSuccess,
                                                       OnOpenCommissioningWindowFailure, MakeOptional(kTimedInvokeTimeoutMs)));
#if CHIP_DEVICE_CONFIG_ENABLE_JOINT_FABRIC
        }
        else
        {
            mJointCommissioning = false; // Reset the flag for next use

            JointFabricAdministrator::Commands::OpenJointCommissioningWindow::Type request;
            request.commissioningTimeout = mCommissioningWindowTimeout.count();
            request.PAKEPasscodeVerifier = serializedVerifierSpan;
            request.discriminator        = mDiscriminator.GetLongValue();
            request.iterations           = mPBKDFIterations;
            request.salt                 = mPBKDFSalt;

            ReturnErrorOnFailure(cluster.InvokeCommand(request, this, OnOpenCommissioningWindowSuccess,
                                                       OnOpenCommissioningWindowFailure, MakeOptional(kTimedInvokeTimeoutMs)));
        }
#endif // CHIP_DEVICE_CONFIG_ENABLE_JOINT_FABRIC
    }
    else
    {
        AdministratorCommissioning::Commands::OpenBasicCommissioningWindow::Type request;
        request.commissioningTimeout = mCommissioningWindowTimeout.count();
        ReturnErrorOnFailure(cluster.InvokeCommand(request, this, OnOpenCommissioningWindowSuccess,
                                                   OnOpenCommissioningWindowFailure, MakeOptional(kTimedInvokeTimeoutMs)));
    }

    return CHIP_NO_ERROR;
}

void CommissioningWindowOpener::OnPIDReadResponse(void * context, uint16_t value)
{
    ChipLogProgress(Controller, "Received PID for the device. Value %d", value);
    auto * self                   = static_cast<CommissioningWindowOpener *>(context);
    self->mSetupPayload.productID = value;

    self->mNextStep = Step::kOpenCommissioningWindow;

    CHIP_ERROR err = self->mController->GetConnectedDevice(self->mNodeId, &self->mDeviceConnected, &self->mDeviceConnectionFailure);
    if (err != CHIP_NO_ERROR)
    {
        OnOpenCommissioningWindowFailure(context, err);
    }
}

void CommissioningWindowOpener::OnVIDReadResponse(void * context, VendorId value)
{
    ChipLogProgress(Controller, "Received VID for the device. Value %d", to_underlying(value));

    auto * self = static_cast<CommissioningWindowOpener *>(context);

    self->mSetupPayload.vendorID = value;

    self->mNextStep = Step::kReadPID;
    CHIP_ERROR err = self->mController->GetConnectedDevice(self->mNodeId, &self->mDeviceConnected, &self->mDeviceConnectionFailure);
    if (err != CHIP_NO_ERROR)
    {
        OnOpenCommissioningWindowFailure(context, err);
    }
}

void CommissioningWindowOpener::OnVIDPIDReadFailureResponse(void * context, CHIP_ERROR error)
{
    ChipLogProgress(Controller, "Failed to read VID/PID for the device. error %" CHIP_ERROR_FORMAT, error.Format());
    OnOpenCommissioningWindowFailure(context, error);
}

void CommissioningWindowOpener::OnOpenCommissioningWindowSuccess(void * context, const chip::app::DataModel::NullObjectType &)
{
    ChipLogProgress(Controller, "Successfully opened pairing window on the device");
    auto * self     = static_cast<CommissioningWindowOpener *>(context);
    self->mNextStep = Step::kAcceptCommissioningStart;
    if (self->mCommissioningWindowCallback != nullptr)
    {
        char payloadBuffer[QRCodeBasicSetupPayloadGenerator::kMaxQRCodeBase38RepresentationLength + 1];

        MutableCharSpan manualCode(payloadBuffer);
        CHIP_ERROR err = ManualSetupPayloadGenerator(self->mSetupPayload).payloadDecimalStringRepresentation(manualCode);
        if (err == CHIP_NO_ERROR)
        {
            ChipLogProgress(Controller, "Manual pairing code: [%s]", payloadBuffer);
        }
        else
        {
            ChipLogError(Controller, "Unable to generate manual code for setup payload: %" CHIP_ERROR_FORMAT, err.Format());
        }

        MutableCharSpan QRCode(payloadBuffer);
        err = QRCodeBasicSetupPayloadGenerator(self->mSetupPayload).payloadBase38Representation(QRCode);
        if (err == CHIP_NO_ERROR)
        {
            ChipLogProgress(Controller, "SetupQRCode: [%s]", payloadBuffer);
        }
        else
        {
            ChipLogError(Controller, "Unable to generate QR code for setup payload: %" CHIP_ERROR_FORMAT, err.Format());
        }

        self->mCommissioningWindowCallback->mCall(self->mCommissioningWindowCallback->mContext, self->mNodeId, CHIP_NO_ERROR,
                                                  self->mSetupPayload);
        // Don't touch `self` anymore; it might have been destroyed by the callee.
    }
    else if (self->mCommissioningWindowVerifierCallback != nullptr)
    {
        self->mCommissioningWindowVerifierCallback->mCall(self->mCommissioningWindowVerifierCallback->mContext, self->mNodeId,
                                                          CHIP_NO_ERROR);
        // Don't touch `self` anymore; it might have been destroyed by the callee.
    }
    else if (self->mBasicCommissioningWindowCallback != nullptr)
    {
        self->mBasicCommissioningWindowCallback->mCall(self->mBasicCommissioningWindowCallback->mContext, self->mNodeId,
                                                       CHIP_NO_ERROR);
        // Don't touch `self` anymore; it might have been destroyed by the callee.
    }
}

void CommissioningWindowOpener::OnOpenCommissioningWindowFailure(void * context, CHIP_ERROR error)
{
    ChipLogError(Controller, "Failed to open pairing window on the device. Status %" CHIP_ERROR_FORMAT, error.Format());
    auto * self     = static_cast<CommissioningWindowOpener *>(context);
    self->mNextStep = Step::kAcceptCommissioningStart;
    if (self->mCommissioningWindowCallback != nullptr)
    {
        self->mCommissioningWindowCallback->mCall(self->mCommissioningWindowCallback->mContext, self->mNodeId, error,
                                                  SetupPayload());
    }
    else if (self->mCommissioningWindowVerifierCallback != nullptr)
    {
        self->mCommissioningWindowVerifierCallback->mCall(self->mCommissioningWindowVerifierCallback->mContext, self->mNodeId,
                                                          error);
    }
    else if (self->mBasicCommissioningWindowCallback != nullptr)
    {
        self->mBasicCommissioningWindowCallback->mCall(self->mBasicCommissioningWindowCallback->mContext, self->mNodeId, error);
    }
}

void CommissioningWindowOpener::OnDeviceConnectedCallback(void * context, Messaging::ExchangeManager & exchangeMgr,
                                                          const SessionHandle & sessionHandle)
{
    auto * self = static_cast<CommissioningWindowOpener *>(context);

#if CHIP_ERROR_LOGGING
    const char * messageIfError = nullptr;
#endif // CHIP_ERROR_LOGGING
    CHIP_ERROR err = CHIP_NO_ERROR;

    switch (self->mNextStep)
    {
    case Step::kReadVID: {
        ClusterBase cluster(exchangeMgr, sessionHandle, kRootEndpointId);
        err = cluster.ReadAttribute<BasicInformation::Attributes::VendorID::TypeInfo>(context, OnVIDReadResponse,
                                                                                      OnVIDPIDReadFailureResponse);
#if CHIP_ERROR_LOGGING
        messageIfError = "Could not read VID for opening commissioning window";
#endif // CHIP_ERROR_LOGGING
        break;
    }
    case Step::kReadPID: {
        ClusterBase cluster(exchangeMgr, sessionHandle, kRootEndpointId);
        err = cluster.ReadAttribute<BasicInformation::Attributes::ProductID::TypeInfo>(context, OnPIDReadResponse,
                                                                                       OnVIDPIDReadFailureResponse);
#if CHIP_ERROR_LOGGING
        messageIfError = "Could not read PID for opening commissioning window";
#endif // CHIP_ERROR_LOGGING
        break;
    }
    case Step::kOpenCommissioningWindow: {
        err = self->OpenCommissioningWindowInternal(exchangeMgr, sessionHandle);
#if CHIP_ERROR_LOGGING
        messageIfError = "Could not connect to open commissioning window";
#endif // CHIP_ERROR_LOGGING
        break;
    }
    case Step::kAcceptCommissioningStart: {
        err = CHIP_ERROR_INCORRECT_STATE;
#if CHIP_ERROR_LOGGING
        messageIfError = "Just got a connected device; how can we be done?";
#endif // CHIP_ERROR_LOGGING
        break;
    }
    }

    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Controller, "%s: %" CHIP_ERROR_FORMAT, messageIfError, err.Format());
        OnOpenCommissioningWindowFailure(context, err);
    }
}

void CommissioningWindowOpener::OnDeviceConnectionFailureCallback(void * context, const ScopedNodeId & peerId, CHIP_ERROR error)
{
    OnOpenCommissioningWindowFailure(context, error);
}

AutoCommissioningWindowOpener::AutoCommissioningWindowOpener(DeviceController * controller) :
    CommissioningWindowOpener(controller), mOnOpenCommissioningWindowCallback(OnOpenCommissioningWindowResponse, this),
    mOnOpenBasicCommissioningWindowCallback(OnOpenBasicCommissioningWindowResponse, this)
{}

CHIP_ERROR AutoCommissioningWindowOpener::OpenBasicCommissioningWindow(DeviceController * controller, NodeId deviceId,
                                                                       Seconds16 timeout)
{
    // Not using Platform::New because we want to keep our constructor private.
    auto * opener = new (std::nothrow) AutoCommissioningWindowOpener(controller);
    if (opener == nullptr)
    {
        return CHIP_ERROR_NO_MEMORY;
    }

    CHIP_ERROR err = opener->CommissioningWindowOpener::OpenBasicCommissioningWindow(
        deviceId, timeout, &opener->mOnOpenBasicCommissioningWindowCallback);
    if (err != CHIP_NO_ERROR)
    {
        delete opener;
    }
    // Else will clean up when the callback is called.
    return err;
}

CHIP_ERROR AutoCommissioningWindowOpener::OpenCommissioningWindow(DeviceController * controller, NodeId deviceId, Seconds16 timeout,
                                                                  uint32_t iteration, uint16_t discriminator,
                                                                  Optional<uint32_t> setupPIN, Optional<ByteSpan> salt,
                                                                  SetupPayload & payload, bool readVIDPIDAttributes)
{
    // Not using Platform::New because we want to keep our constructor private.
    auto * opener = new (std::nothrow) AutoCommissioningWindowOpener(controller);
    if (opener == nullptr)
    {
        return CHIP_ERROR_NO_MEMORY;
    }

    CHIP_ERROR err = opener->CommissioningWindowOpener::OpenCommissioningWindow(
        deviceId, timeout, iteration, discriminator, setupPIN, salt, &opener->mOnOpenCommissioningWindowCallback, payload,
        readVIDPIDAttributes);
    if (err != CHIP_NO_ERROR)
    {
        delete opener;
    }
    // Else will clean up when the callback is called.
    return err;
}

void AutoCommissioningWindowOpener::OnOpenCommissioningWindowResponse(void * context, NodeId deviceId, CHIP_ERROR status,
                                                                      chip::SetupPayload payload)
{
    auto * self = static_cast<AutoCommissioningWindowOpener *>(context);
    delete self;
}
void AutoCommissioningWindowOpener::OnOpenBasicCommissioningWindowResponse(void * context, NodeId deviceId, CHIP_ERROR status)
{
    auto * self = static_cast<AutoCommissioningWindowOpener *>(context);
    delete self;
}

} // namespace Controller
} // namespace chip
