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

TEST_F(MaintenanceManagerTest,Unsolicited_Maintenance)
{
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

    // getMaintenanceStartTime - does not return Start Time
    ASSERT_EQ(status, Core::ERROR_NONE);
    status = InvokeServiceMethod("org.rdk.MaintenanceManager", "getMaintenanceStartTime", params, results);
    ASSERT_EQ(status, Core::ERROR_NONE);
    
    status = InvokeServiceMethod("org.rdk.MaintenanceManager", "setMaintenanceMode", params, results);
    ASSERT_EQ(status, Core::ERROR_NONE);

    //stopMaintenance on an active Maintenance Cycle
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","stopMaintenance",params1, results1);
    ASSERT_EQ(results1["success"].Boolean(), true);
    ASSERT_EQ(status, Core::ERROR_NONE);
    
    sleep(5);
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","getMaintenanceActivityStatus",params1, results1);
    ASSERT_EQ(results1["maintenanceStatus"].String(), "MAINTENANCE_ERROR");
    ASSERT_EQ(results1["isRebootPending"].Boolean(), false);
    ASSERT_EQ(status, Core::ERROR_NONE);
    ASSERT_EQ(results1["success"].Boolean(), true);

    // stopMaintenance on an inactive Maintenance
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","stopMaintenance",params1, results1);
    ASSERT_EQ(results1["success"].Boolean(), false);
    ASSERT_EQ(status, Core::ERROR_GENERAL);
}
// getMaintenanceStartTime jsonRPC returns Start Time
TEST_F(MaintenanceManagerTest, getMaintenanceStartTime_json_rpc)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params1, results1;
    
    std::ofstream MaintenanceManagerConfFile("/opt/rdk_maintenance.conf");
    
    if (MaintenanceManagerConfFile.is_open()) {
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
    status = InvokeServiceMethod("org.rdk.MaintenanceManager", "getMaintenanceStartTime", params1, results1);
    ASSERT_EQ(results1["success"].Boolean(), true);
    ASSERT_EQ(status, Core::ERROR_NONE);
    }

}
//getMaintenanceActivityStatus jsonRPC
TEST_F(MaintenanceManagerTest,getMaintenanceActivityStatus)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params1;
    JsonObject results1;
    sleep(60);
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","getMaintenanceActivityStatus",params1, results1);
    ASSERT_EQ(results1["maintenanceStatus"].String(), "MAINTENANCE_STARTED");
    ASSERT_EQ(status, Core::ERROR_NONE);
}

//setMaintenanceMode json RPC
TEST_F(MaintenanceManagerTest,setMaintenanceMode_json_rpc)
{
    uint32_t status = Core::ERROR_GENERAL;
     JsonObject params;
    JsonObject results;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE";
    status = InvokeServiceMethod("org.rdk.MaintenanceManager", "setMaintenanceMode", params, results);
    ASSERT_EQ(status, Core::ERROR_NONE);
}

//stopMaintenance on an active Maintenance Cycle
TEST_F(MaintenanceManagerTest, stopMaintenanceWhenNotStarted)
{
    // Test Case 1: stopMaintenance when maintenance is NOT started
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(MAINTENANCEMANAGER_CALLSIGN, MAINTENANCEMANAGER_CALLSIGN);
    JsonObject results;
    JsonObject params;

    uint32_t status = jsonrpc.Invoke<JsonObject, JsonObject>(1000, _T("stopMaintenance"), params, results);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_FALSE(results["success"].Boolean());  // ← Should return false (not started)
}

TEST_F(MaintenanceManagerTest, stopMaintenanceWhenStarted)
{
    // Test Case 2: stopMaintenance when maintenance IS started
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(MAINTENANCEMANAGER_CALLSIGN, MAINTENANCEMANAGER_CALLSIGN);
    JsonObject params;
    JsonObject results;

    // Start maintenance first
    JsonObject startResults;
    uint32_t status = jsonrpc.Invoke<JsonObject, JsonObject>(1000, _T("startMaintenance"), params, startResults);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(startResults["success"].Boolean());

    // Wait for maintenance to actually start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Now stop maintenance
    status = jsonrpc.Invoke<JsonObject, JsonObject>(1000, _T("stopMaintenance"), params, results);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(results["success"].Boolean());  // ← Should now pass
}

//startMaintenance jsonPRC Test on an active Maintenance Cycle
TEST_F(MaintenanceManagerTest, startMaintenance_active_maintenance)
{
    JsonObject  params1, results1;
    uint32_t status = InvokeServiceMethod("org.rdk.MaintenanceManager", "startMaintenance", params1, results1);
    ASSERT_EQ(status, Core::ERROR_GENERAL);
    ASSERT_EQ(results1["success"].Boolean(), false);
    
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","getMaintenanceActivityStatus",params1, results1);
    ASSERT_EQ(results1["maintenanceStatus"].String(), "MAINTENANCE_STARTED");
    ASSERT_EQ(results1["isRebootPending"].Boolean(), false);
    ASSERT_EQ(status, Core::ERROR_NONE);
    ASSERT_EQ(results1["success"].Boolean(), true);
}
TEST_F(MaintenanceManagerTest, Solicited_maintenance)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params1, results1;
    sleep(20);
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","stopMaintenance",params1, results1);
    ASSERT_EQ(results1["success"].Boolean(), true);
    ASSERT_EQ(status, Core::ERROR_NONE);
    sleep(5);
    status = InvokeServiceMethod("org.rdk.MaintenanceManager", "startMaintenance", params1, results1);
    ASSERT_EQ(status, Core::ERROR_NONE);  

    status = InvokeServiceMethod("org.rdk.MaintenanceManager","stopMaintenance",params1, results1);
    ASSERT_EQ(results1["success"].Boolean(), true);
    ASSERT_EQ(status, Core::ERROR_NONE);
    
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","getMaintenanceActivityStatus",params1, results1);
    ASSERT_EQ(results1["maintenanceStatus"].String(), "MAINTENANCE_ERROR");
    ASSERT_EQ(results1["isRebootPending"].Boolean(), true);
    ASSERT_EQ(status, Core::ERROR_NONE);
    ASSERT_EQ(results1["success"].Boolean(), true);
}

TEST_F(MaintenanceManagerTest, getMaintenanceMode_json_rpc)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params1, results1;
    status = InvokeServiceMethod("org.rdk.MaintenanceManager", "getMaintenanceMode", params1, results1);
    ASSERT_EQ(status, Core::ERROR_NONE);
}


