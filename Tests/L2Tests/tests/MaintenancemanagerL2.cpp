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

TEST_F(MaintenanceManagerTest,knowWhoamI)
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
        
    ASSERT_EQ(status, Core::ERROR_NONE);
    status = InvokeServiceMethod("org.rdk.MaintenanceManager", "getMaintenanceStartTime", params, results);
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
}
TEST_F(MaintenanceManagerTest, TestStartMaintenance)
{
       DeactivateService("org.rdk.Network");  
}

TEST_F(MaintenanceManagerTest,Test1)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params1;
    JsonObject results1;
    sleep(60);
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","getMaintenanceActivityStatus",params1, results1);
    ASSERT_EQ(results1["maintenanceStatus"].String(), "MAINTENANCE_STARTED");
    ASSERT_EQ(status, Core::ERROR_NONE);
}

TEST_F(MaintenanceManagerTest,Test2)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params1;
    JsonObject results1;
    status = InvokeServiceMethod("org.rdk.MaintenanceManager", "getMaintenanceStartTime", params1, results1);
    ASSERT_EQ(status, Core::ERROR_NONE);
}
TEST_F(MaintenanceManagerTest,Test3)
{
    uint32_t status = Core::ERROR_GENERAL;
     JsonObject params;
    JsonObject results;
    params["maintenanceMode"] = "BACKGROUND";
    params["optOut"] = "IGNORE_UPDATE";
    status = InvokeServiceMethod("org.rdk.MaintenanceManager", "setMaintenanceMode", params, results);
    ASSERT_EQ(status, Core::ERROR_NONE);
}

TEST_F(MaintenanceManagerTest,Test4)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params1, results1;
    status = InvokeServiceMethod("org.rdk.MaintenanceManager","stopMaintenance",params1, results1);
    ASSERT_EQ(results1["success"].Boolean(), true);
    ASSERT_EQ(status, Core::ERROR_NONE);
}

TEST_F(MaintenanceManagerTest, Test5)
{
    JsonObject  params1, results1;
    uint32_t status = InvokeServiceMethod("org.rdk.MaintenanceManager", "startMaintenance", params1, results1);
    ASSERT_EQ(status, Core::ERROR_GENERAL);
    ASSERT_EQ(results1["success"].Boolean(), false);
}
TEST_F(MaintenanceManagerTest, Test6)
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
    ASSERT_EQ(results1["isRebootPending"].Boolean(), false);
    ASSERT_EQ(status, Core::ERROR_NONE);
    ASSERT_EQ(results1["success"].Boolean(), true);
}
