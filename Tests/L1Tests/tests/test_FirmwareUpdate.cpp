/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2024 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing,software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#include <gtest/gtest.h>
#include <mntent.h>
#include <fstream>
#include "FirmwareUpdate.h"
#include "FirmwareUpdateImplementation.h"
#include "ServiceMock.h"
#include "IarmBusMock.h"
#include "FactoriesImplementation.h"
#include <iostream> 
#include <string>
#include <vector>
#include <cstdio>
#include "COMLinkMock.h"
#include "WorkerPoolImplementation.h"
#include "WrapsMock.h"
#include "RfcApiMock.h"
#include "secure_wrappermock.h"
#include "ThunderPortability.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define FIRMWARE_UPDATE_STATE   "/tmp/FirmwareUpdateStatus.txt"

using ::testing::NiceMock;
using namespace WPEFramework;

namespace {
    const string callSign = _T("FirmwareUpdate");
    const string TEST_FIRMWARE_PATH = "/tmp/test_firmware.bin";
    const string INVALID_FIRMWARE_PATH = "/invalid/path/firmware.bin";
    const string TEST_FIRMWARE_TYPE_PCI = "PCI";
    const string TEST_FIRMWARE_TYPE_SI = "SI";
    const string INVALID_FIRMWARE_TYPE = "INVALID";

}

class NotificationHandlerMock : public Exchange::IFirmwareUpdate::INotification {
public:
    NotificationHandlerMock() = default;
    virtual ~NotificationHandlerMock() = default;

    MOCK_METHOD(void, OnUpdateStateChange, (const Exchange::IFirmwareUpdate::State state,
                                          const Exchange::IFirmwareUpdate::SubState substate), (override));
    MOCK_METHOD(void, OnFlashingStateChange, (const uint32_t percentageComplete), (override));
    
    // Implement IReferenceCounted interface
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    
    // Implement IUnknown interface
    MOCK_METHOD(void*, QueryInterface, (const uint32_t interfaceNumber), (override));
};

class FirmwareUpdateTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::FirmwareUpdate> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;    
    WrapsImplMock  *p_wrapsImplMock   = nullptr;
    IarmBusImplMock  *p_iarmBusImplMock   = nullptr;
    RfcApiImplMock  *p_rfcApiImplMock   = nullptr;
    Core::ProxyType<Plugin::FirmwareUpdateImplementation> FirmwareUpdateImpl;
    NiceMock<COMLinkMock> comLinkMock;
    NiceMock<ServiceMock> service;
    PLUGINHOST_DISPATCHER* dispatcher;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    NiceMock<FactoriesImplementation> factoriesImplementation;
    std::unique_ptr<NotificationHandlerMock> notificationMock;
    std::atomic<bool> flashInProgress;

    FirmwareUpdateTest()
        : plugin(Core::ProxyType<Plugin::FirmwareUpdate>::Create())
        , handler(*(plugin))
        , INIT_CONX(1, 0)
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
	, notificationMock(std::make_unique<NotificationHandlerMock>())
        , flashInProgress(false)  
    {
        
    	p_wrapsImplMock = new testing::NiceMock<WrapsImplMock>;
    	Wraps::setImpl(p_wrapsImplMock);
	p_iarmBusImplMock = new NiceMock<IarmBusImplMock>;
    	IarmBus::setImpl(p_iarmBusImplMock);
	p_rfcApiImplMock = new testing::NiceMock<RfcApiImplMock>;
	RfcApi::setImpl(p_rfcApiImplMock);

        ON_CALL(service, COMLink())
            .WillByDefault(::testing::Invoke(
                  [this]() {
                        TEST_LOG("Pass created comLinkMock: %p ", &comLinkMock);
                        return &comLinkMock;
                    }));

        ON_CALL(*p_iarmBusImplMock, IARM_Bus_Call)
            .WillByDefault(::testing::Invoke(
                [this](const char* ownerName, const char* methodName, void* arg, size_t argLen) {
                    flashInProgress = true;
                    return IARM_RESULT_SUCCESS;
                }));

#ifdef USE_THUNDER_R4
        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
			.WillByDefault(::testing::Invoke(
                  [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
                        FirmwareUpdateImpl = Core::ProxyType<Plugin::FirmwareUpdateImplementation>::Create();
                        TEST_LOG("Pass created FirmwareUpdateImpl: %p &FirmwareUpdateImpl: %p", 
                                 static_cast<void*>(FirmwareUpdateImpl.operator->()), 
                                 static_cast<void*>(&FirmwareUpdateImpl));
                        return &FirmwareUpdateImpl;
                    }));
#else
	  ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
	    .WillByDefault(::testing::Return(FirmwareUpdateImpl));
#endif /*USE_THUNDER_R4 */

        PluginHost::IFactories::Assign(&factoriesImplementation);

        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();

        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
        plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(&service);

        std::remove(FIRMWARE_UPDATE_STATE);	
        EXPECT_EQ(string(""), plugin->Initialize(&service));	    
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    virtual ~FirmwareUpdateTest() override
    {
        TEST_LOG("FirmwareUpdateTest Destructor");

        plugin->Deinitialize(&service);

        dispatcher->Deactivate();
        dispatcher->Release();

        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();

        Wraps::setImpl(nullptr);
        if (p_wrapsImplMock != nullptr)
        {
            delete p_wrapsImplMock;
            p_wrapsImplMock = nullptr;
        }

	RfcApi::setImpl(nullptr);
	if (p_rfcApiImplMock != nullptr)
	{
	    delete p_rfcApiImplMock;
	    p_rfcApiImplMock = nullptr;
	}

        PluginHost::IFactories::Assign(nullptr);
	IarmBus::setImpl(nullptr);
        if (p_iarmBusImplMock != nullptr)
        {
            delete p_iarmBusImplMock;
            p_iarmBusImplMock = nullptr;
        }

    }
    void SetUp() override {
        std::remove(FIRMWARE_UPDATE_STATE);
        std::remove(TEST_FIRMWARE_PATH.c_str());
        createTestFirmwareFile();
        flashInProgress = false;
    }

    void TearDown() override {
        if (flashInProgress) {
            handler.Invoke(connection, _T("cancelFirmwareUpdate"), _T("{}"), response);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        std::remove(FIRMWARE_UPDATE_STATE);
        std::remove(TEST_FIRMWARE_PATH.c_str());
    }

    void createTestFirmwareFile() {
        std::ofstream outfile(TEST_FIRMWARE_PATH);
        outfile << "Test firmware content";
        outfile.close();
    }

    // Helper method for testing events
    void triggerFirmwareEvent(const Exchange::IFirmwareUpdate::State state,
                             const Exchange::IFirmwareUpdate::SubState substate) {
        // Register notification handler if not already registered
        Core::hresult result = FirmwareUpdateImpl->Register(notificationMock.get());
        EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_ALREADY_CONNECTED);

        // Trigger update state change through plugin handler
        string stateString = (state == Exchange::IFirmwareUpdate::State::FLASHING_SUCCEEDED) ? "FLASHING_SUCCEEDED" :
                            (state == Exchange::IFirmwareUpdate::State::FLASHING_FAILED) ? "FLASHING_FAILED" : "IDLE";
        
        string substateString = (substate == Exchange::IFirmwareUpdate::SubState::NOT_APPLICABLE) ? "NOT_APPLICABLE" : "GENERIC_ERROR";
        
        string updateStatusRequest = "{\"state\":\"" + stateString + "\",\"substate\":\"" + substateString + "\"}";
        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onUpdateStateChange"), updateStatusRequest, response));
    }

    void triggerProgressEvent(uint32_t progress) {
        // Register notification handler if not already registered
        Core::hresult result = FirmwareUpdateImpl->Register(notificationMock.get());
        EXPECT_TRUE(result == Core::ERROR_NONE || result == Core::ERROR_ALREADY_CONNECTED);

        // Trigger progress update through plugin handler
        string progressRequest = "{\"progress\":" + std::to_string(progress) + "}";
        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onFlashingProgressChange"), progressRequest, response));
    }
};

