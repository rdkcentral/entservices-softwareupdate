#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "L2Tests.h"
#include "L2TestsMock.h"
//#include "../../MaintenanceManager/MaintenanceManager.h"
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
        
        //file << "start_hr=\"8\"\n";
        //file << "start_min=\"30\"\n";
        //file << "tz_mode=\"Asia/Kolkata\"\n"

        MaintenanceManagerConfFile << "start_hr=\"8\"\n";
        MaintenanceManagerConfFile << "start_min=\"30\"\n";
        MaintenanceManagerConfFile << "tz_mode=\"UTC\"\n"; 
        MaintenanceManagerConfFile.close();
        
    std::ifstream MaintenanceManagerConfFile("/opt/rdk_maintenance.conf");
    if (!MaintenanceManagerConfFile) {
        std::cerr << "Failed to open /opt/rdk_maintenance.conf for reading." << std::endl;
    }

    std::string line;
    while (std::getline(MaintenanceManagerConfFile, line)) {
        std::cout << line << std::endl;
    }
    }
    string getCurrentTestName()
    {
        const testing::TestInfo *const test_info = testing::UnitTest::GetInstance()->current_test_info();
        return test_info->name();
    }
    string test_name = getCurrentTestName();
    
    IARM_EventHandler_t               controlEventHandler_;
    uint32_t status = Core::ERROR_GENERAL;
    status = ActivateService("org.rdk.MaintenanceManager");
    EXPECT_EQ(Core::ERROR_NONE, status);
    if (test_name != "Test7")
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

TEST_F(MaintenanceManagerTest, TestStartMaintenance2)
{
    JsonObject  params1;
    JsonObject  results1;
    
    uint32_t status = InvokeServiceMethod("org.rdk.MaintenanceManager", "startMaintenance", params1, results1);
    ASSERT_EQ(status, Core::ERROR_NONE);
    ASSERT_EQ(results1["success"].Boolean(), true);
    InvokeServiceMethod("org.rdk.MaintenanceManager","stopMaintenance",params1, results1);
}
*/

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
/*
TEST_F(MaintenanceManagerTest,Test1)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params,params1;
    JsonObject results,results1;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE";
    sleep(60);
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","getMaintenanceActivityStatus",params1, results1);
    ASSERT_EQ(results1["maintenanceStatus"].String(), "MAINTENANCE_STARTED");
    ASSERT_EQ(status, Core::ERROR_NONE);
}

TEST_F(MaintenanceManagerTest,Test2)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params,params1;
    JsonObject results,results1;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE";
    status = InvokeServiceMethod("org.rdk.MaintenanceManager", "getMaintenanceStartTime", params, results);
    ASSERT_EQ(status, Core::ERROR_NONE);
}
TEST_F(MaintenanceManagerTest,Test3)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params,params1;
    JsonObject results,results1;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE";
    status = InvokeServiceMethod("org.rdk.MaintenanceManager", "setMaintenanceMode", params, results);
    ASSERT_EQ(status, Core::ERROR_NONE);
}

TEST_F(MaintenanceManagerTest,Test4)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params,params1;
    JsonObject results,results1;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE";
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","getMaintenanceActivityStatus",params1, results1);
    ASSERT_EQ(status, Core::ERROR_NONE);
}

TEST_F(MaintenanceManagerTest,Test5)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params,params1;
    JsonObject results,results1;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE";
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","stopMaintenance",params1, results1);
    ASSERT_EQ(results1["success"].Boolean(), true);
    ASSERT_EQ(status, Core::ERROR_NONE);
    sleep(5);
}

TEST_F(MaintenanceManagerTest,Test6)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params,params1;
    JsonObject results,results1;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE";
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","getMaintenanceActivityStatus",params1, results1);
    ASSERT_EQ(status, Core::ERROR_NONE);
}

TEST_F(MaintenanceManagerTest,Test7)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params,params1;
    JsonObject results,results1;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE";
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","stopMaintenance",params1, results1);
    ASSERT_EQ(results1["success"].Boolean(), false);
    ASSERT_EQ(status, Core::ERROR_GENERAL);
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

        sleep(60);
        status = InvokeServiceMethod("org.rdk.MaintenanceManager","getMaintenanceActivityStatus",params1, results1);
        ASSERT_EQ(results1["maintenanceStatus"].String(), "MAINTENANCE_STARTED");
        ASSERT_EQ(results1["isRebootPending"].Boolean(), false);
        ASSERT_EQ(results1["success"].Boolean(), true);
        
    
        ASSERT_EQ(status, Core::ERROR_NONE);
        status = InvokeServiceMethod("org.rdk.MaintenanceManager", "getMaintenanceStartTime", params, results);
        //ASSERT_EQ(results1["maintenanceStartTime"].String(), -1);
        ASSERT_EQ(status, Core::ERROR_NONE);
    
        status = InvokeServiceMethod("org.rdk.MaintenanceManager", "setMaintenanceMode", params, results);
        ASSERT_EQ(status, Core::ERROR_NONE);
    
        status = InvokeServiceMethod("org.rdk.MaintenanceManager","stopMaintenance",params1, results1);
        ASSERT_EQ(results1["success"].Boolean(), true);
        ASSERT_EQ(status, Core::ERROR_NONE);
    
        sleep(5);
        status = InvokeServiceMethod("org.rdk.MaintenanceManager","getMaintenanceActivityStatus",params1, results1);
        ASSERT_EQ(results1["maintenanceStatus"].String(), "MAINTENANCE_ERROR");
        ASSERT_EQ(results1["isRebootPending"].Boolean(), false);
        ASSERT_EQ(status, Core::ERROR_NONE);
        ASSERT_EQ(results1["success"].Boolean(), true);
    
        status = InvokeServiceMethod("org.rdk.MaintenanceManager","stopMaintenance",params1, results1);
        ASSERT_EQ(results1["success"].Boolean(), false);
        ASSERT_EQ(status, Core::ERROR_GENERAL);
   // }
}
TEST_F(MaintenanceManagerTest, TestStartMaintenance)
{
       DeactivateService("org.rdk.Network");  
}
/*
TEST_F(MaintenanceManagerTest, TestStartMaintenance)
{
    JsonObject  params1;
    JsonObject  results1;
    
    sleep(30);
    uint32_t status = InvokeServiceMethod("org.rdk.MaintenanceManager", "startMaintenance", params1, results1);
    ASSERT_EQ(status, Core::ERROR_NONE);
    ASSERT_EQ(results1["success"].Boolean(), false);
    
}
*/

