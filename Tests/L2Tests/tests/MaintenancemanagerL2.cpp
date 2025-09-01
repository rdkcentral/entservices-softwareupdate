#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "L2Tests.h"
#include "L2TestsMock.h"
#include <fstream>
#include <iostream>


using namespace WPEFramework;

class MaintenanceManagerTest : public L2TestMocks {
protected:
    virtual ~MaintenanceManagerTest() override;

public:
    MaintenanceManagerTest();
};

MaintenanceManagerTest::MaintenanceManagerTest() : L2TestMocks() {
    uint32_t status = Core::ERROR_GENERAL;
    status = ActivateService("org.rdk.MaintenanceManager");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

MaintenanceManagerTest::~MaintenanceManagerTest() {
    uint32_t status = Core::ERROR_GENERAL;
    status = DeactivateService("org.rdk.MaintenanceManager");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

// Example test cases

TEST_F(MaintenanceManagerTest, EmptyPartnerId) {
    JsonObject params, result;
    params["partnerId"] = "";
    uint32_t status = InvokeServiceMethod("org.rdk.MaintenanceManager", "setPartnerId", params, result);
    EXPECT_NE(status, Core::ERROR_NONE); // Should fail or return error for empty partnerId
}

TEST_F(MaintenanceManagerTest, ValidDeviceInitializationContext) {
    JsonObject contextData, fullResponse, result;
    contextData["partnerId"] = "Sky";
    contextData["regionalConfigService"] = "region.sky.com";
    fullResponse["deviceInitializationContext"] = contextData;
    uint32_t status = InvokeServiceMethod("org.rdk.MaintenanceManager", "setDeviceInitializationContext", fullResponse, result);
    EXPECT_EQ(status, Core::ERROR_NONE); // Should succeed for valid input
}

// Add more test cases as needed to cover all service methods and edge cases.