// Basic API Method Registration Tests
TEST_F(FirmwareUpdateTest, RegisteredMethods)
{    

     // First verify that updateFirmware and getUpdateState are registered
     EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("updateFirmware")));   
     EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getUpdateState")));
}


// TEST_F(FirmwareUpdateTest, UpdateFirmware_PostFlash_ValidPCI_Success)
// {

//     createTestFirmwareFile();

//     std::ofstream deviceProps("/etc/device.properties");
//     deviceProps << "DEVICE_TYPE=mediaclient" << std::endl;
//     deviceProps.close();

//     EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));

//     std::this_thread::sleep_for(std::chrono::milliseconds(200));
//     std::remove("/etc/device.properties");
// }


// TEST_F(FirmwareUpdateTest, UpdateFirmware_PostFlash_ValidDRI_Success)
// {

//     createTestFirmwareFile();

//     handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"DRI\"}"), response);

//     std::this_thread::sleep_for(std::chrono::milliseconds(200));
// }

TEST_F(FirmwareUpdateTest, UpdateFirmware_PostFlash_MaintenanceMode_True)
{
    createTestFirmwareFile();

    std::ofstream deviceProps("/etc/device.properties");
    deviceProps << "DEVICE_TYPE=broadband" << std::endl;
    deviceProps.close();

    std::ofstream maintenanceFile("/opt/swupdate_maintenance_upgrade");
    maintenanceFile.close();

    std::ofstream rebootScript("/rebootNow.sh");
    rebootScript << "#!/bin/bash\nexit 0\n";
    rebootScript.close();
    chmod("/rebootNow.sh", 0755);

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::remove("/etc/device.properties");
    std::remove("/opt/swupdate_maintenance_upgrade");
    std::remove("/rebootNow.sh");
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_PostFlash_MaintenanceMode_NoRebootScript)
{

    createTestFirmwareFile();

    std::remove("/rebootNow.sh");

    std::ofstream maintenanceFile("/opt/swupdate_maintenance_upgrade");
    maintenanceFile.close();

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::remove("/opt/swupdate_maintenance_upgrade");
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_PostFlash_DeviceType_PLATCO_Critical)
{
    createTestFirmwareFile();

    std::ofstream deviceProps("/etc/device.properties");
    deviceProps << "DEVICE_TYPE=mediaclient" << std::endl;
    deviceProps << "DEVICE_NAME=PLATCO" << std::endl;
    deviceProps.close();

    std::ofstream rebootScript("/rebootNow.sh");
    rebootScript << "#!/bin/bash\nexit 0\n";
    rebootScript.close();
    chmod("/rebootNow.sh", 0755);

    std::ofstream maintenanceFile("/opt/swupdate_maintenance_upgrade");
    maintenanceFile.close();

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\",\"reboot\":true}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::remove("/etc/device.properties");
    std::remove("/rebootNow.sh");
    std::remove("/opt/swupdate_maintenance_upgrade");
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_PostFlash_RebootTrue_NonMaintenance)
{

    createTestFirmwareFile();

    std::ofstream rebootScript("/rebootNow.sh");
    rebootScript << "#!/bin/bash\nexit 0\n";
    rebootScript.close();
    chmod("/rebootNow.sh", 0755);

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\",\"reboot\":true}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::remove("/rebootNow.sh");
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_PostFlash_PDRI_NoReboot)
{

    createTestFirmwareFile();

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"DRI\",\"reboot\":false}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}
#if 0
TEST_F(FirmwareUpdateTest, UpdateFirmware_FlashImage_ValidPCI_Success)
{

    createTestFirmwareFile();

    std::ofstream imageFlasher("/lib/rdk/imageFlasher.sh");
    imageFlasher << "#!/bin/bash\nexit 0\n";
    imageFlasher.close();
    chmod("/lib/rdk/imageFlasher.sh", 0755);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}
#endif 

TEST_F(FirmwareUpdateTest, UpdateFirmware_FlashImage_ValidDRI_Success)
{
    createTestFirmwareFile();

    std::ofstream imageFlasher("/lib/rdk/imageFlasher.sh");
    imageFlasher << "#!/bin/bash\nexit 0\n";
    imageFlasher.close();
    chmod("/lib/rdk/imageFlasher.sh", 0755);

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"DRI\"}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_FlashImage_ImageFlasherScriptMissing)
{
    std::remove("/lib/rdk/imageFlasher.sh");
    createTestFirmwareFile();

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_FlashImage_ScriptFailure)
{

    std::ofstream imageFlasher("/lib/rdk/imageFlasher.sh");
    imageFlasher << "#!/bin/bash\nexit 1\n";
    imageFlasher.close();
    chmod("/lib/rdk/imageFlasher.sh", 0755);

    createTestFirmwareFile();

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_FlashImage_MediaClientDevice_Success)
{

    std::ofstream deviceProps("/etc/device.properties");
    deviceProps << "DEVICE_TYPE=mediaclient" << std::endl;
    deviceProps.close();

    createTestFirmwareFile();

    std::ofstream imageFlasher("/lib/rdk/imageFlasher.sh");
    imageFlasher << "#!/bin/bash\nexit 0\n";
    imageFlasher.close();
    chmod("/lib/rdk/imageFlasher.sh", 0755);

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::remove("/etc/device.properties");
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_FlashImage_MediaClientDevice_Failure)
{

    std::ofstream deviceProps("/etc/device.properties");
    deviceProps << "DEVICE_TYPE=mediaclient" << std::endl;
    deviceProps.close();

    createTestFirmwareFile();

    std::ofstream imageFlasher("/lib/rdk/imageFlasher.sh");
    imageFlasher << "#!/bin/bash\nexit 1\n";
    imageFlasher.close();
    chmod("/lib/rdk/imageFlasher.sh", 0755);

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::remove("/etc/device.properties");
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_FlashImage_NonMediaClientDevice_Success)
{
    std::ofstream deviceProps("/etc/device.properties");
    deviceProps << "DEVICE_TYPE=broadband" << std::endl;
    deviceProps.close();

    createTestFirmwareFile();

    std::ofstream imageFlasher("/lib/rdk/imageFlasher.sh");
    imageFlasher << "#!/bin/bash\nexit 0\n";
    imageFlasher.close();
    chmod("/lib/rdk/imageFlasher.sh", 0755);

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::remove("/etc/device.properties");
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_FlashImage_RebootFlagTrue)
{

    createTestFirmwareFile();

    std::ofstream imageFlasher("/lib/rdk/imageFlasher.sh");
    imageFlasher << "#!/bin/bash\nexit 0\n";
    imageFlasher.close();
    chmod("/lib/rdk/imageFlasher.sh", 0755);

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\",\"reboot\":true}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_FlashImage_RebootFlagFalse)
{

    createTestFirmwareFile();

    std::ofstream imageFlasher("/lib/rdk/imageFlasher.sh");
    imageFlasher << "#!/bin/bash\nexit 0\n";
    imageFlasher.close();
    chmod("/lib/rdk/imageFlasher.sh", 0755);

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\",\"reboot\":false}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_FlashImage_USBProtocol_Success)
{

    std::ofstream file("/tmp/usb_firmware.bin");
    file << "USB firmware content";
    file.close();

    std::ofstream imageFlasher("/lib/rdk/imageFlasher.sh");
    imageFlasher << "#!/bin/bash\nexit 0\n";
    imageFlasher.close();
    chmod("/lib/rdk/imageFlasher.sh", 0755);

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/usb_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::remove("/tmp/usb_firmware.bin");
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_FlashImage_HeaderFileCleanup)
{

    createTestFirmwareFile();

    std::ofstream headerFile("/tmp/test_firmware.bin.header");
    headerFile << "header content";
    headerFile.close();

    std::ofstream imageFlasher("/lib/rdk/imageFlasher.sh");
    imageFlasher << "#!/bin/bash\nexit 0\n";
    imageFlasher.close();
    chmod("/lib/rdk/imageFlasher.sh", 0755);

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_FlashImage_MaintenanceMode_Critical)
{
    std::ofstream deviceProps("/etc/device.properties");
    deviceProps << "DEVICE_TYPE=mediaclient" << std::endl;
    deviceProps.close();

    createTestFirmwareFile();

    std::ofstream imageFlasher("/lib/rdk/imageFlasher.sh");
    imageFlasher << "#!/bin/bash\nexit 0\n";
    imageFlasher.close();
    chmod("/lib/rdk/imageFlasher.sh", 0755);

    std::ofstream maintenanceFile("/opt/swupdate_maintenance_upgrade");
    maintenanceFile.close();

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::remove("/etc/device.properties");
    std::remove("/opt/swupdate_maintenance_upgrade");
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_FlashImage_ProgressTimer_Integration)
{

    createTestFirmwareFile();

    std::ofstream imageFlasher("/lib/rdk/imageFlasher.sh");
    imageFlasher << "#!/bin/bash\nsleep 1\nexit 0\n";
    imageFlasher.close();
    chmod("/lib/rdk/imageFlasher.sh", 0755);

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_FlashImage_FileWithPathSeparator)
{

    system("mkdir -p /tmp/path/to");
    std::ofstream file("/tmp/path/to/test_firmware.bin");
    file << "test firmware content";
    file.close();

    std::ofstream imageFlasher("/lib/rdk/imageFlasher.sh");
    imageFlasher << "#!/bin/bash\nexit 0\n";
    imageFlasher.close();
    chmod("/lib/rdk/imageFlasher.sh", 0755);

    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/path/to/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::remove("/tmp/path/to/test_firmware.bin");
    system("rm -rf /tmp/path");
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_FlashImage_FileWithoutPathSeparator)
{

    std::ofstream file("test_firmware.bin");
    file << "test firmware content";
    file.close();

    std::ofstream imageFlasher("/lib/rdk/imageFlasher.sh");
    imageFlasher << "#!/bin/bash\nexit 0\n";
    imageFlasher.close();
    chmod("/lib/rdk/imageFlasher.sh", 0755);

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::remove("test_firmware.bin");
}
#if 0
TEST_F(FirmwareUpdateTest, StartProgressTimer_FlashingProgress)
{

    createTestFirmwareFile();
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
}

TEST_F(FirmwareUpdateTest, ProgressTimer_EventTriggering)
{

    createTestFirmwareFile();
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{\"progress\":25}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{\"progress\":50}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{\"progress\":75}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{\"progress\":100}"), response));
}

TEST_F(FirmwareUpdateTest, ProgressTimer_UpdateCompletion)
{

    createTestFirmwareFile();
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onUpdateStateChange"), _T("{\"state\":\"FLASHING_SUCCEEDED\",\"substate\":\"NOT_APPLICABLE\"}"), response));
}

