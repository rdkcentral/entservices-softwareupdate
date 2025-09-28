/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2025 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "L2Tests.h"
#include "L2TestsMock.h"
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <interfaces/IFirmwareUpdate.h>
#include "HdmiCec.h"

#define TEST_LOG(x, ...) fprintf( stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

#define JSON_TIMEOUT   (1000)
#define FIRMWAREUPDATE_CALLSIGN  _T("org.rdk.FirmwareUpdate")
#define FIRMWAREUPDATEL2TEST_CALLSIGN _T("L2tests.1")

using ::testing::NiceMock;
using namespace WPEFramework;
using testing::StrictMock;
using ::WPEFramework::Exchange::IFirmwareUpdate;


class AsyncHandlerMock
{
    public:
        AsyncHandlerMock()
        {
        }
};

class FirmwareUpdateTest : public L2TestMocks {
protected:
    virtual ~FirmwareUpdateTest() override;

    public:
    FirmwareUpdateTest();

};

FirmwareUpdateTest:: FirmwareUpdateTest():L2TestMocks()
{
        Core::JSONRPC::Message message;
        string response;
        uint32_t status = Core::ERROR_GENERAL;

         /* Activate plugin in constructor */
         status = ActivateService("org.rdk.FirmwareUpdate");
         EXPECT_EQ(Core::ERROR_NONE, status);
         status = Core::ERROR_GENERAL;
         status = ActivateService("org.rdk.NetworkManager.1");
         EXPECT_EQ(Core::ERROR_NONE, status);
         status = Core::ERROR_GENERAL;
         status = ActivateService("org.rdk.Network");
         EXPECT_EQ(Core::ERROR_NONE, status);
         status = ActivateService("org.rdk.Network.1");
         EXPECT_EQ(Core::ERROR_NONE, status);
         status = ActivateService("org.rdk.Network.2");
         EXPECT_EQ(Core::ERROR_NONE, status);


    JsonObject params, params1;
    JsonObject results, results1;
    params1["ipversion"] ="IPv4"; 
    status = InvokeServiceMethod("org.rdk.Network.1", "isConnectedToInternet", params1, results1);
    InvokeServiceMethod("org.rdk.Network", "isConnectedToInternet", params1, results1);
    InvokeServiceMethod("org.rdk.Network.2", "isConnectedToInternet", params1, results1);
}

/**
 * @brief Destructor for SystemServices L2 test class
 */
FirmwareUpdateTest::~FirmwareUpdateTest()
{
    uint32_t status = Core::ERROR_GENERAL;

    status = DeactivateService("org.rdk.FirmwareUpdate");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

TEST_F(FirmwareUpdateTest,EmptyFirmwareFilepath)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    //Case1 Empty firmwareFilepath
    params["firmwareFilepath"] = "";
    params["firmwareType"]     = "PCI";
    status = InvokeServiceMethod("org.rdk.FirmwareUpdate", "updateFirmware", params, result);

    EXPECT_NE(status,Core::ERROR_NONE);
}

TEST_F(FirmwareUpdateTest,EmptyFirmwareType)
{
    uint32_t status = Core::ERROR_GENERAL;
    
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
    std::ofstream file(filePath, std::ios::binary); 

    if (file.is_open()) {
        JsonObject params;
        JsonObject result;
        params["firmwareFilepath"] = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
        params["firmwareType"]     = "";
        status = InvokeServiceMethod("org.rdk.FirmwareUpdate", "updateFirmware", params, result);
        EXPECT_NE(status,Core::ERROR_NONE);
    }
}

TEST_F(FirmwareUpdateTest,InvalidFirmwareType)
{
    uint32_t status = Core::ERROR_GENERAL;
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
    std::ofstream file(filePath, std::ios::binary); 

    if (file.is_open()) {
        JsonObject params;
        JsonObject result;
        params["firmwareFilepath"] = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
        params["firmwareType"]     = "ABC";
        status = InvokeServiceMethod("org.rdk.FirmwareUpdate", "updateFirmware", params, result);
        EXPECT_NE(status,Core::ERROR_NONE);

        JsonObject params1;
        JsonObject result1;
        status = InvokeServiceMethod("org.rdk.FirmwareUpdate", "getUpdateState", params1, result1);
        EXPECT_EQ(Core::ERROR_NONE, status);
    }
}

