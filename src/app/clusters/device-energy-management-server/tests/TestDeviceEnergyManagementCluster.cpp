/*
 *
 *    Copyright (c) 2025 Project CHIP Authors
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

#include "DeviceEnergyManagementMockDelegate.h"
#include <app/clusters/device-energy-management-server/DeviceEnergyManagementCluster.h>
#include <app/clusters/device-energy-management-server/tests/DeviceEnergyManagementMockDelegate.h>
#include <pw_unit_test/framework.h>

#include <app/clusters/testing/ClusterTester.h>
#include <app/clusters/testing/ValidateGlobalAttributes.h>
#include <app/data-model-provider/tests/ReadTesting.h>
#include <app/server-cluster/testing/TestServerClusterContext.h>
#include <clusters/DeviceEnergyManagement/Attributes.h>
#include <clusters/DeviceEnergyManagement/Metadata.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::DeviceEnergyManagement;
using namespace chip::app::Clusters::DeviceEnergyManagement::Attributes;
using namespace chip::Test;
using namespace chip::Testing;

namespace {

constexpr EndpointId kTestEndpointId = 1;

struct TestDeviceEnergyManagementCluster : public ::testing::Test
{
    static void SetUpTestSuite() { ASSERT_EQ(chip::Platform::MemoryInit(), CHIP_NO_ERROR); }
    static void TearDownTestSuite() { chip::Platform::MemoryShutdown(); }

    void SetUp() override {}
};

TEST_F(TestDeviceEnergyManagementCluster, TestFeatures)
{
    chip::Test::TestServerClusterContext context;
    DeviceEnergyManagementMockDelegate mockDelegate;

    // Test 1: No features - should only have mandatory attributes and no commands
    {
        BitMask<Feature> noFeatures;

        DeviceEnergyManagementCluster cluster(DeviceEnergyManagementCluster::Config(kTestEndpointId, noFeatures, &mockDelegate));

        EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);

        // Verify only mandatory attributes are present
        EXPECT_TRUE(IsAttributesListEqualTo(cluster,
                                            {
                                                ESAType::kMetadataEntry,
                                                ESACanGenerate::kMetadataEntry,
                                                ESAState::kMetadataEntry,
                                                AbsMinPower::kMetadataEntry,
                                                AbsMaxPower::kMetadataEntry,
                                            }));

        // Verify no commands are available
        ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> commandsBuilder;
        EXPECT_EQ(cluster.AcceptedCommands(ConcreteClusterPath(kTestEndpointId, DeviceEnergyManagement::Id), commandsBuilder),
                  CHIP_NO_ERROR);
        EXPECT_EQ(commandsBuilder.TakeBuffer().size(), 0u);

        cluster.Shutdown();
    }

    // Test 2: PowerAdjustment feature - should add PowerAdjustmentCapability attribute and PA commands
    {
        BitMask<Feature> features(Feature::kPowerAdjustment);

        DeviceEnergyManagementCluster cluster(DeviceEnergyManagementCluster::Config(kTestEndpointId, features, &mockDelegate));

        EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);

        // Verify PowerAdjustmentCapability and OptOutState attributes are present
        EXPECT_TRUE(IsAttributesListEqualTo(cluster,
                                            {
                                                ESAType::kMetadataEntry,
                                                ESACanGenerate::kMetadataEntry,
                                                ESAState::kMetadataEntry,
                                                AbsMinPower::kMetadataEntry,
                                                AbsMaxPower::kMetadataEntry,
                                                PowerAdjustmentCapability::kMetadataEntry,
                                                OptOutState::kMetadataEntry,
                                            }));

        cluster.Shutdown();
    }

    // Test 3: Forecast features - should add Forecast attribute
    {
        BitMask<Feature> features(Feature::kPowerForecastReporting);

        DeviceEnergyManagementCluster cluster(DeviceEnergyManagementCluster::Config(kTestEndpointId, features, &mockDelegate));

        EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);

        // Verify Forecast attribute is present
        EXPECT_TRUE(IsAttributesListEqualTo(cluster,
                                            {
                                                ESAType::kMetadataEntry,
                                                ESACanGenerate::kMetadataEntry,
                                                ESAState::kMetadataEntry,
                                                AbsMinPower::kMetadataEntry,
                                                AbsMaxPower::kMetadataEntry,
                                                Forecast::kMetadataEntry,
                                            }));

        cluster.Shutdown();
    }

    // Test 4: All features enabled
    {
        BitMask<Feature> allFeatures(Feature::kPowerAdjustment, Feature::kPowerForecastReporting, Feature::kStateForecastReporting,
                                     Feature::kStartTimeAdjustment, Feature::kPausable, Feature::kForecastAdjustment,
                                     Feature::kConstraintBasedAdjustment);

        DeviceEnergyManagementCluster cluster(DeviceEnergyManagementCluster::Config(kTestEndpointId, allFeatures, &mockDelegate));

        EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);

        // Verify all optional attributes are present
        EXPECT_TRUE(IsAttributesListEqualTo(cluster,
                                            {
                                                ESAType::kMetadataEntry,
                                                ESACanGenerate::kMetadataEntry,
                                                ESAState::kMetadataEntry,
                                                AbsMinPower::kMetadataEntry,
                                                AbsMaxPower::kMetadataEntry,
                                                PowerAdjustmentCapability::kMetadataEntry,
                                                Forecast::kMetadataEntry,
                                                OptOutState::kMetadataEntry,
                                            }));

        cluster.Shutdown();
    }
}

TEST_F(TestDeviceEnergyManagementCluster, TestAttributes)
{
    chip::Test::TestServerClusterContext context;
    DeviceEnergyManagementMockDelegate mockDelegate;

    // Test 1: Read mandatory attributes with no features
    {
        BitMask<Feature> noFeatures;

        DeviceEnergyManagementCluster cluster(DeviceEnergyManagementCluster::Config(kTestEndpointId, noFeatures, &mockDelegate));

        EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);

        chip::Test::ClusterTester tester(cluster);

        // Read mandatory attributes
        ESATypeEnum esaType;
        ASSERT_EQ(tester.ReadAttribute(ESAType::Id, esaType), CHIP_NO_ERROR);
        EXPECT_EQ(esaType, ESATypeEnum::kEvse);

        bool esaCanGenerate;
        ASSERT_EQ(tester.ReadAttribute(ESACanGenerate::Id, esaCanGenerate), CHIP_NO_ERROR);
        EXPECT_EQ(esaCanGenerate, false);

        ESAStateEnum esaState;
        ASSERT_EQ(tester.ReadAttribute(ESAState::Id, esaState), CHIP_NO_ERROR);
        EXPECT_EQ(esaState, ESAStateEnum::kOnline);

        int64_t absMinPower;
        ASSERT_EQ(tester.ReadAttribute(AbsMinPower::Id, absMinPower), CHIP_NO_ERROR);
        EXPECT_EQ(absMinPower, DeviceEnergyManagementMockDelegate::kAbsMinPower);

        int64_t absMaxPower;
        ASSERT_EQ(tester.ReadAttribute(AbsMaxPower::Id, absMaxPower), CHIP_NO_ERROR);
        EXPECT_EQ(absMaxPower, DeviceEnergyManagementMockDelegate::kAbsMaxPower);

        cluster.Shutdown();
    }

    // Test 2: Read optional attributes with PowerAdjustment feature
    {
        BitMask<Feature> features(Feature::kPowerAdjustment);

        DeviceEnergyManagementCluster cluster(DeviceEnergyManagementCluster::Config(kTestEndpointId, features, &mockDelegate));

        EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);

        Span<const ConcreteClusterPath> paths = cluster.GetPaths();
        ASSERT_EQ(paths.size(), 1u);

        // Test PowerAdjustmentCapability (requires kPowerAdjustment)
        {
            const ConcreteDataAttributePath path = { paths[0].mEndpointId, paths[0].mClusterId, PowerAdjustmentCapability::Id };
            chip::app::Testing::ReadOperation readOperation(path);
            std::unique_ptr<AttributeValueEncoder> encoder = readOperation.StartEncoding();
            auto result                                    = cluster.ReadAttribute(readOperation.GetRequest(), *encoder);
            EXPECT_TRUE(result.IsSuccess());
        }

        // Test OptOutState (requires PA, STA, PAU, FA, or CON)
        {
            const ConcreteDataAttributePath path = { paths[0].mEndpointId, paths[0].mClusterId, OptOutState::Id };
            chip::app::Testing::ReadOperation readOperation(path);
            std::unique_ptr<AttributeValueEncoder> encoder = readOperation.StartEncoding();
            auto result                                    = cluster.ReadAttribute(readOperation.GetRequest(), *encoder);
            EXPECT_TRUE(result.IsSuccess());
        }

        cluster.Shutdown();
    }

    // Test 3: Read optional attributes with Forecast features
    {
        BitMask<Feature> features(Feature::kPowerForecastReporting);

        DeviceEnergyManagementCluster cluster(DeviceEnergyManagementCluster::Config(kTestEndpointId, features, &mockDelegate));

        EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);

        Span<const ConcreteClusterPath> paths = cluster.GetPaths();
        ASSERT_EQ(paths.size(), 1u);

        // Test Forecast (requires PFR or SFR)
        {
            const ConcreteDataAttributePath path = { paths[0].mEndpointId, paths[0].mClusterId, Forecast::Id };
            chip::app::Testing::ReadOperation readOperation(path);
            std::unique_ptr<AttributeValueEncoder> encoder = readOperation.StartEncoding();
            auto result                                    = cluster.ReadAttribute(readOperation.GetRequest(), *encoder);
            EXPECT_TRUE(result.IsSuccess());
        }

        cluster.Shutdown();
    }
}

TEST_F(TestDeviceEnergyManagementCluster, TestCommands)
{
    chip::Test::TestServerClusterContext context;
    DeviceEnergyManagementMockDelegate mockDelegate;

    // Test 1: Verify AcceptedCommands list is empty when no features are enabled
    {
        BitMask<Feature> noFeatures;

        DeviceEnergyManagementCluster cluster(DeviceEnergyManagementCluster::Config(kTestEndpointId, noFeatures, &mockDelegate));

        EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);

        // Verify no commands are in AcceptedCommands
        ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> commandsBuilder;
        EXPECT_EQ(cluster.AcceptedCommands(ConcreteClusterPath(kTestEndpointId, DeviceEnergyManagement::Id), commandsBuilder),
                  CHIP_NO_ERROR);
        auto commandsBuffer = commandsBuilder.TakeBuffer();
        EXPECT_EQ(commandsBuffer.size(), 0u);

        cluster.Shutdown();
    }

    // Test 2: Verify PowerAdjustment commands are in AcceptedCommands with kPowerAdjustment feature
    {
        BitMask<Feature> features(Feature::kPowerAdjustment);

        DeviceEnergyManagementCluster cluster(DeviceEnergyManagementCluster::Config(kTestEndpointId, features, &mockDelegate));

        EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);

        // Verify AcceptedCommands includes PowerAdjustment commands
        ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> commandsBuilder;
        EXPECT_EQ(cluster.AcceptedCommands(ConcreteClusterPath(kTestEndpointId, DeviceEnergyManagement::Id), commandsBuilder),
                  CHIP_NO_ERROR);
        auto commandsBuffer = commandsBuilder.TakeBuffer();
        EXPECT_EQ(commandsBuffer.size(), 2u);

        using namespace Commands;
        bool foundPowerAdjust       = false;
        bool foundCancelPowerAdjust = false;
        for (const auto & entry : commandsBuffer)
        {
            if (entry.commandId == PowerAdjustRequest::Id)
            {
                foundPowerAdjust = true;
            }
            if (entry.commandId == CancelPowerAdjustRequest::Id)
            {
                foundCancelPowerAdjust = true;
            }
        }
        EXPECT_TRUE(foundPowerAdjust);
        EXPECT_TRUE(foundCancelPowerAdjust);

        // PowerAdjustRequest with feature - should succeed
        {
            PowerAdjustRequest::Type command;
            command.power    = 1000;
            command.duration = 60;
            command.cause    = AdjustmentCauseEnum::kLocalOptimization;

            chip::Test::ClusterTester tester(cluster);
            auto result = tester.Invoke(PowerAdjustRequest::Id, command);
            EXPECT_TRUE(result.IsSuccess());
        }

        // CancelPowerAdjustRequest with feature - should succeed
        {
            CancelPowerAdjustRequest::Type command;

            chip::Test::ClusterTester tester(cluster);
            auto result = tester.Invoke(CancelPowerAdjustRequest::Id, command);
            EXPECT_TRUE(result.IsSuccess());
        }

        cluster.Shutdown();
    }

    // Test 3: Verify StartTimeAdjustment command is in AcceptedCommands with kStartTimeAdjustment feature
    {
        BitMask<Feature> features(Feature::kStartTimeAdjustment);

        DeviceEnergyManagementCluster cluster(DeviceEnergyManagementCluster::Config(kTestEndpointId, features, &mockDelegate));

        EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);

        // Verify AcceptedCommands includes StartTimeAdjustRequest and CancelRequest
        ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> commandsBuilder;
        EXPECT_EQ(cluster.AcceptedCommands(ConcreteClusterPath(kTestEndpointId, DeviceEnergyManagement::Id), commandsBuilder),
                  CHIP_NO_ERROR);
        auto commandsBuffer = commandsBuilder.TakeBuffer();
        EXPECT_EQ(commandsBuffer.size(), 2u);

        using namespace Commands;
        bool foundStartTimeAdjust = false;
        bool foundCancel          = false;
        for (const auto & entry : commandsBuffer)
        {
            if (entry.commandId == StartTimeAdjustRequest::Id)
            {
                foundStartTimeAdjust = true;
            }
            if (entry.commandId == CancelRequest::Id)
            {
                foundCancel = true;
            }
        }
        EXPECT_TRUE(foundStartTimeAdjust);
        EXPECT_TRUE(foundCancel);

        // StartTimeAdjustRequest with feature - should succeed
        {
            StartTimeAdjustRequest::Type command;
            command.requestedStartTime = 1000;
            command.cause              = AdjustmentCauseEnum::kLocalOptimization;

            chip::Test::ClusterTester tester(cluster);
            auto result = tester.Invoke(StartTimeAdjustRequest::Id, command);
            EXPECT_TRUE(result.IsSuccess());
        }

        cluster.Shutdown();
    }

    // Test 4: Verify Pausable commands are in AcceptedCommands with kPausable feature
    {
        BitMask<Feature> features(Feature::kPausable);

        DeviceEnergyManagementCluster cluster(DeviceEnergyManagementCluster::Config(kTestEndpointId, features, &mockDelegate));

        EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);

        // Verify AcceptedCommands includes Pausable commands
        ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> commandsBuilder;
        EXPECT_EQ(cluster.AcceptedCommands(ConcreteClusterPath(kTestEndpointId, DeviceEnergyManagement::Id), commandsBuilder),
                  CHIP_NO_ERROR);
        auto commandsBuffer = commandsBuilder.TakeBuffer();
        EXPECT_EQ(commandsBuffer.size(), 2u);

        using namespace Commands;
        bool foundPause  = false;
        bool foundResume = false;
        for (const auto & entry : commandsBuffer)
        {
            if (entry.commandId == PauseRequest::Id)
            {
                foundPause = true;
            }
            if (entry.commandId == ResumeRequest::Id)
            {
                foundResume = true;
            }
        }
        EXPECT_TRUE(foundPause);
        EXPECT_TRUE(foundResume);

        // PauseRequest with feature - should succeed
        {
            PauseRequest::Type command;
            command.duration = 60;
            command.cause    = AdjustmentCauseEnum::kLocalOptimization;

            chip::Test::ClusterTester tester(cluster);
            auto result = tester.Invoke(PauseRequest::Id, command);
            EXPECT_TRUE(result.IsSuccess());
        }

        // ResumeRequest with feature - should succeed
        {
            ResumeRequest::Type command;

            chip::Test::ClusterTester tester(cluster);
            auto result = tester.Invoke(ResumeRequest::Id, command);
            EXPECT_TRUE(result.IsSuccess());
        }

        cluster.Shutdown();
    }

    // Test 5: Verify ForecastAdjustment command is in AcceptedCommands with kForecastAdjustment feature
    {
        BitMask<Feature> features(Feature::kForecastAdjustment);

        DeviceEnergyManagementCluster cluster(DeviceEnergyManagementCluster::Config(kTestEndpointId, features, &mockDelegate));

        EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);

        // Verify AcceptedCommands includes ModifyForecastRequest and CancelRequest
        ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> commandsBuilder;
        EXPECT_EQ(cluster.AcceptedCommands(ConcreteClusterPath(kTestEndpointId, DeviceEnergyManagement::Id), commandsBuilder),
                  CHIP_NO_ERROR);
        auto commandsBuffer = commandsBuilder.TakeBuffer();
        EXPECT_EQ(commandsBuffer.size(), 2u);

        using namespace Commands;
        bool foundModifyForecast = false;
        bool foundCancel         = false;
        for (const auto & entry : commandsBuffer)
        {
            if (entry.commandId == ModifyForecastRequest::Id)
            {
                foundModifyForecast = true;
            }
            if (entry.commandId == CancelRequest::Id)
            {
                foundCancel = true;
            }
        }
        EXPECT_TRUE(foundModifyForecast);
        EXPECT_TRUE(foundCancel);

        // ModifyForecastRequest with feature - should succeed
        {
            ModifyForecastRequest::Type command;
            command.forecastID = mockDelegate.GetForecast().Value().forecastID;
            command.cause      = AdjustmentCauseEnum::kLocalOptimization;

            chip::Test::ClusterTester tester(cluster);
            auto result = tester.Invoke(ModifyForecastRequest::Id, command);
            EXPECT_TRUE(result.IsSuccess());
        }

        cluster.Shutdown();
    }

    // Test 6: Verify ConstraintBasedAdjustment command is in AcceptedCommands with kConstraintBasedAdjustment feature
    {
        BitMask<Feature> features(Feature::kConstraintBasedAdjustment);

        DeviceEnergyManagementCluster cluster(DeviceEnergyManagementCluster::Config(kTestEndpointId, features, &mockDelegate));

        EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);

        // Verify AcceptedCommands includes RequestConstraintBasedForecast and CancelRequest
        ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> commandsBuilder;
        EXPECT_EQ(cluster.AcceptedCommands(ConcreteClusterPath(kTestEndpointId, DeviceEnergyManagement::Id), commandsBuilder),
                  CHIP_NO_ERROR);
        auto commandsBuffer = commandsBuilder.TakeBuffer();
        EXPECT_EQ(commandsBuffer.size(), 2u);

        using namespace Commands;
        bool foundRequestConstraint = false;
        bool foundCancel            = false;
        for (const auto & entry : commandsBuffer)
        {
            if (entry.commandId == RequestConstraintBasedForecast::Id)
            {
                foundRequestConstraint = true;
            }
            if (entry.commandId == CancelRequest::Id)
            {
                foundCancel = true;
            }
        }
        EXPECT_TRUE(foundRequestConstraint);
        EXPECT_TRUE(foundCancel);

        // RequestConstraintBasedForecast with feature - should succeed
        {
            RequestConstraintBasedForecast::Type command;
            command.cause = AdjustmentCauseEnum::kLocalOptimization;

            chip::Test::ClusterTester tester(cluster);
            auto result = tester.Invoke(RequestConstraintBasedForecast::Id, command);
            EXPECT_TRUE(result.IsSuccess());
        }

        cluster.Shutdown();
    }

    // Test 7: Verify CancelRequest is in AcceptedCommands when any of STA, FA, or CON features are enabled
    {
        // Test with StartTimeAdjustment feature
        {
            BitMask<Feature> features(Feature::kStartTimeAdjustment);

            DeviceEnergyManagementCluster cluster(DeviceEnergyManagementCluster::Config(kTestEndpointId, features, &mockDelegate));

            EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);

            // Verify AcceptedCommands includes CancelRequest
            ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> commandsBuilder;
            EXPECT_EQ(cluster.AcceptedCommands(ConcreteClusterPath(kTestEndpointId, DeviceEnergyManagement::Id), commandsBuilder),
                      CHIP_NO_ERROR);
            auto commandsBuffer = commandsBuilder.TakeBuffer();

            using namespace Commands;
            bool foundCancel = false;
            for (const auto & entry : commandsBuffer)
            {
                if (entry.commandId == CancelRequest::Id)
                {
                    foundCancel = true;
                    break;
                }
            }
            EXPECT_TRUE(foundCancel);

            // CancelRequest with STA feature - should succeed when ForecastUpdateReason is not kInternalOptimization
            // Use StartTimeAdjustRequest to set ForecastUpdateReason to kLocalOptimization first
            {
                StartTimeAdjustRequest::Type startTimeCommand;
                startTimeCommand.requestedStartTime = 1000;
                startTimeCommand.cause              = AdjustmentCauseEnum::kLocalOptimization;

                chip::Test::ClusterTester tester(cluster);
                auto startTimeResult = tester.Invoke(StartTimeAdjustRequest::Id, startTimeCommand);
                EXPECT_TRUE(startTimeResult.IsSuccess());
            }

            // Now test CancelRequest - should succeed
            {
                CancelRequest::Type command;

                chip::Test::ClusterTester tester(cluster);
                auto result = tester.Invoke(CancelRequest::Id, command);
                EXPECT_TRUE(result.IsSuccess());

                // Verify that CancelRequest updated ForecastUpdateReason to kInternalOptimization
                EXPECT_EQ(mockDelegate.GetForecast().Value().forecastUpdateReason, ForecastUpdateReasonEnum::kInternalOptimization);
            }

            // Test that CancelRequest fails when ForecastUpdateReason is already kInternalOptimization
            // (After the previous CancelRequest, ForecastUpdateReason is now kInternalOptimization)
            {
                CancelRequest::Type command;

                chip::Test::ClusterTester tester(cluster);
                auto result = tester.Invoke(CancelRequest::Id, command);
                EXPECT_FALSE(result.IsSuccess());
                ASSERT_TRUE(result.status.has_value());
                EXPECT_EQ(result.status.value().GetStatusCode().GetStatus(), Protocols::InteractionModel::Status::InvalidInState);
            }

            cluster.Shutdown();
        }

        // Test with ForecastAdjustment feature
        {
            BitMask<Feature> features(Feature::kForecastAdjustment);

            DeviceEnergyManagementCluster cluster(DeviceEnergyManagementCluster::Config(kTestEndpointId, features, &mockDelegate));

            EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);

            // Verify AcceptedCommands includes CancelRequest
            ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> commandsBuilder;
            EXPECT_EQ(cluster.AcceptedCommands(ConcreteClusterPath(kTestEndpointId, DeviceEnergyManagement::Id), commandsBuilder),
                      CHIP_NO_ERROR);
            auto commandsBuffer = commandsBuilder.TakeBuffer();

            using namespace Commands;
            bool foundCancel = false;
            for (const auto & entry : commandsBuffer)
            {
                if (entry.commandId == CancelRequest::Id)
                {
                    foundCancel = true;
                    break;
                }
            }
            EXPECT_TRUE(foundCancel);

            cluster.Shutdown();
        }

        // Test with ConstraintBasedAdjustment feature
        {
            BitMask<Feature> features(Feature::kConstraintBasedAdjustment);

            DeviceEnergyManagementCluster cluster(DeviceEnergyManagementCluster::Config(kTestEndpointId, features, &mockDelegate));

            EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);

            // Verify AcceptedCommands includes CancelRequest
            ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> commandsBuilder;
            EXPECT_EQ(cluster.AcceptedCommands(ConcreteClusterPath(kTestEndpointId, DeviceEnergyManagement::Id), commandsBuilder),
                      CHIP_NO_ERROR);
            auto commandsBuffer = commandsBuilder.TakeBuffer();

            using namespace Commands;
            bool foundCancel = false;
            for (const auto & entry : commandsBuffer)
            {
                if (entry.commandId == CancelRequest::Id)
                {
                    foundCancel = true;
                    break;
                }
            }
            EXPECT_TRUE(foundCancel);

            cluster.Shutdown();
        }
    }
}

} // namespace