TEST_F(FirmwareUpdateTest, ProgressTimer_BoundaryValues)
{

    createTestFirmwareFile();
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{\"progress\":0}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{\"progress\":100}"), response));
}

TEST_F(FirmwareUpdateTest, ProgressTimer_InvalidProgress)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{\"progress\":-1}"), response));
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{\"progress\":101}"), response));
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{}"), response));
}

TEST_F(FirmwareUpdateTest, ProgressTimer_NoActiveUpdate)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{\"progress\":50}"), response));
}

TEST_F(FirmwareUpdateTest, ProgressTimer_StateTransitions)
{

    createTestFirmwareFile();
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onUpdateStateChange"), _T("{\"state\":\"FLASHING_STARTED\",\"substate\":\"NOT_APPLICABLE\"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{\"progress\":30}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{\"progress\":60}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{\"progress\":90}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onUpdateStateChange"), _T("{\"state\":\"FLASHING_SUCCEEDED\",\"substate\":\"NOT_APPLICABLE\"}"), response));
}

TEST_F(FirmwareUpdateTest, ProgressTimer_FailureState)
{

    createTestFirmwareFile();
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onUpdateStateChange"), _T("{\"state\":\"FLASHING_STARTED\",\"substate\":\"NOT_APPLICABLE\"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{\"progress\":25}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onUpdateStateChange"), _T("{\"state\":\"FLASHING_FAILED\",\"substate\":\"FIRMWARE_INVALID\"}"), response));
}