/*
TEST_F(MaintenanceManagerTest, TestStartMaintenance1)
{
    JsonObject  params1;
    JsonObject  results1;
    //WPEFramework::Plugin::MaintenanceManager::g_unsolicited_complete = true;
    DeactivateService("org.rdk.MaintenanceManager");
    
    sleep(200);
    ActivateService("org.rdk.MaintenanceManager");
    uint32_t status = InvokeServiceMethod("org.rdk.MaintenanceManager", "startMaintenance", params1, results1);
    ASSERT_EQ(status, Core::ERROR_NONE);
    ASSERT_EQ(results1["success"].Boolean(), true);
    DeactivateService("org.rdk.MaintenanceManager");
    
}
*/

TEST_F(MaintenanceManagerTest,Test1)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params,params1;
    JsonObject results,results1;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE";
    sleep(60);
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","getMaintenanceActivityStatus",params1, results1);
    ASSERT_EQ(results1["maintenanceStatus"].String(), "MAINTENANCE_STARTED");
    ASSERT_EQ(status, Core::ERROR_NONE);
}

TEST_F(MaintenanceManagerTest,Test2)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params,params1;
    JsonObject results,results1;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE";
    status = InvokeServiceMethod("org.rdk.MaintenanceManager", "getMaintenanceStartTime", params, results);
    ASSERT_EQ(status, Core::ERROR_NONE);
}
TEST_F(MaintenanceManagerTest,Test3)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params,params1;
    JsonObject results,results1;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE";
    status = InvokeServiceMethod("org.rdk.MaintenanceManager", "setMaintenanceMode", params, results);
    ASSERT_EQ(status, Core::ERROR_NONE);
}

TEST_F(MaintenanceManagerTest,Test4)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params,params1;
    JsonObject results,results1;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE";
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","stopMaintenance",params1, results1);
    ASSERT_EQ(results1["success"].Boolean(), true);
    ASSERT_EQ(status, Core::ERROR_NONE);
}

TEST_F(MaintenanceManagerTest, Test5)
{
    JsonObject  params1;
    JsonObject  results1;
    
    uint32_t status = InvokeServiceMethod("org.rdk.MaintenanceManager", "startMaintenance", params1, results1);
    ASSERT_EQ(status, Core::ERROR_GENERAL);
    ASSERT_EQ(results1["success"].Boolean(), false);
    //InvokeServiceMethod("org.rdk.MaintenanceManager","stopMaintenance",params1, results1);
}
TEST_F(MaintenanceManagerTest,Test6)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params,params1;
    JsonObject results,results1;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE";
    sleep(10);
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","stopMaintenance",params1, results1);
    ASSERT_EQ(results1["success"].Boolean(), true);
    ASSERT_EQ(status, Core::ERROR_NONE);
    sleep(5);
    status = InvokeServiceMethod("org.rdk.MaintenanceManager", "startMaintenance", params1, results1);
    ASSERT_EQ(status, Core::ERROR_NONE);
    //InvokeServiceMethod("org.rdk.MaintenanceManager", "getMaintenanceStartTime", params, results);
    
}

TEST_F(MaintenanceManagerTest,Test7)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params,params1;
    JsonObject results,results1;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE"; 
    sleep(10);
    DeactivateService("org.rdk.Network");
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","stopMaintenance",params1, results1);
    ASSERT_EQ(results1["success"].Boolean(), true);
    ASSERT_EQ(status, Core::ERROR_NONE);
    sleep(10);
    //InvokeServiceMethod("org.rdk.MaintenanceManager", "getMaintenanceStartTime", params, results);
    const char* filepath = "/etc/device.properties";
    std::ofstream file(filepath, std::ofstream::out | std::ofstream::trunc);
    if (file.is_open()) {
        std::cout << "File content cleared successfully.\n";
        file.close();
    } else {
        std::cerr << "Failed to open file for clearing.\n";
    }

}

