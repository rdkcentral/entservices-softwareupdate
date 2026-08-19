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
#define SERVICE_TRANSITION_RETRY_COUNT 20
#define SERVICE_TRANSITION_RETRY_SLEEP_SEC 2



class MaintenanceManagerTest : public L2TestMocks {
protected:
    virtual ~MaintenanceManagerTest() override;

public:
    MaintenanceManagerTest();

private:
    bool EnsureServiceActive(const char* callsign);
    void BestEffortDeactivate(const char* callsign);
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
    EXPECT_TRUE(EnsureServiceActive("org.rdk.MaintenanceManager"));
    EXPECT_TRUE(EnsureServiceActive("org.rdk.Network"));
    EXPECT_TRUE(EnsureServiceActive("org.rdk.SecManager"));
    EXPECT_TRUE(EnsureServiceActive("org.rdk.AuthService"));
}

MaintenanceManagerTest::~MaintenanceManagerTest() {
    JsonObject params, results;
    // Best-effort stop; ignore errors (maintenance may not have been started)
    try {
        InvokeServiceMethod("org.rdk.MaintenanceManager", "stopMaintenance", params, results);
    } catch (...) {
        // Silently ignore stop failures during cleanup
    }

    sleep(6);
    BestEffortDeactivate("org.rdk.AuthService");
    BestEffortDeactivate("org.rdk.SecManager");
    BestEffortDeactivate("org.rdk.Network");
    BestEffortDeactivate("org.rdk.MaintenanceManager");
}

bool MaintenanceManagerTest::EnsureServiceActive(const char* callsign) {
    for (int attempt = 0; attempt < SERVICE_TRANSITION_RETRY_COUNT; ++attempt) {
        uint32_t status = ActivateService(callsign);
        if (status == Core::ERROR_NONE) {
            return true;
        }

        // Let controller/plugin state transitions settle before retrying.
        sleep(SERVICE_TRANSITION_RETRY_SLEEP_SEC);
    }

    return false;
}

void MaintenanceManagerTest::BestEffortDeactivate(const char* callsign) {
    for (int attempt = 0; attempt < SERVICE_TRANSITION_RETRY_COUNT; ++attempt) {
        uint32_t status = DeactivateService(callsign);
        if (status == Core::ERROR_NONE) {
            return;
        }
        sleep(SERVICE_TRANSITION_RETRY_SLEEP_SEC);
    }
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
// TEST_F(MaintenanceManagerTest,stopMaintenance)
// {
//     uint32_t status = Core::ERROR_GENERAL;
//     JsonObject params1, results1;
//     status = InvokeServiceMethod("org.rdk.MaintenanceManager","stopMaintenance",params1, results1);
//     ASSERT_EQ(results1["success"].Boolean(), true);
//     ASSERT_EQ(status, Core::ERROR_NONE);
// }

//startMaintenance jsonPRC Test on an active Maintenance Cycle
TEST_F(MaintenanceManagerTest, startMaintenance_active_maintenance)
{
    JsonObject  params1, results1;

    // Wait for the bootup (unsolicited) maintenance to reach STARTED state.
    // The task thread must pass isDeviceOnline (~30s network retry) and WhoAmI before
    // emitting MAINTENANCE_STARTED. Poll so the test is not sensitive to exact timing.
    for (int i = 0; i < 25; ++i) {
        uint32_t s = InvokeServiceMethod("org.rdk.MaintenanceManager", "getMaintenanceActivityStatus", params1, results1);
        if (s == Core::ERROR_NONE && results1["maintenanceStatus"].String() == "MAINTENANCE_STARTED") break;
        sleep(2);
    }

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

    // Wait for the bootup (unsolicited) maintenance to reach STARTED state before stopping it.
    // The task thread needs ~30s (network retry sleep) + WhoAmI time before emitting STARTED.
    for (int i = 0; i < 25; ++i) {
        uint32_t s = InvokeServiceMethod("org.rdk.MaintenanceManager", "getMaintenanceActivityStatus", params1, results1);
        if (s == Core::ERROR_NONE && results1["maintenanceStatus"].String() == "MAINTENANCE_STARTED") break;
        sleep(2);
    }
    sleep(5);

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