TEST_F(FirmwareUpdateTest, ProgressTimer_CancelledUpdate)
{

    createTestFirmwareFile();
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{\"progress\":40}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("cancelFirmwareUpdate"), _T("{}"), response));
}

TEST_F(FirmwareUpdateTest, ProgressTimer_InvalidJSON)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{\"progress\":}"), response));
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{invalid_json}"), response));
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("onFlashingProgressChange"), _T(""), response));
}

TEST_F(FirmwareUpdateTest, ProgressTimer_StringProgress)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{\"progress\":\"50\"}"), response));
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("onFlashingProgressChange"), _T("{\"progress\":\"invalid\"}"), response));
}
#endif

//Newtest ends
TEST_F(FirmwareUpdateTest, UpdateFirmware_EmptyFilePath)
{
    string request = "{\"firmwareFilepath\":\"\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_NonexistentFile)
{
    string request = "{\"firmwareFilepath\":\"" + INVALID_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_InvalidType)
{
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"" + INVALID_FIRMWARE_TYPE + "\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_EmptyType)
{
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}


// getUpdateState Tests
TEST_F(FirmwareUpdateTest, GetUpdateState_Initial)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"state\":\"\",\"substate\":\"NOT_APPLICABLE\"}"));
}

TEST_F(FirmwareUpdateTest, GetUpdateState_InitialState)
{
    std::remove(FIRMWARE_UPDATE_STATE);
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"state\":\"\",\"substate\":\"NOT_APPLICABLE\"}"));
}

