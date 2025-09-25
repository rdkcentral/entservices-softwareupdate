#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "L2Tests.h"
#include "L2TestsMock.h"
#include <fstream>
#include <iostream>
#include <condition_variable>

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

    std::ofstream devicePropertiesFile("/etc/device.properties");
    if (devicePropertiesFile.is_open()) {

        devicePropertiesFile << "WHOAMI_SUPPORT=true";
        devicePropertiesFile.close();
        
    std::ifstream devicePropertiesFile("/etc/device.properties");
    if (!devicePropertiesFile) {
        std::cerr << "Failed to open /etc/device.properties for reading." << std::endl;
    }

    std::string line;
    while (std::getline(devicePropertiesFile, line)) {
        std::cout << line << std::endl;
    }
    }

    std::ofstream MaintenanceManagerConfFile("/opt/rdk_maintenance.conf");
    
        if (MaintenanceManagerConfFile.is_open()) {

        MaintenanceManagerConfFile << "start_hr=2";
        MaintenanceManagerConfFile << "start_min=39";
        MaintenanceManagerConfFile << "tz_mode=UTC";
        MaintenanceManagerConfFile.close();
        
    std::ifstream MaintenanceManagerConfFile("/etc/device.properties");
    if (!MaintenanceManagerConfFile) {
        std::cerr << "Failed to open /opt/rdk_maintenance.conf for reading." << std::endl;
    }

    std::string line;
    while (std::getline(MaintenanceManagerConfFile, line)) {
        std::cout << line << std::endl;
    }
    }

    
    IARM_EventHandler_t               controlEventHandler_;
    uint32_t status = Core::ERROR_GENERAL;
    status = ActivateService("org.rdk.MaintenanceManager");
    EXPECT_EQ(Core::ERROR_NONE, status);
    status =ActivateService("org.rdk.Network");
    EXPECT_EQ(Core::ERROR_NONE, status);
    status =ActivateService("org.rdk.SecManager");
    EXPECT_EQ(Core::ERROR_NONE, status);
    status =ActivateService("org.rdk.AuthService");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

MaintenanceManagerTest::~MaintenanceManagerTest() {
    uint32_t status = Core::ERROR_GENERAL;
    status = DeactivateService("org.rdk.MaintenanceManager");
}
/*
TEST_F(MaintenanceManagerTest, TestStartMaintenance)
{
    JsonObject params, params1;
    JsonObject results, results1;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE";
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent)
            .Times(::testing::AnyNumber())
            .WillRepeatedly(
                    [](const char* ownerName, int eventId, void* arg, size_t argLen) {
                    return IARM_RESULT_SUCCESS;
                    });
    sleep(30);
    uint32_t status = InvokeServiceMethod("org.rdk.MaintenanceManager", "setMaintenanceMode", params, results);
    params1["ipversion"] ="IPv4";
    ASSERT_EQ(status, Core::ERROR_NONE);
    ASSERT_EQ(results["success"].Boolean(), true);
    DeactivateService("org.rdk.MaintenanceManager");
    
}
*/
TEST_F(MaintenanceManagerTest,knowWhoamI)
{
    
    //std::ofstream devicePropertiesFile("/etc/device.properties");
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params,params1;
    JsonObject results,results1;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE";
/*
    if (devicePropertiesFile.is_open()) {

        devicePropertiesFile << "WHOAMI_SUPPORT=true";
        devicePropertiesFile.close();
        
    std::ifstream devicePropertiesFile("/etc/device.properties");
    if (!devicePropertiesFile) {
        std::cerr << "Failed to open /etc/device.properties for reading." << std::endl;
    }

    std::string line;
    while (std::getline(devicePropertiesFile, line)) {
        std::cout << line << std::endl;
    }
    */
        sleep(60);
        //uint32_t status = InvokeServiceMethod("org.rdk.MaintenanceManager", "setMaintenanceMode", params, results);
        status = InvokeServiceMethod("org.rdk.MaintenanceManager", "getMaintenanceStartTime", params, results);
        InvokeServiceMethod("org.rdk.MaintenanceManager", "setMaintenanceMode", params, results);
        InvokeServiceMethod("org.rdk.MaintenanceManager","getMaintenanceActivityStatus",params1, results1);
        InvokeServiceMethod("org.rdk.MaintenanceManager","stopMaintenance",params1, results1);
    
        sleep(5);
        EXPECT_EQ(Core::ERROR_NONE, status);
   // }
}
TEST_F(MaintenanceManagerTest, TestStartMaintenance)
{
    JsonObject  params1;
    JsonObject  results1;
    
    sleep(30);
    uint32_t status = InvokeServiceMethod("org.rdk.MaintenanceManager", "startMaintenance", params1, results1);
    ASSERT_EQ(status, Core::ERROR_NONE);
    ASSERT_EQ(results1["success"].Boolean(), false);
    
}
TEST_F(MaintenanceManagerTest, TestStartMaintenance1)
{
    JsonObject  params1;
    JsonObject  results1;
    
    sleep(200);
    uint32_t status = InvokeServiceMethod("org.rdk.MaintenanceManager", "startMaintenance", params1, results1);
    ASSERT_EQ(status, Core::ERROR_NONE);
    ASSERT_EQ(results1["success"].Boolean(), true);
    
}