TEST_F(FirmwareUpdateTest,FirmwareFilepath_not_exist)
{
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

   //Case4 given firmwareFilepath not exist
    params["firmwareFilepath"] = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
    params["firmwareType"]     = "PCI";
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";

    if (std::remove(filePath) == 0) {
        std::cout << "File removed successfully.\n";
        status = InvokeServiceMethod("org.rdk.FirmwareUpdate", "updateFirmware", params, result);
        EXPECT_NE(status,Core::ERROR_NONE);
    }    
}

TEST_F(FirmwareUpdateTest,FirmwareUptoDateValidatation)
{
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
    std::ofstream versionFile("/version.txt");
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    // Create and open the file
    std::ofstream file(filePath, std::ios::binary); // Open in binary mode (if needed)

    if (file.is_open() && versionFile.is_open()) {
        
        versionFile << "imagename:ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614";
        versionFile.close();
        params["firmwareFilepath"] = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
        params["firmwareType"]     = "PCI";
        status = InvokeServiceMethod("org.rdk.FirmwareUpdate", "updateFirmware", params, result);
        EXPECT_NE(status,Core::ERROR_NONE);
    }
}

TEST_F(FirmwareUpdateTest,FirmwareUpdate_without_imageFlasher)
{
    std::ofstream file("/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin");
    std::ofstream versionFile("/version.txt");
    std::ofstream devicePropertiesFile("/etc/device.properties");
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    if (file.is_open() && versionFile.is_open() && devicePropertiesFile.is_open()) {

        file << "imagename:ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614";
        file.close();

        versionFile << "imagename:ELTE11MWR_MIDDLEWARE_DEV_default_2024112214545";
        versionFile.close();

        devicePropertiesFile << "DEVICE_TYPE=mediaclient";
        devicePropertiesFile << "CPU_ARCH=ARM";
        devicePropertiesFile << "DIFW_PATH=/opt/CDL";

        params["firmwareFilepath"] = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
        params["firmwareType"]     = "PCI";

        EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent)
            .Times(::testing::AnyNumber())
            .WillRepeatedly(
                    [](const char* ownerName, int eventId, void* arg, size_t argLen) {
                    return IARM_RESULT_SUCCESS;
                    });

        status = InvokeServiceMethod("org.rdk.FirmwareUpdate", "updateFirmware", params, result);
        sleep(5);
        EXPECT_EQ(Core::ERROR_NONE, status);
    }
}

TEST_F(FirmwareUpdateTest,FirmwareUpdate_with_imageFlasher)
{
    std::ofstream file("/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin");
    std::ofstream versionFile("/version.txt");
    std::ofstream devicePropertiesFile("/etc/device.properties");
    std::ofstream imageFlasher("/lib/rdk/imageFlasher.sh");

    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    if (file.is_open() && versionFile.is_open() && devicePropertiesFile.is_open() && imageFlasher.is_open()  ) {

        file << "imagename:ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614";
        file.close();

        versionFile << "imagename:ELTE11MWR_MIDDLEWARE_DEV_default_2024112214545";
        versionFile.close();

        devicePropertiesFile << "DEVICE_TYPE=mediaclient";
        devicePropertiesFile << "DEVICE_NAME=EXAMPLE";
        devicePropertiesFile << "CPU_ARCH=ARM";
        devicePropertiesFile << "DIFW_PATH=/opt/CDL";
        devicePropertiesFile.close();

        imageFlasher <<"ret 0" ;
        imageFlasher.close();
        
        params["firmwareFilepath"] = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
        params["firmwareType"]     = "PCI";

        EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent)
            .Times(::testing::AnyNumber())
            .WillRepeatedly(
                    [](const char* ownerName, int eventId, void* arg, size_t argLen) {
                    return IARM_RESULT_SUCCESS;
                    });

        EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
            .Times(::testing::AnyNumber())
            .WillRepeatedly(::testing::Return(Core::ERROR_NONE));
        
        status = InvokeServiceMethod("org.rdk.FirmwareUpdate", "updateFirmware", params, result);
        sleep(10);
    }
}