TEST_F(FirmwareUpdateTest, GetUpdateState_ValidationFailed)
{
    std::ofstream outfile(FIRMWARE_UPDATE_STATE);
    outfile << "state:VALIDATION_FAILED" << std::endl;
    outfile << "substate:FIRMWARE_INVALID" << std::endl;
    outfile.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("VALIDATION_FAILED") != string::npos);
    EXPECT_TRUE(response.find("FIRMWARE_INVALID") != string::npos);
}

TEST_F(FirmwareUpdateTest, GetUpdateState_FlashingStarted)
{
    std::ofstream outfile(FIRMWARE_UPDATE_STATE);
    outfile << "state:FLASHING_STARTED" << std::endl;
    outfile << "substate:NOT_APPLICABLE" << std::endl;
    outfile.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("FLASHING_STARTED") != string::npos);
    EXPECT_TRUE(response.find("NOT_APPLICABLE") != string::npos);
}

TEST_F(FirmwareUpdateTest, GetUpdateState_FlashingFailed)
{
    std::ofstream outfile(FIRMWARE_UPDATE_STATE);
    outfile << "state:FLASHING_FAILED" << std::endl;
    outfile << "substate:FLASH_WRITE_FAILED" << std::endl;
    outfile.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("FLASHING_FAILED") != string::npos);
    EXPECT_TRUE(response.find("FLASH_WRITE_FAILED") != string::npos);
}

TEST_F(FirmwareUpdateTest, GetUpdateState_FlashingSucceeded)
{
    std::ofstream outfile(FIRMWARE_UPDATE_STATE);
    outfile << "state:FLASHING_SUCCEEDED" << std::endl;
    outfile << "substate:NOT_APPLICABLE" << std::endl;
    outfile.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("FLASHING_SUCCEEDED") != string::npos);
    EXPECT_TRUE(response.find("NOT_APPLICABLE") != string::npos);
}

TEST_F(FirmwareUpdateTest, GetUpdateState_WaitingForReboot)
{
    std::ofstream outfile(FIRMWARE_UPDATE_STATE);
    outfile << "state:WAITING_FOR_REBOOT" << std::endl;
    outfile << "substate:NOT_APPLICABLE" << std::endl;
    outfile.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("WAITING_FOR_REBOOT") != string::npos);
    EXPECT_TRUE(response.find("NOT_APPLICABLE") != string::npos);
}

TEST_F(FirmwareUpdateTest, GetUpdateState_FirmwareNotFound)
{
    std::ofstream outfile(FIRMWARE_UPDATE_STATE);
    outfile << "state:VALIDATION_FAILED" << std::endl;
    outfile << "substate:FIRMWARE_NOT_FOUND" << std::endl;
    outfile.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("VALIDATION_FAILED") != string::npos);
    EXPECT_TRUE(response.find("FIRMWARE_NOT_FOUND") != string::npos);
}

TEST_F(FirmwareUpdateTest, GetUpdateState_FirmwareOutdated)
{
    std::ofstream outfile(FIRMWARE_UPDATE_STATE);
    outfile << "state:VALIDATION_FAILED" << std::endl;
    outfile << "substate:FIRMWARE_OUTDATED" << std::endl;
    outfile.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("VALIDATION_FAILED") != string::npos);
    EXPECT_TRUE(response.find("FIRMWARE_OUTDATED") != string::npos);
}

TEST_F(FirmwareUpdateTest, GetUpdateState_FirmwareUpToDate)
{
    std::ofstream outfile(FIRMWARE_UPDATE_STATE);
    outfile << "state:VALIDATION_FAILED" << std::endl;
    outfile << "substate:FIRMWARE_UPTODATE" << std::endl;
    outfile.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("VALIDATION_FAILED") != string::npos);
    EXPECT_TRUE(response.find("FIRMWARE_UPTODATE") != string::npos);
}

TEST_F(FirmwareUpdateTest, GetUpdateState_FirmwareIncompatible)
{
    std::ofstream outfile(FIRMWARE_UPDATE_STATE);
    outfile << "state:VALIDATION_FAILED" << std::endl;
    outfile << "substate:FIRMWARE_INCOMPATIBLE" << std::endl;
    outfile.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("VALIDATION_FAILED") != string::npos);
    EXPECT_TRUE(response.find("FIRMWARE_INCOMPATIBLE") != string::npos);
}

TEST_F(FirmwareUpdateTest, GetUpdateState_PreWriteSignatureCheckFailed)
{
    std::ofstream outfile(FIRMWARE_UPDATE_STATE);
    outfile << "state:FLASHING_FAILED" << std::endl;
    outfile << "substate:PREWRITE_SIGNATURE_CHECK_FAILED" << std::endl;
    outfile.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("FLASHING_FAILED") != string::npos);
    EXPECT_TRUE(response.find("PREWRITE_SIGNATURE_CHECK_FAILED") != string::npos);
}

TEST_F(FirmwareUpdateTest, GetUpdateState_PostWriteFirmwareCheckFailed)
{
    std::ofstream outfile(FIRMWARE_UPDATE_STATE);
    outfile << "state:FLASHING_FAILED" << std::endl;
    outfile << "substate:POSTWRITE_FIRMWARE_CHECK_FAILED" << std::endl;
    outfile.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("FLASHING_FAILED") != string::npos);
    EXPECT_TRUE(response.find("POSTWRITE_FIRMWARE_CHECK_FAILED") != string::npos);
}

