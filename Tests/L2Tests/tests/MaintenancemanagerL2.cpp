#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "L2Tests.h"
#include "L2TestsMock.h"
#include <fstream>
#include <iostream>
#include <condition_variable>
#include "MockNetworkPlugin.h"
//#include "MockNetworkPlugin.cpp"


using ::testing::_;
using ::testing::Invoke;
using ::testing::Test;
using ::testing::StrEq;
using namespace WPEFramework;

/*
#define TEST_LOG(x, ...) fprintf( stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

#define JSON_TIMEOUT   (1000)
*/
#define MAINTENANCEMANAGER_CALLSIGN  _T("org.rdk.MaintenanceManager")
#define MAINTENANCEMANAGERL2TEST_CALLSIGN _T("L2tests.1")



class MaintenanceManagerTest : public L2TestMocks {
protected:
    virtual ~MaintenanceManagerTest() override;

public:
    MaintenanceManagerTest();
};

MaintenanceManagerTest::MaintenanceManagerTest() : L2TestMocks() {
    IARM_EventHandler_t               controlEventHandler_;

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_RegisterEventHandler(StrEq(IARM_BUS_MAINTENANCE_MGR_NAME),IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE, _))
            .WillOnce(Invoke(
                [&](const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler) {
                    controlEventHandler_ = handler;
                    return IARM_RESULT_SUCCESS;
                }));
/*        EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_RegisterEventHandler(StrEq(IARM_BUS_MAINTENANCE_MGR_NAME), IARM_BUS_DCM_NEW_START_TIME_EVENT, _))
            .WillRepeatedly(Invoke(
                [&](const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler) {
                    EXPECT_TRUE(isValidCtrlmRcuIarmEvent(eventId));
                    controlEventHandler_ = handler;
                    return IARM_RESULT_SUCCESS;
                }));  */
    uint32_t status = Core::ERROR_GENERAL;
   // PluginHost::IShell::state state = PluginHost::IShell::state::ACTIVATED; //ghg
    //ActivateService("org.rdk.Network");
    //status = ActivateService("org.rdk.MockPlugin");
   // EXPECT_EQ(Core::ERROR_NONE, status);
    status = ActivateService("org.rdk.Network");
    EXPECT_EQ(Core::ERROR_NONE, status);
   /* status = ActivateService("org.rdk.Network");
    EXPECT_EQ(Core::ERROR_NONE, status);
    status = Core::ERROR_GENERAL;
    status = ActivateService("org.rdk.AuthService");
    EXPECT_EQ(Core::ERROR_NONE, status);
    status = Core::ERROR_GENERAL; */
    status = Core::ERROR_GENERAL;
    status = ActivateService("org.rdk.Network.1");
    status = ActivateService("org.rdk.MaintenanceManager");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

MaintenanceManagerTest::~MaintenanceManagerTest() {
    uint32_t status = Core::ERROR_GENERAL;
    status = DeactivateService("org.rdk.MaintenanceManager");
   // EXPECT_EQ(Core::ERROR_NONE, status);
}

//worked-yes

TEST_F(MaintenanceManagerTest, TestStartMaintenance)
{
    JsonObject params, params1;
    JsonObject results, results1;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE";
    uint32_t status = InvokeServiceMethod("org.rdk.MaintenanceManager", "setMaintenanceMode", params, results);

    params1["ipversion"] ="IPv4";

    ASSERT_EQ(status, Core::ERROR_NONE);
    ASSERT_EQ(results["success"].Boolean(), true);
    
    sleep(10);
    status = InvokeServiceMethod("org.rdk.Network.MaintenanceManager", "isConnectedToInternet", params1, results1);
    //status = DeactivateService("org.rdk.MaintenanceManager"); //jjj
  //  EXPECT_EQ(Core::ERROR_NONE, status);
}



TEST_F(MaintenanceManagerTest, TestStartMaintenance1)
{
    JsonObject params;
    JsonObject results;
    params["reason"] = "scheduled";
    uint32_t status = InvokeServiceMethod("org.rdk.MaintenanceManager", "startMaintenance", params, results);

    ASSERT_EQ(status, Core::ERROR_NONE);
    ASSERT_EQ(results["success"].Boolean(), true);
}




/* 
TEST_F(L2TestMocks, TestStartMaintenance)
{
    JsonObject params;
    JsonObject results;
    params["reason"] = "scheduled";
    uint32_t status = InvokeServiceMethod("MaintenanceManager.1", "startMaintenance", params, results);

    ASSERT_EQ(status, Core::ERROR_NONE);
    ASSERT_EQ(results["success"].Boolean(), true);
}
*/

/*
TEST_F(MaintenanceManagerTest, EmptyPartnerId) {
    JsonObject params, result;
    params["partnerId"] = "";
    uint32_t status = InvokeServiceMethod("org.rdk.MaintenanceManager", "isConnectedToInternet", params, result);
    EXPECT_NE(status, Core::ERROR_NONE); // Should fail or return error for empty partnerId
}
*/
/*
TEST_F(MaintenanceManagerTest, ValidDeviceInitializationContext) {

    JsonObject contextData, fullResponse, result;
    contextData["partnerId"] = "Sky";
    contextData["regionalConfigService"] = "region.sky.com";
    fullResponse["deviceInitializationContext"] = contextData;
    uint32_t status = InvokeServiceMethod("org.rdk.MaintenanceManager", "setDeviceInitializationContext", fullResponse, result);
    EXPECT_EQ(status, Core::ERROR_NONE); //hbbhkm
*/