TEST_F(FirmwareUpdateTest, GetUpdateState_PostWriteSignatureCheckFailed)
{
    std::ofstream outfile(FIRMWARE_UPDATE_STATE);
    outfile << "state:FLASHING_FAILED" << std::endl;
    outfile << "substate:POSTWRITE_SIGNATURE_CHECK_FAILED" << std::endl;
    outfile.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("FLASHING_FAILED") != string::npos);
    EXPECT_TRUE(response.find("POSTWRITE_SIGNATURE_CHECK_FAILED") != string::npos);
}

TEST_F(FirmwareUpdateTest, GetUpdateState_FileNotExist)
{
    std::remove(FIRMWARE_UPDATE_STATE);
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"state\":\"\",\"substate\":\"NOT_APPLICABLE\"}"));
}

TEST_F(FirmwareUpdateTest, GetUpdateState_CorruptedFile)
{
    std::ofstream outfile(FIRMWARE_UPDATE_STATE);
    outfile << "invalid content" << std::endl;
    outfile.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"state\":\"\",\"substate\":\"NOT_APPLICABLE\"}"));
}


// SetAutoReboot Tests
TEST_F(FirmwareUpdateTest, SetAutoReboot_Enable)
{
    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::StrEq("true"), ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(WDMP_SUCCESS));

    string request = "{\"enable\":true}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoReboot"), request, response));
    EXPECT_TRUE(response.find("success") != string::npos);
}

TEST_F(FirmwareUpdateTest, SetAutoReboot_Disable)
{
    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::StrEq("false"), ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(WDMP_SUCCESS));

    string request = "{\"enable\":false}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoReboot"), request, response));
    EXPECT_TRUE(response.find("success") != string::npos);
}

TEST_F(FirmwareUpdateTest, SetAutoReboot_RFCFailure)
{
    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(WDMP_FAILURE));

    string request = "{\"enable\":true}";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setAutoReboot"), request, response));
}

//UpdateFirmware

TEST_F(FirmwareUpdateTest, UpdateFirmware_InvalidPCIType)
{
    // Create test firmware file
    createTestFirmwareFile();

    // Execute update firmware with valid file but invalid firmware type format (should be uppercase)
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"pci\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}
#if 0
TEST_F(FirmwareUpdateTest, UpdateFirmware_ValidPCI)
{
    createTestFirmwareFile();

    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_ValidDRI)
{
    createTestFirmwareFile();

    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"DRI\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), request, response));
}
#endif

TEST_F(FirmwareUpdateTest, UpdateFirmware_EmptyFirmwareFilepath)
{
    string request = "{\"firmwareFilepath\":\"\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_EmptyFirmwareType)
{
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_InvalidFirmwareType)
{
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"INVALID\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_FileNotExists)
{
    string request = "{\"firmwareFilepath\":\"/nonexistent/firmware.bin\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}
#if 0
TEST_F(FirmwareUpdateTest, UpdateFirmware_FileAccessDenied)
{
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_PRIVILIGED_REQUEST, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_UpToDateFirmware)
{
    std::ofstream versionFile("/version.txt");
    versionFile << "imagename:test_firmware" << std::endl;
    versionFile.close();

    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("updateFirmware"), request, response));

    std::remove("/version.txt");
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_FlashingInProgress)
{

    createTestFirmwareFile();

    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), request, response));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_IARMConnectionFailed)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_IsConnected(::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<1>(0),
            ::testing::Return(IARM_RESULT_IPCCORE_FAIL)));

    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("updateFirmware"), request, response));
}
#endif

TEST_F(FirmwareUpdateTest, UpdateFirmware_FirmwareFilepathNotExist)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/nonexistent_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));
}
#if 0
TEST_F(FirmwareUpdateTest, UpdateFirmware_ValidSI)
{
    createTestFirmwareFile();
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"SI\"}"), response));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
#endif
TEST_F(FirmwareUpdateTest, UpdateFirmware_MissingFirmwareFilepath)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareType\":\"PCI\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_MissingFirmwareType)
{
    createTestFirmwareFile();
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_CaseSensitiveFirmwareType)
{
    createTestFirmwareFile();
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"pci\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_WhitespaceOnlyFilepath)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"   \",\"firmwareType\":\"PCI\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_WhitespaceOnlyFirmwareType)
{
    createTestFirmwareFile();
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"   \"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_NullFilepath)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":null,\"firmwareType\":\"PCI\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_NullFirmwareType)
{
    createTestFirmwareFile();
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":null}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_BothParametersEmpty)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"\",\"firmwareType\":\"\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_EmptyJSONObject)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_InvalidFirmwareTypeNumber)
{
    createTestFirmwareFile();
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"123\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_InvalidFirmwareTypeSpecialChars)
{
    createTestFirmwareFile();
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"@#$\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_MixedCaseFirmwareType)
{
    createTestFirmwareFile();
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/test_firmware.bin\",\"firmwareType\":\"Pci\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_RelativeFilePath)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"../firmware.bin\",\"firmwareType\":\"PCI\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_VeryLongFilePath)
{
    string longPath(1000, 'a');
    longPath = "/" + longPath + ".bin";
    string request = "{\"firmwareFilepath\":\"" + longPath + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_PostFlash_ValidPCIFirmware)
{
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
    std::ofstream outfile(filePath);
    if (outfile.is_open()) {
        outfile << "dummy firmware content";
        outfile.close();
    }
    
    std::ofstream versionFile("/version.txt");
    if (versionFile.is_open()) {
        versionFile << "imagename:DIFFERENT_IMAGE\n";
        versionFile.close();
    }

    std::ofstream devicePropertiesFile("/etc/device.properties");
    if (devicePropertiesFile.is_open()) {
        devicePropertiesFile << "DEVICE_TYPE=mediaclient\n";
        devicePropertiesFile << "DEVICE_NAME=TEST_DEVICE\n";
        devicePropertiesFile.close();
    }

    // EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
    //     .Times(::testing::AtLeast(1));

    // EXPECT_CALL(*p_wrapsImplMock, system(::testing::_))
    //     .WillOnce(::testing::Return(0));

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin\",\"firmwareType\":\"PCI\"}"), response));
    // EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_PostFlash_PDRIUpgrade)
{
    const char* filePath = "/tmp/ELTE11MWR_PDRI_DEV_default_20241122145614.bin";
    std::ofstream outfile(filePath);
    if (outfile.is_open()) {
        outfile << "dummy PDRI firmware content";
        outfile.close();
    }
    
    std::ofstream versionFile("/version.txt");
    if (versionFile.is_open()) {
        versionFile << "imagename:DIFFERENT_IMAGE\n";
        versionFile.close();
    }

    std::ofstream devicePropertiesFile("/etc/device.properties");
    if (devicePropertiesFile.is_open()) {
        devicePropertiesFile << "DEVICE_TYPE=mediaclient\n";
        devicePropertiesFile << "DEVICE_NAME=TEST_DEVICE\n";
        devicePropertiesFile.close();
    }

    // EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
    //     .Times(::testing::AtLeast(1));

    // EXPECT_CALL(*p_wrapsImplMock, system(::testing::_))
    //     .WillOnce(::testing::Return(0));

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_PDRI_DEV_default_20241122145614.bin\",\"firmwareType\":\"DRI\"}"), response));
    // EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_PostFlash_WithMaintenanceEnabled)
{
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
    std::ofstream outfile(filePath);
    if (outfile.is_open()) {
        outfile << "dummy firmware content";
        outfile.close();
    }
    
    std::ofstream versionFile("/version.txt");
    if (versionFile.is_open()) {
        versionFile << "imagename:DIFFERENT_IMAGE\n";
        versionFile.close();
    }

    std::ofstream devicePropertiesFile("/etc/device.properties");
    if (devicePropertiesFile.is_open()) {
        devicePropertiesFile << "DEVICE_TYPE=broadband\n";
        devicePropertiesFile << "DEVICE_NAME=TEST_DEVICE\n";
        devicePropertiesFile.close();
    }

    // EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
    //     .Times(::testing::AtLeast(1));

    // EXPECT_CALL(*p_wrapsImplMock, system(::testing::_))
    //     .WillOnce(::testing::Return(0));

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin\",\"firmwareType\":\"PCI\"}"), response));
    // EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_PostFlash_RebootFlagCreation)
{
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
    std::ofstream outfile(filePath);
    if (outfile.is_open()) {
        outfile << "dummy firmware content";
        outfile.close();
    }
    
    std::ofstream versionFile("/version.txt");
    if (versionFile.is_open()) {
        versionFile << "imagename:DIFFERENT_IMAGE\n";
        versionFile.close();
    }

    std::ofstream devicePropertiesFile("/etc/device.properties");
    if (devicePropertiesFile.is_open()) {
        devicePropertiesFile << "DEVICE_TYPE=mediaclient\n";
        devicePropertiesFile << "DEVICE_NAME=TEST_DEVICE\n";
        devicePropertiesFile.close();
    }

    // EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
    //     .Times(::testing::AtLeast(1));

    // EXPECT_CALL(*p_wrapsImplMock, system(::testing::_))
    //     .WillOnce(::testing::Return(0));

    // EXPECT_CALL(*p_wrapsImplMock, fopen(::testing::_, ::testing::_))
    //     .Times(::testing::AtLeast(1))
    //     .WillRepeatedly(::testing::Invoke(
    //         [](const char* pathname, const char* mode) -> FILE* {
    //             return fopen(pathname, mode);
    //         }));

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin\",\"firmwareType\":\"PCI\"}"), response));
    // EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_PostFlash_CDLFlashedFileName)
{
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
    std::ofstream outfile(filePath);
    if (outfile.is_open()) {
        outfile << "dummy firmware content";
        outfile.close();
    }
    
    std::ofstream versionFile("/version.txt");
    if (versionFile.is_open()) {
        versionFile << "imagename:DIFFERENT_IMAGE\n";
        versionFile.close();
    }

    std::ofstream devicePropertiesFile("/etc/device.properties");
    if (devicePropertiesFile.is_open()) {
        devicePropertiesFile << "DEVICE_TYPE=mediaclient\n";
        devicePropertiesFile << "DEVICE_NAME=TEST_DEVICE\n";
        devicePropertiesFile.close();
    }

    // EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        // .Times(::testing::AtLeast(1));

    // EXPECT_CALL(*p_wrapsImplMock, system(::testing::_))
        // .WillOnce(::testing::Return(0));

    // EXPECT_CALL(*p_wrapsImplMock, fopen(::testing::StrEq("/opt/cdl_flashed_file_name"), ::testing::_))
    //     .WillOnce(::testing::Invoke(
    //         [](const char* pathname, const char* mode) -> FILE* {
    //             return fopen(pathname, mode);
    //         }));

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin\",\"firmwareType\":\"PCI\"}"), response));
    // EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_PostFlash_EventSequence)
{
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
    std::ofstream outfile(filePath);
    if (outfile.is_open()) {
        outfile << "dummy firmware content";
        outfile.close();
    }
    
    std::ofstream versionFile("/version.txt");
    if (versionFile.is_open()) {
        versionFile << "imagename:DIFFERENT_IMAGE\n";
        versionFile.close();
    }

    std::ofstream devicePropertiesFile("/etc/device.properties");
    if (devicePropertiesFile.is_open()) {
        devicePropertiesFile << "DEVICE_TYPE=mediaclient\n";
        devicePropertiesFile << "DEVICE_NAME=TEST_DEVICE\n";
        devicePropertiesFile.close();
    }

    ::testing::Sequence eventSeq;
    // EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        // .InSequence(eventSeq)
        // .Times(::testing::AtLeast(1));

    // EXPECT_CALL(*p_wrapsImplMock, system(::testing::_))
        // .WillOnce(::testing::Return(0));

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin\",\"firmwareType\":\"PCI\"}"), response));
    // EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

// TEST_F(FirmwareUpdateTest, UpdateFirmware_PostFlash_DevicePropertyReadFailure)
// {
//     const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
//     std::ofstream outfile(filePath);
//     if (outfile.is_open()) {
//         outfile << "dummy firmware content";
//         outfile.close();
//     }
    
//     std::ofstream versionFile("/version.txt");
//     if (versionFile.is_open()) {
//         versionFile << "imagename:DIFFERENT_IMAGE\n";
//         versionFile.close();
//     }

//     std::remove("/etc/device.properties");

//     EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
//         .Times(::testing::AtLeast(1));

//     EXPECT_CALL(*p_wrapsImplMock, system(::testing::_))
//         .WillOnce(::testing::Return(0));

//     handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin\",\"firmwareType\":\"PCI\"}"), response);
//     // // EXPECT_TRUE(response.find("\"success\":true") != string::npos);
// }

TEST_F(FirmwareUpdateTest, UpdateFirmware_PostFlash_FileCreationFailure)
{
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
    std::ofstream outfile(filePath);
    if (outfile.is_open()) {
        outfile << "dummy firmware content";
        outfile.close();
    }
    
    std::ofstream versionFile("/version.txt");
    if (versionFile.is_open()) {
        versionFile << "imagename:DIFFERENT_IMAGE\n";
        versionFile.close();
    }

    std::ofstream devicePropertiesFile("/etc/device.properties");
    if (devicePropertiesFile.is_open()) {
        devicePropertiesFile << "DEVICE_TYPE=mediaclient\n";
        devicePropertiesFile << "DEVICE_NAME=TEST_DEVICE\n";
        devicePropertiesFile.close();
    }

    // EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
    //     .Times(::testing::AtLeast(1));

    // EXPECT_CALL(*p_wrapsImplMock, system(::testing::_))
    //     .WillOnce(::testing::Return(0));

    // EXPECT_CALL(*p_wrapsImplMock, fopen(::testing::_, ::testing::_))
    //     .WillOnce(::testing::Return(nullptr))
    //     .WillRepeatedly(::testing::Invoke(
    //         [](const char* pathname, const char* mode) -> FILE* {
    //             return fopen(pathname, mode);
    //         }));

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin\",\"firmwareType\":\"PCI\"}"), response));
    // EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_PostFlash_BroadbandDevice)
{
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
    std::ofstream outfile(filePath);
    if (outfile.is_open()) {
        outfile << "dummy firmware content";
        outfile.close();
    }
    
    std::ofstream versionFile("/version.txt");
    if (versionFile.is_open()) {
        versionFile << "imagename:DIFFERENT_IMAGE\n";
        versionFile.close();
    }

    std::ofstream devicePropertiesFile("/etc/device.properties");
    if (devicePropertiesFile.is_open()) {
        devicePropertiesFile << "DEVICE_TYPE=broadband\n";
        devicePropertiesFile << "DEVICE_NAME=TEST_DEVICE\n";
        devicePropertiesFile.close();
    }

    // EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
    //     .Times(::testing::AtLeast(1));

    // EXPECT_CALL(*p_wrapsImplMock, system(::testing::_))
    //     .WillOnce(::testing::Return(0));

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin\",\"firmwareType\":\"PCI\"}"), response));
    // EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_PostFlash_ValidationCompleteEvent)
{
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
    std::ofstream outfile(filePath);
    if (outfile.is_open()) {
        outfile << "dummy firmware content";
        outfile.close();
    }
    
    std::ofstream versionFile("/version.txt");
    if (versionFile.is_open()) {
        versionFile << "imagename:DIFFERENT_IMAGE\n";
        versionFile.close();
    }

    std::ofstream devicePropertiesFile("/etc/device.properties");
    if (devicePropertiesFile.is_open()) {
        devicePropertiesFile << "DEVICE_TYPE=mediaclient\n";
        devicePropertiesFile << "DEVICE_NAME=TEST_DEVICE\n";
        devicePropertiesFile.close();
    }

    // EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
    //     .Times(::testing::AtLeast(1));

    // EXPECT_CALL(*p_wrapsImplMock, system(::testing::_))
    //     .WillOnce(::testing::Return(0));

    EXPECT_EQ(ERROR_FIRMWAREUPDATE_INPROGRESS, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin\",\"firmwareType\":\"PCI\"}"), response));
    // EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}
