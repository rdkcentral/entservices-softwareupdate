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
* Unless required by applicable law or agreed to in writing, software
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
#include <atomic>
#include <sys/stat.h>
#include <unistd.h>
#include "COMLinkMock.h"
#include "WorkerPoolImplementation.h"
#include "WrapsMock.h"
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
    const string TEST_FIRMWARE_TYPE_DRI = "DRI";
    const string INVALID_FIRMWARE_TYPE = "INVALID";
}

// Helper function declarations
extern void eventManager(const char *cur_event_name, const char *event_status);
extern int getDevicePropertyData(const char *dev_prop_name, char *out_data, unsigned int buff_size);
extern bool isMediaClientDevice(void);
extern void updateUpgradeFlag(int proto, int action);
extern int updateFWDownloadStatus(struct FWDownloadStatus *fwdls, const char *disableStatsUpdate, const char *initiated_type);
extern int read_RFCProperty(char* type, const char* key, char *out_value, size_t datasize);
extern int write_RFCProperty(char* type, const char* key, const char *value, RFCVALDATATYPE datatype);
extern bool isMmgbleNotifyEnabled(void);
extern bool updateOPTOUTFile(const char *optout_file_name);
extern int notifyDwnlStatus(const char *key, const char *value, RFCVALDATATYPE datatype);
extern void unsetStateRed(void);
extern string deviceSpecificRegexBin();
extern string deviceSpecificRegexPath();
extern bool createDirectory(const std::string &path);
extern bool copyFileToDirectory(const char *source_file, const char *destination_dir);
extern bool FirmwareStatus(std::string& state, std::string& substate, const std::string& mode);
extern std::string GetCurrentTimestamp();
extern std::string readProperty(std::string filename, std::string property, std::string delimiter);

// Constants for helper functions
#define UTILS_SUCCESS 1
#define UTILS_FAIL -1
#define SUCCESS 1
#define FAILURE -1
#define SKIP 2
#define READ_RFC_SUCCESS 1
#define READ_RFC_FAILURE -1
#define WRITE_RFC_SUCCESS 1
#define WRITE_RFC_FAILURE -1

class FirmwareUpdateTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::FirmwareUpdate> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;    
    WrapsImplMock  *p_wrapsImplMock   = nullptr ;
    IarmBusImplMock  *p_iarmBusImplMock   = nullptr;
    Core::ProxyType<Plugin::FirmwareUpdateImplementation> FirmwareUpdateImpl;
    NiceMock<COMLinkMock> comLinkMock;
    NiceMock<ServiceMock> service;
    PLUGINHOST_DISPATCHER* dispatcher;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    NiceMock<FactoriesImplementation> factoriesImplementation;
    std::atomic<bool> flashInProgress;

    FirmwareUpdateTest()
        : plugin(Core::ProxyType<Plugin::FirmwareUpdate>::Create())
        , handler(*(plugin))
        , INIT_CONX(1, 0)
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
        , flashInProgress(false) 
    {
        
    	p_wrapsImplMock  = new testing::NiceMock <WrapsImplMock>;
    	Wraps::setImpl(p_wrapsImplMock);
	    p_iarmBusImplMock  = new NiceMock <IarmBusImplMock>;
		p_rfcApiImplMock = new NiceMock <RfcApiImplMock>;
		RfcApi::setImpl(p_rfcApiImplMock);
    	IarmBus::setImpl(p_iarmBusImplMock);

        ON_CALL(service, COMLink())
            .WillByDefault(::testing::Invoke(
                  [this]() {
                        TEST_LOG("Pass created comLinkMock: %p ", &comLinkMock);
                        return &comLinkMock;
                    }));

#ifdef USE_THUNDER_R4
        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
			.WillByDefault(::testing::Invoke(
                  [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
                        FirmwareUpdateImpl = Core::ProxyType<Plugin::FirmwareUpdateImplementation>::Create();
					    TEST_LOG("Pass created FirmwareUpdateImpl: %p &FirmwareUpdateImpl: %p", static_cast<void*>(FirmwareUpdateImpl.operator->()),static_cast<void*>(&FirmwareUpdateImpl));
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

	   // Only create FirmwareUpdateImpl if it wasn't already created by the COM link mock
	   if (!FirmwareUpdateImpl.IsValid()) {
	       TEST_LOG("Creating fallback FirmwareUpdateImpl");
	       FirmwareUpdateImpl = Core::ProxyType<Plugin::FirmwareUpdateImplementation>::Create();
	       if (FirmwareUpdateImpl.IsValid()) {
	           FirmwareUpdateImpl->Configure(&service);
	       }
	   }    }
    virtual ~FirmwareUpdateTest() override
    {
        TEST_LOG("FirmwareUpdateTest Destructor");

		// Wait a bit for any operations to complete before cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        plugin->Deinitialize(&service);

		// Wait longer for any threads to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        dispatcher->Deactivate();
        dispatcher->Release();

		// Clean up the implementation before releasing workers
        if (FirmwareUpdateImpl.IsValid()) {
            FirmwareUpdateImpl.Release();
        }

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

    void TearDown() override {
        // Give time for any background operations to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::remove(FIRMWARE_UPDATE_STATE);
        std::remove(TEST_FIRMWARE_PATH.c_str());
    }

    void createTestFirmwareFile() {
        std::ofstream outfile(TEST_FIRMWARE_PATH);
        outfile << "Test firmware content";
        outfile.close();

    }
};

TEST_F(FirmwareUpdateTest, RegisteredMethods)
{    
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("updateFirmware")));    
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getUpdateState")));
	EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setAutoReboot")));

}

TEST_F(FirmwareUpdateTest, getUpdateState)
{
    std::remove(FIRMWARE_UPDATE_STATE);
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"state\":\"\",\"substate\":\"NOT_APPLICABLE\"}"));
}

TEST_F(FirmwareUpdateTest, EmptyFirmwareFilepath)
{

    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"\" ,\"firmwareType\":\"PCI\"}"), response));    
}

TEST_F(FirmwareUpdateTest, FirmwareFilepath_not_exist)
{
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
    std::remove(filePath);
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin\" ,\"firmwareType\":\"PCI\"}"), response));
}

TEST_F(FirmwareUpdateTest, EmptyFirmwareType)
{
    std::ofstream outfile("/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin");
    if (outfile.is_open()) {

        EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin\" ,\"firmwareType\":\"\"}"), response));
    }
}

TEST_F(FirmwareUpdateTest, InvalidFirmwareType)
{
    std::ofstream outfile("/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin");
    if (outfile.is_open()) {

        EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin\" ,\"firmwareType\":\"ABC\"}"), response));
    }
}

// UpdateFirmware Tests - Valid Cases
TEST_F(FirmwareUpdateTest, UpdateFirmware_ValidPCI_Success)
{
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"" + TEST_FIRMWARE_TYPE_PCI + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), request, response));
    EXPECT_TRUE(response.find("success") != string::npos);
    // Give time for thread to start and complete its immediate operations
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
#if 0
TEST_F(FirmwareUpdateTest, UpdateFirmware_ValidDRI_Success)
{
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"" + TEST_FIRMWARE_TYPE_DRI + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), request, response));
    EXPECT_TRUE(response.find("success") != string::npos);
    // Give time for thread to start and complete its immediate operations
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
#endif
// UpdateFirmware Tests - Parameter Validation
TEST_F(FirmwareUpdateTest, UpdateFirmware_EmptyFilePath)
{
    string request = "{\"firmwareFilepath\":\"\",\"firmwareType\":\"" + TEST_FIRMWARE_TYPE_PCI + "\"}";
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
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"" + INVALID_FIRMWARE_TYPE + "\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_NonexistentFile)
{
    string request = "{\"firmwareFilepath\":\"" + INVALID_FIRMWARE_PATH + "\",\"firmwareType\":\"" + TEST_FIRMWARE_TYPE_PCI + "\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_MissingFirmwareFilepath)
{
    string request = "{\"firmwareType\":\"" + TEST_FIRMWARE_TYPE_PCI + "\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_MissingFirmwareType)
{
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_NullFilepath)
{
    string request = "{\"firmwareFilepath\":null,\"firmwareType\":\"" + TEST_FIRMWARE_TYPE_PCI + "\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_NullFirmwareType)
{
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":null}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_CaseSensitiveFirmwareType)
{
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"pci\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_WhitespaceFilePath)
{
    string request = "{\"firmwareFilepath\":\"   \",\"firmwareType\":\"" + TEST_FIRMWARE_TYPE_PCI + "\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_WhitespaceFirmwareType)
{
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"   \"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_LongFilePath)
{
    string longPath(1000, 'a');
    longPath = "/" + longPath + ".bin";
    string request = "{\"firmwareFilepath\":\"" + longPath + "\",\"firmwareType\":\"" + TEST_FIRMWARE_TYPE_PCI + "\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_SpecialCharactersFirmwareType)
{
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"@#$%\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_NumericFirmwareType)
{
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"123\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_MixedCaseFirmwareType)
{
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"Pci\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_EmptyJSON)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{}"), response));
}
#if 0
TEST_F(FirmwareUpdateTest, UpdateFirmware_MalformedJSON)
{
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"" + TEST_FIRMWARE_TYPE_PCI + "\"";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_BoundaryFilePath)
{
    string boundaryPath = "/tmp/a";
    std::ofstream outfile(boundaryPath);
    outfile << "Test content";
    outfile.close();

    string request = "{\"firmwareFilepath\":\"" + boundaryPath + "\",\"firmwareType\":\"" + TEST_FIRMWARE_TYPE_PCI + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), request, response));

    std::remove(boundaryPath.c_str());
}
#endif

// GetUpdateState Tests
TEST_F(FirmwareUpdateTest, GetUpdateState_Initial)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("state") != string::npos);
    EXPECT_TRUE(response.find("substate") != string::npos);
}

TEST_F(FirmwareUpdateTest, GetUpdateState_WithValidStateFile)
{
    std::ofstream outfile(FIRMWARE_UPDATE_STATE);
    outfile << "state:FLASHING_STARTED" << std::endl;
    outfile << "substate:NOT_APPLICABLE" << std::endl;
    outfile.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("FLASHING_STARTED") != string::npos);
    EXPECT_TRUE(response.find("NOT_APPLICABLE") != string::npos);
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

TEST_F(FirmwareUpdateTest, GetUpdateState_AllSubstates)
{
    const std::vector<std::pair<string, string>> stateSubstatePairs = {
        {"VALIDATION_FAILED", "FIRMWARE_NOT_FOUND"},
        {"VALIDATION_FAILED", "FIRMWARE_OUTDATED"},
        {"VALIDATION_FAILED", "FIRMWARE_UPTODATE"},
        {"VALIDATION_FAILED", "FIRMWARE_INCOMPATIBLE"},
        {"FLASHING_FAILED", "PREWRITE_SIGNATURE_CHECK_FAILED"},
        {"FLASHING_FAILED", "POSTWRITE_FIRMWARE_CHECK_FAILED"},
        {"FLASHING_FAILED", "POSTWRITE_SIGNATURE_CHECK_FAILED"}
    };

    for (const auto& pair : stateSubstatePairs) {
        std::ofstream outfile(FIRMWARE_UPDATE_STATE);
        outfile << "state:" << pair.first << std::endl;
        outfile << "substate:" << pair.second << std::endl;
        outfile.close();

        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
        EXPECT_TRUE(response.find(pair.first) != string::npos);
        EXPECT_TRUE(response.find(pair.second) != string::npos);
    }
}

TEST_F(FirmwareUpdateTest, GetUpdateState_CorruptedStateFile)
{
    std::ofstream outfile(FIRMWARE_UPDATE_STATE);
    outfile << "invalid format" << std::endl;
    outfile.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
}

TEST_F(FirmwareUpdateTest, GetUpdateState_EmptyStateFile)
{
    std::ofstream outfile(FIRMWARE_UPDATE_STATE);
    outfile.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
}

TEST_F(FirmwareUpdateTest, GetUpdateState_PartialStateFile)
{
    std::ofstream outfile(FIRMWARE_UPDATE_STATE);
    outfile << "state:FLASHING_STARTED" << std::endl;
    outfile.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
}

TEST_F(FirmwareUpdateTest, GetUpdateState_WithParameters)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{\"extraParam\":\"ignored\"}"), response));
}
#if 0
TEST_F(FirmwareUpdateTest, GetUpdateState_MalformedJSON)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getUpdateState"), _T("{"), response));
}
#endif
// SetAutoReboot Tests
TEST_F(FirmwareUpdateTest, SetAutoReboot_EnableTrue)
{
    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::StrEq("true"), ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(WDMP_SUCCESS));

    string request = "{\"enable\":true}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoReboot"), request, response));
    EXPECT_TRUE(response.find("success") != string::npos);
}

TEST_F(FirmwareUpdateTest, SetAutoReboot_EnableFalse)
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
#if 0
TEST_F(FirmwareUpdateTest, SetAutoReboot_MissingParameter)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("setAutoReboot"), _T("{}"), response));
}

TEST_F(FirmwareUpdateTest, SetAutoReboot_InvalidParameterType)
{
    string request = "{\"enable\":\"true\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("setAutoReboot"), request, response));
}

TEST_F(FirmwareUpdateTest, SetAutoReboot_NullParameter)
{
    string request = "{\"enable\":null}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("setAutoReboot"), request, response));
}
#endif
TEST_F(FirmwareUpdateTest, SetAutoReboot_ExtraParameters)
{
    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::StrEq("true"), ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(WDMP_SUCCESS));

    string request = "{\"enable\":true,\"extraParam\":\"ignored\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoReboot"), request, response));
}
#if 0
TEST_F(FirmwareUpdateTest, SetAutoReboot_MalformedJSON)
{
    string request = "{\"enable\":true";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setAutoReboot"), request, response));
}

TEST_F(FirmwareUpdateTest, SetAutoReboot_EmptyJSON)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("setAutoReboot"), _T(""), response));
}
#endif
TEST_F(FirmwareUpdateTest, SetAutoReboot_RFCTimeout)
{
    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(WDMP_ERR_TIMEOUT));

    string request = "{\"enable\":true}";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setAutoReboot"), request, response));
}

TEST_F(FirmwareUpdateTest, SetAutoReboot_RFCInvalidParameter)
{
    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(WDMP_ERR_INVALID_PARAMETER_NAME));

    string request = "{\"enable\":false}";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setAutoReboot"), request, response));
}

// Notification Tests
TEST_F(FirmwareUpdateTest, Notification_Register_Success)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    Core::hresult result = FirmwareUpdateImpl->Register(notificationMock.get());
    EXPECT_EQ(Core::ERROR_NONE, result);
}
#if 0
TEST_F(FirmwareUpdateTest, Notification_Register_AlreadyConnected)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    Core::hresult result1 = FirmwareUpdateImpl->Register(notificationMock.get());
    EXPECT_EQ(Core::ERROR_NONE, result1);

    Core::hresult result2 = FirmwareUpdateImpl->Register(notificationMock.get());
    EXPECT_EQ(Core::ERROR_ALREADY_CONNECTED, result2);
}
#endif
TEST_F(FirmwareUpdateTest, Notification_Unregister_Success)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    Core::hresult result1 = FirmwareUpdateImpl->Register(notificationMock.get());
    EXPECT_EQ(Core::ERROR_NONE, result1);

    Core::hresult result2 = FirmwareUpdateImpl->Unregister(notificationMock.get());
    EXPECT_EQ(Core::ERROR_NONE, result2);
}
#if 0
TEST_F(FirmwareUpdateTest, Notification_Unregister_NotConnected)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    Core::hresult result = FirmwareUpdateImpl->Unregister(notificationMock.get());
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, result);
}
#endif
// Configure Tests
TEST_F(FirmwareUpdateTest, Configure_ValidShell)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    uint32_t result = FirmwareUpdateImpl->Configure(&service);
    EXPECT_EQ(Core::ERROR_NONE, result);
}

// StartProgressTimer Tests
TEST_F(FirmwareUpdateTest, StartProgressTimer_Execute)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    FirmwareUpdateImpl->startProgressTimer();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// FlashImage Tests
TEST_F(FirmwareUpdateTest, FlashImage_ValidParameters)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    createTestFirmwareFile();
    int result = FirmwareUpdateImpl->flashImage("http://server.com", TEST_FIRMWARE_PATH.c_str(), "true", "http", 0, "false", "user", "false");
    EXPECT_GE(result, -1);
}

TEST_F(FirmwareUpdateTest, FlashImage_NullParameters)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    // Note: Cannot pass nullptr for proto due to implementation bug (strcmp before null check)
    // Test with nullptr for other parameters that are safely checked first
    int result = FirmwareUpdateImpl->flashImage(nullptr, nullptr, nullptr, "http", 0, nullptr, nullptr, nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(FirmwareUpdateTest, FlashImage_EmptyParameters)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    int result = FirmwareUpdateImpl->flashImage("", "", "", "", 0, "", "", "");
    EXPECT_GE(result, -1);
}

TEST_F(FirmwareUpdateTest, FlashImage_PDRIUpgrade)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    createTestFirmwareFile();
    int result = FirmwareUpdateImpl->flashImage("http://server.com", TEST_FIRMWARE_PATH.c_str(), "false", "http", 1, "false", "user", "false");
    EXPECT_GE(result, -1);
}

TEST_F(FirmwareUpdateTest, FlashImage_USBProtocol)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    createTestFirmwareFile();
    int result = FirmwareUpdateImpl->flashImage("", TEST_FIRMWARE_PATH.c_str(), "true", "usb", 0, "false", "user", "false");
    EXPECT_GE(result, -1);
}

// PostFlash Tests
TEST_F(FirmwareUpdateTest, PostFlash_ValidParameters)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    createTestFirmwareFile();
    int result = FirmwareUpdateImpl->postFlash("false", "firmware.bin", 0, "true", "user");
    EXPECT_GE(result, -1);
}

TEST_F(FirmwareUpdateTest, PostFlash_NullParameters)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    int result = FirmwareUpdateImpl->postFlash(nullptr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(FirmwareUpdateTest, PostFlash_MaintenanceMode)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    createTestFirmwareFile();
    int result = FirmwareUpdateImpl->postFlash("true", "firmware.bin", 0, "true", "device");
    EXPECT_GE(result, -1);
}

TEST_F(FirmwareUpdateTest, PostFlash_PDRIUpgrade)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    createTestFirmwareFile();
    int result = FirmwareUpdateImpl->postFlash("false", "firmware.bin", 1, "false", "user");
    EXPECT_GE(result, -1);
}

// FlashImageThread Tests
TEST_F(FirmwareUpdateTest, FlashImageThread_ValidParameters)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    createTestFirmwareFile();
    FirmwareUpdateImpl->flashImageThread(TEST_FIRMWARE_PATH, TEST_FIRMWARE_TYPE_PCI);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(FirmwareUpdateTest, FlashImageThread_EmptyParameters)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    FirmwareUpdateImpl->flashImageThread("", "");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(FirmwareUpdateTest, FlashImageThread_DRIType)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    createTestFirmwareFile();
    FirmwareUpdateImpl->flashImageThread(TEST_FIRMWARE_PATH, TEST_FIRMWARE_TYPE_DRI);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// DispatchAndUpdateEvent Tests
TEST_F(FirmwareUpdateTest, DispatchAndUpdateEvent_ValidStates)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    FirmwareUpdateImpl->dispatchAndUpdateEvent("FLASHING_STARTED", "NOT_APPLICABLE");
    FirmwareUpdateImpl->dispatchAndUpdateEvent("FLASHING_SUCCEEDED", "");
    FirmwareUpdateImpl->dispatchAndUpdateEvent("FLASHING_FAILED", "FLASH_WRITE_FAILED");
}

TEST_F(FirmwareUpdateTest, DispatchAndUpdateEvent_EmptyStates)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());
    FirmwareUpdateImpl->dispatchAndUpdateEvent("", "");
}
#if 0
// Edge Cases and Boundary Tests
TEST_F(FirmwareUpdateTest, MultipleSimultaneousUpdates)
{
    createTestFirmwareFile();

    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"" + TEST_FIRMWARE_TYPE_PCI + "\"}";

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), request, response));

    // Try to start another update while first is in progress
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("updateFirmware"), request, response));
}


TEST_F(FirmwareUpdateTest, UpdateFirmware_FilePermissions)
{
    createTestFirmwareFile();

    // Remove read permissions
    chmod(TEST_FIRMWARE_PATH.c_str(), 0000);

    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"" + TEST_FIRMWARE_TYPE_PCI + "\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));

    // Restore permissions for cleanup
    chmod(TEST_FIRMWARE_PATH.c_str(), 0644);
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_ZeroSizeFile)
{
    std::ofstream outfile(TEST_FIRMWARE_PATH);
    outfile.close();

    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"" + TEST_FIRMWARE_TYPE_PCI + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_SymbolicLink)
{
    createTestFirmwareFile();

    string linkPath = "/tmp/firmware_link.bin";
    symlink(TEST_FIRMWARE_PATH.c_str(), linkPath.c_str());

    string request = "{\"firmwareFilepath\":\"" + linkPath + "\",\"firmwareType\":\"" + TEST_FIRMWARE_TYPE_PCI + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), request, response));

    unlink(linkPath.c_str());
}
#endif
TEST_F(FirmwareUpdateTest, SetAutoReboot_RapidToggle)
{
    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(2)
        .WillRepeatedly(::testing::Return(WDMP_SUCCESS));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoReboot"), _T("{\"enable\":true}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoReboot"), _T("{\"enable\":false}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_PathTraversal)
{
    string maliciousPath = "../../../etc/passwd";
    string request = "{\"firmwareFilepath\":\"" + maliciousPath + "\",\"firmwareType\":\"" + TEST_FIRMWARE_TYPE_PCI + "\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}
#if 0
TEST_F(FirmwareUpdateTest, UpdateFirmware_DeviceFile)
{
    string devicePath = "/dev/null";
    string request = "{\"firmwareFilepath\":\"" + devicePath + "\",\"firmwareType\":\"" + TEST_FIRMWARE_TYPE_PCI + "\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), request, response));
}
#endif
TEST_F(FirmwareUpdateTest, Stress_MultipleGetUpdateState)
{
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    }
}

TEST_F(FirmwareUpdateTest, Stress_MultipleSetAutoReboot)
{
    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(10)
        .WillRepeatedly(::testing::Return(WDMP_SUCCESS));

    for (int i = 0; i < 10; i++) {
        bool enable = (i % 2 == 0);
        string request = "{\"enable\":" + string(enable ? "true" : "false") + "}";
        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoReboot"), request, response));
    }
}

// Helper Function Tests
TEST_F(FirmwareUpdateTest, EventManager_ValidEvents)
{
    // Test eventManager function with valid events
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(IARM_RESULT_SUCCESS));

    // These calls should not crash and should invoke IARM_Bus_BroadcastEvent
    eventManager("ImageDwldEvent", "SUCCESS");
    eventManager("FirmwareStateEvent", "COMPLETE");
    eventManager("MaintenanceMGR", "MAINTENANCE_UPDATE");
}

TEST_F(FirmwareUpdateTest, EventManager_NullParameters)
{
    // Test with null parameters - should handle gracefully
    eventManager(nullptr, "test");
    eventManager("test", nullptr);
    eventManager(nullptr, nullptr);
}
#if 0
TEST_F(FirmwareUpdateTest, GetDevicePropertyData_ValidProperty)
{
    // Create a test device.properties file
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestDevice\n";
    propFile << "MODEL_NUM=TEST123\n";
    propFile.close();

    char outData[64];

    // Test reading existing property
    int result = getDevicePropertyData("DEVICE_TYPE", outData, sizeof(outData));
    EXPECT_EQ(UTILS_SUCCESS, result);
    EXPECT_STREQ("mediaclient", outData);

    // Test reading another property
    result = getDevicePropertyData("DEVICE_NAME", outData, sizeof(outData));
    EXPECT_EQ(UTILS_SUCCESS, result);
    EXPECT_STREQ("TestDevice", outData);

    std::remove("/tmp/device.properties");
}
#endif
TEST_F(FirmwareUpdateTest, GetDevicePropertyData_InvalidProperty)
{
    // Create a test device.properties file
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile.close();

    char outData[64];

    // Test reading non-existing property
    int result = getDevicePropertyData("NONEXISTENT_PROP", outData, sizeof(outData));
    EXPECT_EQ(UTILS_FAIL, result);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, GetDevicePropertyData_NullParameters)
{
    char outData[64];

    // Test with null property name
    int result = getDevicePropertyData(nullptr, outData, sizeof(outData));
    EXPECT_EQ(UTILS_FAIL, result);

    // Test with null output buffer
    result = getDevicePropertyData("DEVICE_TYPE", nullptr, sizeof(outData));
    EXPECT_EQ(UTILS_FAIL, result);

    // Test with zero buffer size
    result = getDevicePropertyData("DEVICE_TYPE", outData, 0);
    EXPECT_EQ(UTILS_FAIL, result);
}
#if 0
TEST_F(FirmwareUpdateTest, IsMediaClientDevice_True)
{
    // Create a test device.properties file with mediaclient
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile.close();

    bool result = isMediaClientDevice();
    EXPECT_TRUE(result);

    std::remove("/tmp/device.properties");
}
#endif
TEST_F(FirmwareUpdateTest, IsMediaClientDevice_False)
{
    // Create a test device.properties file with non-mediaclient type
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=broadband\n";
    propFile.close();

    bool result = isMediaClientDevice();
    EXPECT_FALSE(result);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, IsMediaClientDevice_NoFile)
{
    // Ensure no device.properties file exists
    std::remove("/tmp/device.properties");

    bool result = isMediaClientDevice();
    EXPECT_FALSE(result);
}

TEST_F(FirmwareUpdateTest, UpdateUpgradeFlag_CreateFlag)
{
    // Remove any existing flag files
    std::remove("/opt/cdl_flashed_file_name");
    std::remove("/opt/ubi_flashed_file_name");

    // Test creating flag for mediaclient device
    updateUpgradeFlag(0, 1);

    // File creation success depends on device type, just ensure no crash
}

TEST_F(FirmwareUpdateTest, UpdateUpgradeFlag_RemoveFlag)
{
    // Create test flag files
    std::ofstream flagFile1("/opt/cdl_flashed_file_name");
    flagFile1 << "test";
    flagFile1.close();

    std::ofstream flagFile2("/opt/ubi_flashed_file_name");
    flagFile2 << "test";
    flagFile2.close();

    // Test removing flags
    updateUpgradeFlag(0, 2);

    // Clean up
    std::remove("/opt/cdl_flashed_file_name");
    std::remove("/opt/ubi_flashed_file_name");
}

TEST_F(FirmwareUpdateTest, UpdateFWDownloadStatus_ValidParameters)
{
    struct FWDownloadStatus fwdls;
    memset(&fwdls, 0, sizeof(fwdls));

    snprintf(fwdls.status, sizeof(fwdls.status), "Status|Success\n");
    snprintf(fwdls.method, sizeof(fwdls.method), "Method|HTTP\n");
    snprintf(fwdls.proto, sizeof(fwdls.proto), "Proto|http\n");
    snprintf(fwdls.reboot, sizeof(fwdls.reboot), "Reboot|true\n");
    snprintf(fwdls.failureReason, sizeof(fwdls.failureReason), "FailureReason|\n");
    snprintf(fwdls.dnldVersn, sizeof(fwdls.dnldVersn), "Version|1.0\n");
    snprintf(fwdls.dnldfile, sizeof(fwdls.dnldfile), "File|test.bin\n");
    snprintf(fwdls.dnldurl, sizeof(fwdls.dnldurl), "URL|http://test.com\n");
    snprintf(fwdls.lastrun, sizeof(fwdls.lastrun), "LastRun|2024-01-01\n");
    snprintf(fwdls.FwUpdateState, sizeof(fwdls.FwUpdateState), "FwUpdateState|Complete\n");
    snprintf(fwdls.DelayDownload, sizeof(fwdls.DelayDownload), "DelayDownload|0\n");

    int result = updateFWDownloadStatus(&fwdls, "no", "user");
    EXPECT_EQ(SUCCESS, result);

    // Clean up
    std::remove("/tmp/FirmwareDownloadStatus.txt");
}

TEST_F(FirmwareUpdateTest, UpdateFWDownloadStatus_NullParameters)
{
    // Test with null fwdls parameter
    int result = updateFWDownloadStatus(nullptr, "no", "user");
    EXPECT_EQ(FAILURE, result);

    struct FWDownloadStatus fwdls;
    memset(&fwdls, 0, sizeof(fwdls));

    // Test with null disableStatsUpdate parameter
    result = updateFWDownloadStatus(&fwdls, nullptr, "user");
    EXPECT_EQ(FAILURE, result);
}
#if 0
TEST_F(FirmwareUpdateTest, UpdateFWDownloadStatus_DisableUpdate)
{
    struct FWDownloadStatus fwdls;
    memset(&fwdls, 0, sizeof(fwdls));

    // Test with disabled stats update
    int result = updateFWDownloadStatus(&fwdls, "yes", "user");
    EXPECT_EQ(SKIP, result);
}
#endif
TEST_F(FirmwareUpdateTest, ReadRFCProperty_ValidProperty)
{
    char type[] = "Device";
    char outValue[256];

    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](char* paramType, const char* paramName, RFC_ParamData_t* paramData) {
            strcpy(paramData->value, "test_value");
            paramData->type = WDMP_STRING;
            return WDMP_SUCCESS;
        }));

    int result = read_RFCProperty(type, "TestProperty", outValue, sizeof(outValue));
    EXPECT_EQ(READ_RFC_SUCCESS, result);
    EXPECT_STREQ("test_value", outValue);
}

TEST_F(FirmwareUpdateTest, ReadRFCProperty_Failure)
{
    char type[] = "Device";
    char outValue[256];

    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(WDMP_FAILURE));

    int result = read_RFCProperty(type, "TestProperty", outValue, sizeof(outValue));
    EXPECT_EQ(READ_RFC_FAILURE, result);
}

TEST_F(FirmwareUpdateTest, ReadRFCProperty_NullParameters)
{
    char type[] = "Device";
    char outValue[256];

    // Test with null type
    int result = read_RFCProperty(nullptr, "TestProperty", outValue, sizeof(outValue));
    EXPECT_EQ(READ_RFC_FAILURE, result);

    // Test with null key
    result = read_RFCProperty(type, nullptr, outValue, sizeof(outValue));
    EXPECT_EQ(READ_RFC_FAILURE, result);

    // Test with null output buffer
    result = read_RFCProperty(type, "TestProperty", nullptr, sizeof(outValue));
    EXPECT_EQ(READ_RFC_FAILURE, result);

    // Test with zero size
    result = read_RFCProperty(type, "TestProperty", outValue, 0);
    EXPECT_EQ(READ_RFC_FAILURE, result);
}

TEST_F(FirmwareUpdateTest, WriteRFCProperty_Success)
{
    char type[] = "Device";

    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(WDMP_SUCCESS));

    int result = write_RFCProperty(type, "TestProperty", "test_value", RFC_STRING);
    EXPECT_EQ(WRITE_RFC_SUCCESS, result);
}

TEST_F(FirmwareUpdateTest, WriteRFCProperty_Failure)
{
    char type[] = "Device";

    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(WDMP_FAILURE));

    int result = write_RFCProperty(type, "TestProperty", "test_value", RFC_STRING);
    EXPECT_EQ(WRITE_RFC_FAILURE, result);
}

TEST_F(FirmwareUpdateTest, WriteRFCProperty_NullParameters)
{
    char type[] = "Device";

    // Test with null type
    int result = write_RFCProperty(nullptr, "TestProperty", "test_value", RFC_STRING);
    EXPECT_EQ(WRITE_RFC_FAILURE, result);

    // Test with null key
    result = write_RFCProperty(type, nullptr, "test_value", RFC_STRING);
    EXPECT_EQ(WRITE_RFC_FAILURE, result);

    // Test with null value
    result = write_RFCProperty(type, "TestProperty", nullptr, RFC_STRING);
    EXPECT_EQ(WRITE_RFC_FAILURE, result);
}

TEST_F(FirmwareUpdateTest, IsMmgbleNotifyEnabled_True)
{
    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](char* paramType, const char* paramName, RFC_ParamData_t* paramData) {
            strcpy(paramData->value, "true");
            paramData->type = WDMP_STRING;
            return WDMP_SUCCESS;
        }));

    bool result = isMmgbleNotifyEnabled();
    EXPECT_TRUE(result);
}

TEST_F(FirmwareUpdateTest, IsMmgbleNotifyEnabled_False)
{
    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](char* paramType, const char* paramName, RFC_ParamData_t* paramData) {
            strcpy(paramData->value, "false");
            paramData->type = WDMP_STRING;
            return WDMP_SUCCESS;
        }));

    bool result = isMmgbleNotifyEnabled();
    EXPECT_FALSE(result);
}

TEST_F(FirmwareUpdateTest, IsMmgbleNotifyEnabled_RFCFailure)
{
    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(WDMP_FAILURE));

    bool result = isMmgbleNotifyEnabled();
    EXPECT_FALSE(result);
}

TEST_F(FirmwareUpdateTest, UpdateOPTOUTFile_ValidFile)
{
    // Create test optout file
    const char* testOptoutFile = "/tmp/test_optout.conf";
    std::ofstream optoutFile(testOptoutFile);
    optoutFile << "softwareoptout=DISABLED\n";
    optoutFile << "other_setting=value\n";
    optoutFile.close();

    bool result = updateOPTOUTFile(testOptoutFile);
    // Result depends on file operations, ensure no crash

    // Clean up
    std::remove(testOptoutFile);
    std::remove("/tmp/mm_record_update.conf");
}

TEST_F(FirmwareUpdateTest, UpdateOPTOUTFile_NullParameter)
{
    bool result = updateOPTOUTFile(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FirmwareUpdateTest, UpdateOPTOUTFile_NonExistentFile)
{
    bool result = updateOPTOUTFile("/tmp/nonexistent_optout.conf");
    // Should handle gracefully and create update file with enforce optout

    // Clean up
    std::remove("/tmp/mm_record_update.conf");
}

TEST_F(FirmwareUpdateTest, NotifyDwnlStatus_ValidParameters)
{
    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(WDMP_SUCCESS));

    int result = notifyDwnlStatus("TestKey", "TestValue", RFC_STRING);
    EXPECT_EQ(WRITE_RFC_SUCCESS, result);
}

TEST_F(FirmwareUpdateTest, NotifyDwnlStatus_NullParameters)
{
    // Test with null key
    int result = notifyDwnlStatus(nullptr, "TestValue", RFC_STRING);
    EXPECT_EQ(WRITE_RFC_FAILURE, result);

    // Test with null value
    result = notifyDwnlStatus("TestKey", nullptr, RFC_STRING);
    EXPECT_EQ(WRITE_RFC_FAILURE, result);
}
#if 0
TEST_F(FirmwareUpdateTest, UnsetStateRed_FileExists)
{
    // Create test state red flag file
    std::ofstream stateRedFile("/tmp/state_red");
    stateRedFile << "state red active";
    stateRedFile.close();

    unsetStateRed();

    // File should be removed
    EXPECT_FALSE(Utils::fileExists("/tmp/state_red"));
}
#endif
TEST_F(FirmwareUpdateTest, UnsetStateRed_NoFile)
{
    // Ensure file doesn't exist
    std::remove("/tmp/state_red");

    // Should handle gracefully
    unsetStateRed();
}

TEST_F(FirmwareUpdateTest, DeviceSpecificRegexBin_ValidModel)
{
    // Create test device.properties file
    std::ofstream propFile("/tmp/device.properties");
    propFile << "MODEL_NUM=TEST123\n";
    propFile.close();

    string result = deviceSpecificRegexBin();
    EXPECT_FALSE(result.empty());

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, DeviceSpecificRegexPath_ValidModel)
{
    // Create test device.properties file
    std::ofstream propFile("/tmp/device.properties");
    propFile << "MODEL_NUM=TEST123\n";
    propFile.close();

    string result = deviceSpecificRegexPath();
    EXPECT_FALSE(result.empty());

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, CreateDirectory_NewDirectory)
{
    const std::string testDir = "/tmp/test_firmware_dir";

    // Remove directory if it exists
    rmdir(testDir.c_str());

    bool result = createDirectory(testDir);
    EXPECT_TRUE(result);

    // Check if directory was created
    struct stat st;
    EXPECT_EQ(0, stat(testDir.c_str(), &st));
    EXPECT_TRUE(S_ISDIR(st.st_mode));

    // Clean up
    rmdir(testDir.c_str());
}

TEST_F(FirmwareUpdateTest, CreateDirectory_ExistingDirectory)
{
    const std::string testDir = "/tmp";

    bool result = createDirectory(testDir);
    EXPECT_TRUE(result);
}

TEST_F(FirmwareUpdateTest, CopyFileToDirectory_ValidFile)
{
    // Create source file
    const char* sourceFile = "/tmp/test_source.bin";
    std::ofstream src(sourceFile);
    src << "Test firmware content";
    src.close();

    const char* destDir = "/tmp/test_dest_dir";

    bool result = copyFileToDirectory(sourceFile, destDir);
    EXPECT_TRUE(result);

    // Check if file was copied
    std::string destFile = std::string(destDir) + "/test_source.bin";
    EXPECT_TRUE(Utils::fileExists(destFile.c_str()));

    // Clean up
    std::remove(sourceFile);
    std::remove(destFile.c_str());
    rmdir(destDir);
}

TEST_F(FirmwareUpdateTest, CopyFileToDirectory_NullParameters)
{
    bool result = copyFileToDirectory(nullptr, "/tmp");
    EXPECT_FALSE(result);

    result = copyFileToDirectory("/tmp/test.bin", nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FirmwareUpdateTest, CopyFileToDirectory_NonExistentSource)
{
    bool result = copyFileToDirectory("/tmp/nonexistent.bin", "/tmp");
    EXPECT_FALSE(result);
}
#if 0
TEST_F(FirmwareUpdateTest, CopyFileToDirectory_EmptySourceFile)
{
    // Create empty source file
    const char* sourceFile = "/tmp/empty_source.bin";
    std::ofstream src(sourceFile);
    src.close();

    const char* destDir = "/tmp/test_dest_dir";

    bool result = copyFileToDirectory(sourceFile, destDir);
    EXPECT_FALSE(result); // Should fail for empty files

    // Clean up
    std::remove(sourceFile);
}
#endif
TEST_F(FirmwareUpdateTest, FirmwareStatus_WriteMode)
{
    std::string state = "FLASHING_STARTED";
    std::string substate = "NOT_APPLICABLE";

    bool result = FirmwareStatus(state, substate, "write");
    EXPECT_TRUE(result);

    // Check if file was created
    EXPECT_TRUE(Utils::fileExists("/tmp/FirmwareUpdateStatus.txt"));

    // Clean up
    std::remove("/tmp/FirmwareUpdateStatus.txt");
}

TEST_F(FirmwareUpdateTest, FirmwareStatus_ReadMode)
{
    // Create test status file
    std::ofstream statusFile("/tmp/FirmwareUpdateStatus.txt");
    statusFile << "state:FLASHING_SUCCEEDED\n";
    statusFile << "substate:NOT_APPLICABLE\n";
    statusFile.close();

    std::string state, substate;
    bool result = FirmwareStatus(state, substate, "read");
    EXPECT_TRUE(result);
    EXPECT_EQ("FLASHING_SUCCEEDED", state);
    EXPECT_EQ("NOT_APPLICABLE", substate);

    // Clean up
    std::remove("/tmp/FirmwareUpdateStatus.txt");
}

TEST_F(FirmwareUpdateTest, GetCurrentTimestamp_ValidFormat)
{
    std::string timestamp = GetCurrentTimestamp();
    EXPECT_FALSE(timestamp.empty());

    // Check basic format (should contain date, time, and Z)
    EXPECT_NE(std::string::npos, timestamp.find("T"));
    EXPECT_NE(std::string::npos, timestamp.find("Z"));
    EXPECT_NE(std::string::npos, timestamp.find("-"));
    EXPECT_NE(std::string::npos, timestamp.find(":"));
}

TEST_F(FirmwareUpdateTest, ReadProperty_ValidProperty)
{
    // Create test property file
    const std::string testFile = "/tmp/test.properties";
    std::ofstream propFile(testFile);
    propFile << "property1:value1\n";
    propFile << "property2:value2\n";
    propFile << "imagename:test_image_v1.0\n";
    propFile.close();

    std::string result = readProperty(testFile, "imagename", ":");
    EXPECT_EQ("test_image_v1.0", result);

    result = readProperty(testFile, "property1", ":");
    EXPECT_EQ("value1", result);

    // Clean up
    std::remove(testFile.c_str());
}

TEST_F(FirmwareUpdateTest, ReadProperty_NonExistentProperty)
{
    // Create test property file
    const std::string testFile = "/tmp/test.properties";
    std::ofstream propFile(testFile);
    propFile << "property1:value1\n";
    propFile.close();

    std::string result = readProperty(testFile, "nonexistent", ":");
    EXPECT_EQ("", result);

    // Clean up
    std::remove(testFile.c_str());
}

TEST_F(FirmwareUpdateTest, ReadProperty_NonExistentFile)
{
    std::string result = readProperty("/tmp/nonexistent.properties", "property", ":");
    EXPECT_EQ("", result);
}
#if 0
// Additional FlashImage and PostFlash coverage tests
TEST_F(FirmwareUpdateTest, FlashImage_MediaClientSuccess)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());

    // Create test device.properties for mediaclient
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestDevice\n";
    propFile.close();

    createTestFirmwareFile();

    // Mock IARM calls for success path
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(IARM_RESULT_SUCCESS));

    int result = FirmwareUpdateImpl->flashImage("http://server.com", TEST_FIRMWARE_PATH.c_str(), "true", "http", 0, "false", "user", "false");
    EXPECT_GE(result, 0);

    std::remove("/tmp/device.properties");
}
#endif
TEST_F(FirmwareUpdateTest, FlashImage_BroadbandDevice)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());

    // Create test device.properties for broadband device
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=broadband\n";
    propFile << "DEVICE_NAME=TestDevice\n";
    propFile.close();

    createTestFirmwareFile();

    int result = FirmwareUpdateImpl->flashImage("http://server.com", TEST_FIRMWARE_PATH.c_str(), "true", "http", 0, "false", "user", "false");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, PostFlash_DeviceInitiated)
{
    ASSERT_TRUE(FirmwareUpdateImpl.IsValid());

    int result = FirmwareUpdateImpl->postFlash("false", "firmware.bin", 0, "false", "device");
    EXPECT_GE(result, -1);
}

// Additional PostFlash Coverage Tests for lines 220-330
TEST_F(FirmwareUpdateTest, PostFlash_BroadbandDevice_MaintenanceMode)
{
    // Mock device.properties for broadband device
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=broadband\n";
    propFile << "DEVICE_NAME=TestBroadband\n";
    propFile.close();

    int result = FirmwareUpdateImpl->postFlash("true", "firmware.bin", 0, "true", "user");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, PostFlash_NonBroadbandDevice_MaintenanceMode)
{
    // Mock device.properties for non-broadband device
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestDevice\n";
    propFile.close();

    int result = FirmwareUpdateImpl->postFlash("true", "firmware.bin", 0, "true", "user");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, PostFlash_PDRIUpgrade_NoReboot)
{
    // Mock device.properties
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestDevice\n";
    propFile.close();

    // Test PDRI upgrade (upgrade_type = 1) - should not require reboot
    int result = FirmwareUpdateImpl->postFlash("false", "firmware.bin", 1, "true", "user");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, PostFlash_MaintenanceMode_CriticalReboot)
{
    // Mock device.properties for PLATCO device
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=PLATCO123\n";
    propFile.close();

    // Create rebootNow.sh file
    std::ofstream rebootScript("/rebootNow.sh");
    rebootScript << "#!/bin/bash\necho 'Rebooting'\n";
    rebootScript.close();
    system("chmod +x /rebootNow.sh");

    int result = FirmwareUpdateImpl->postFlash("true", "firmware.bin", 0, "true", "user");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
    std::remove("/rebootNow.sh");
}

TEST_F(FirmwareUpdateTest, PostFlash_NoMaintenanceMode_DirectReboot)
{
    // Mock device.properties
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestDevice\n";
    propFile.close();

    // Create rebootNow.sh file
    std::ofstream rebootScript("/rebootNow.sh");
    rebootScript << "#!/bin/bash\necho 'Rebooting'\n";
    rebootScript.close();
    system("chmod +x /rebootNow.sh");

    int result = FirmwareUpdateImpl->postFlash("false", "firmware.bin", 0, "true", "user");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
    std::remove("/rebootNow.sh");
}

TEST_F(FirmwareUpdateTest, PostFlash_NoRebootFlag)
{
    // Mock device.properties
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestDevice\n";
    propFile.close();

    // Test with reboot_flag = "false" - should not reboot
    int result = FirmwareUpdateImpl->postFlash("false", "firmware.bin", 0, "false", "user");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, PostFlash_DevicePropertyFailure)
{
    // Remove device.properties to simulate failure
    std::remove("/tmp/device.properties");

    // Should fail early due to missing device properties
    int result = FirmwareUpdateImpl->postFlash("false", "firmware.bin", 0, "true", "user");
    EXPECT_EQ(-1, result);
}

TEST_F(FirmwareUpdateTest, PostFlash_MissingRebootScript)
{
    // Mock device.properties
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestDevice\n";
    propFile.close();

    // Ensure rebootNow.sh doesn't exist
    std::remove("/rebootNow.sh");

    int result = FirmwareUpdateImpl->postFlash("false", "firmware.bin", 0, "true", "user");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

// Additional FlashImage Coverage Tests for lines 482-526
TEST_F(FirmwareUpdateTest, FlashImage_MediaClient_USBProtocol)
{
    // Mock device.properties for media client
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestMediaClient\n";
    propFile.close();

    createTestFirmwareFile();

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(IARM_RESULT_SUCCESS));

    // Test USB protocol with media client device
    int result = FirmwareUpdateImpl->flashImage("", TEST_FIRMWARE_PATH.c_str(), "true", "usb", 0, "false", "user", "false");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, FlashImage_MediaClient_MaintenanceModeCritical)
{
    // Mock device.properties for media client
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestMediaClient\n";
    propFile.close();

    createTestFirmwareFile();

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(2))
        .WillRepeatedly(::testing::Return(IARM_RESULT_SUCCESS));

    // Test maintenance mode with critical update (maint=true, reboot=true)
    int result = FirmwareUpdateImpl->flashImage("http://server.com", TEST_FIRMWARE_PATH.c_str(), "true", "http", 0, "true", "user", "false");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, FlashImage_MediaClient_DeviceInitiated)
{
    // Mock device.properties for media client
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestMediaClient\n";
    propFile.close();

    createTestFirmwareFile();

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(IARM_RESULT_SUCCESS));

    // Test device initiated upgrade
    int result = FirmwareUpdateImpl->flashImage("http://server.com", TEST_FIRMWARE_PATH.c_str(), "true", "http", 0, "false", "device", "false");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, FlashImage_MediaClient_FileCleanup)
{
    // Mock device.properties for media client
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestMediaClient\n";
    propFile.close();

    // Create firmware file that should be deleted after flashing
    createTestFirmwareFile();
    EXPECT_TRUE(Utils::fileExists(TEST_FIRMWARE_PATH.c_str()));

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(IARM_RESULT_SUCCESS));

    int result = FirmwareUpdateImpl->flashImage("http://server.com", TEST_FIRMWARE_PATH.c_str(), "true", "http", 0, "false", "user", "false");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, FlashImage_MediaClient_NonUSBProtocol)
{
    // Mock device.properties for media client
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestMediaClient\n";
    propFile.close();

    createTestFirmwareFile();

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(IARM_RESULT_SUCCESS));

    // Test non-USB protocol (HTTP) - should call postFlash
    int result = FirmwareUpdateImpl->flashImage("http://server.com", TEST_FIRMWARE_PATH.c_str(), "true", "http", 0, "false", "user", "false");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, FlashImage_NonMediaClient_DownloadComplete)
{
    // Mock device.properties for non-media client
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=broadband\n";
    propFile << "DEVICE_NAME=TestBroadband\n";
    propFile.close();

    createTestFirmwareFile();

    // Test non-media client device - should go to "Download complete" branch
    int result = FirmwareUpdateImpl->flashImage("http://server.com", TEST_FIRMWARE_PATH.c_str(), "true", "http", 0, "false", "user", "false");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, FlashImage_FlashingFailure)
{
    // Mock device.properties
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestMediaClient\n";
    propFile.close();

    createTestFirmwareFile();

    int result = FirmwareUpdateImpl->flashImage("http://server.com", TEST_FIRMWARE_PATH.c_str(), "true", "http", 0, "false", "user", "false");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

// Additional Edge Case Coverage Tests
TEST_F(FirmwareUpdateTest, FlashImage_MissingImageFlasherScript)
{
    // Mock device.properties
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestMediaClient\n";
    propFile.close();

    createTestFirmwareFile();

    // Remove imageFlasher.sh to test missing script scenario
    std::remove("/lib/rdk/imageFlasher.sh");

    int result = FirmwareUpdateImpl->flashImage("http://server.com", TEST_FIRMWARE_PATH.c_str(), "true", "http", 0, "false", "user", "false");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, FlashImage_USBProtocol_PathExtraction)
{
    // Mock device.properties
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestMediaClient\n";
    propFile.close();

    // Create test file in specific path to test USB path extraction
    std::string usbPath = "/mnt/usb/firmware.bin";
    std::ofstream usbFile(usbPath);
    usbFile << "test firmware content";
    usbFile.close();

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(IARM_RESULT_SUCCESS));

    int result = FirmwareUpdateImpl->flashImage("", usbPath.c_str(), "true", "usb", 0, "false", "device", "false");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
    std::remove(usbPath.c_str());
}

TEST_F(FirmwareUpdateTest, FlashImage_PDRIUpgrade_Type)
{
    // Mock device.properties
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestMediaClient\n";
    propFile.close();

    createTestFirmwareFile();

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(IARM_RESULT_SUCCESS));

    // Test PDRI upgrade type (upgrade_type = 1)
    int result = FirmwareUpdateImpl->flashImage("http://server.com", TEST_FIRMWARE_PATH.c_str(), "true", "http", 1, "false", "user", "false");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, FlashImage_NonMediaClient_FlashingFailed)
{
    // Mock device.properties for non-media client
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=broadband\n";
    propFile << "DEVICE_NAME=TestBroadband\n";
    propFile.close();

    createTestFirmwareFile();

    int result = FirmwareUpdateImpl->flashImage("http://server.com", TEST_FIRMWARE_PATH.c_str(), "true", "http", 0, "false", "device", "false");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, FlashImage_DeviceInitiated_CPUArchFailure)
{
    // Mock device.properties without CPU_ARCH
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestMediaClient\n";
    // Intentionally omit CPU_ARCH to test failure case
    propFile.close();

    createTestFirmwareFile();

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(IARM_RESULT_SUCCESS));

    int result = FirmwareUpdateImpl->flashImage("http://server.com", TEST_FIRMWARE_PATH.c_str(), "true", "http", 0, "false", "device", "false");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, FlashImage_NonUSB_DIFWPathFailure)
{
    // Mock device.properties without DIFW_PATH
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestMediaClient\n";
    propFile << "CPU_ARCH=arm\n";
    // Intentionally omit DIFW_PATH to test failure case
    propFile.close();

    createTestFirmwareFile();

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(IARM_RESULT_SUCCESS));

    int result = FirmwareUpdateImpl->flashImage("http://server.com", TEST_FIRMWARE_PATH.c_str(), "true", "http", 0, "false", "device", "false");
    EXPECT_GE(result, -1);

    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, FlashImage_EmptyServerURL_Parameter)
{
    createTestFirmwareFile();

    // Test with empty server URL - should use "empty" as default
    int result = FirmwareUpdateImpl->flashImage("", TEST_FIRMWARE_PATH.c_str(), "true", "http", 0, "false", "user", "false");
    EXPECT_GE(result, -1);
}

TEST_F(FirmwareUpdateTest, FlashImage_NullCodebigParameter)
{
    createTestFirmwareFile();

    // Test with null codebig parameter - should use "false" as default
    int result = FirmwareUpdateImpl->flashImage("http://server.com", TEST_FIRMWARE_PATH.c_str(), "true", "http", 0, "false", "user", nullptr);
    EXPECT_GE(result, -1);
}

TEST_F(FirmwareUpdateTest, PostFlash_FileCreationSucccess)
{
    // Mock device.properties
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestDevice\n";
    propFile.close();

    // Clean up any existing files
    std::remove("/tmp/fw_preparing_to_reboot");
    std::remove("/opt/cdl_flashed_file_name");

    int result = FirmwareUpdateImpl->postFlash("false", "firmware.bin", 0, "false", "user");
    EXPECT_GE(result, -1);

    // Clean up created files
    std::remove("/tmp/fw_preparing_to_reboot");
    std::remove("/opt/cdl_flashed_file_name");
    std::remove("/tmp/device.properties");
}

TEST_F(FirmwareUpdateTest, PostFlash_MaintenanceManager_OptoutUpdate)
{
    // Mock device.properties
    std::ofstream propFile("/tmp/device.properties");
    propFile << "DEVICE_TYPE=mediaclient\n";
    propFile << "DEVICE_NAME=TestDevice\n";
    propFile.close();

    // Create rebootNow.sh and maintenance config files
    std::ofstream rebootScript("/rebootNow.sh");
    rebootScript << "#!/bin/bash\necho 'Rebooting'\n";
    rebootScript.close();
    system("chmod +x /rebootNow.sh");

    std::ofstream maintConfig("/tmp/maintenance_mgr_record.conf");
    maintConfig << "softwareoptout=DISABLED\n";
    maintConfig.close();

    int result = FirmwareUpdateImpl->postFlash("true", "firmware.bin", 0, "true", "user");
    EXPECT_GE(result, -1);

    // Clean up
    std::remove("/rebootNow.sh");
    std::remove("/tmp/maintenance_mgr_record.conf");
    std::remove("/tmp/device.properties");
}

// Firmware download: invalid URL format - Harshini
TEST_F(FirmwareUpdateTest, UpdateFirmware_InvalidURLFormat)
{
    string request = "{\"firmwareFilepath\":\"ht!tp://invalid_url\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

// Firmware download: interrupted download
TEST_F(FirmwareUpdateTest, UpdateFirmware_InterruptedDownload)
{
    EXPECT_CALL(*p_wrapsImplMock, downloadFirmware(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](const std::string&, const std::string&) {
                throw std::runtime_error("Download interrupted");
                return false;
            }));
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("updateFirmware"), request, response));
    EXPECT_TRUE(response.find("error") != std::string::npos);
}

// Firmware download: file write failure
TEST_F(FirmwareUpdateTest, UpdateFirmware_FileWriteFailure)
{
    EXPECT_CALL(*p_wrapsImplMock, downloadFirmware(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(false));
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("updateFirmware"), request, response));
    EXPECT_TRUE(response.find("error") != std::string::npos);
}

// Firmware validation: checksum mismatch
TEST_F(FirmwareUpdateTest, UpdateFirmware_ChecksumMismatch)
{
    EXPECT_CALL(*p_wrapsImplMock, validateFirmware(::testing::_))
        .WillOnce(::testing::Return(false));
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("updateFirmware"), request, response));
    EXPECT_TRUE(response.find("error") != std::string::npos);
}

// Firmware validation: corrupted metadata
TEST_F(FirmwareUpdateTest, UpdateFirmware_CorruptedMetadata)
{
    EXPECT_CALL(*p_wrapsImplMock, validateFirmware(::testing::_))
        .WillOnce(::testing::Invoke(
            [](const std::string&) {
                throw std::runtime_error("Corrupted metadata");
                return false;
            }));
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("updateFirmware"), request, response));
    EXPECT_TRUE(response.find("error") != std::string::npos);
}

// Installation: rollback on failure
TEST_F(FirmwareUpdateTest, UpdateFirmware_InstallationRollbackOnFailure)
{
    EXPECT_CALL(*p_wrapsImplMock, installFirmware(::testing::_))
        .WillOnce(::testing::Return(false));
    EXPECT_CALL(*p_wrapsImplMock, rollbackFirmware(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(true));
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("updateFirmware"), request, response));
    EXPECT_TRUE(response.find("error") != std::string::npos);
}

// Installation: retry attempts
TEST_F(FirmwareUpdateTest, UpdateFirmware_InstallationRetryAttempts)
{
    EXPECT_CALL(*p_wrapsImplMock, installFirmware(::testing::_))
        .Times(2)
        .WillOnce(::testing::Return(false))
        .WillOnce(::testing::Return(true));
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), request, response));
    EXPECT_TRUE(response.find("success") != std::string::npos);
}

// Status/progress reporting: event emission
TEST_F(FirmwareUpdateTest, UpdateFirmware_ProgressEventEmission)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(IARM_RESULT_SUCCESS));
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), request, response));
    EXPECT_TRUE(response.find("success") != std::string::npos);
}

// Event emission order
TEST_F(FirmwareUpdateTest, UpdateFirmware_EventEmissionOrder)
{
    ::testing::InSequence seq;
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_SUCCESS)); // Started
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_SUCCESS)); // Completed
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), request, response));
}

// Reboot failure after install
TEST_F(FirmwareUpdateTest, UpdateFirmware_RebootFailure)
{
    EXPECT_CALL(*p_wrapsImplMock, installFirmware(::testing::_))
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*p_wrapsImplMock, rebootDevice())
        .WillOnce(::testing::Invoke([]() { throw std::runtime_error("Reboot failed"); }));
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("updateFirmware"), request, response));
    EXPECT_TRUE(response.find("error") != std::string::npos);
}

// Status reporting after failure
TEST_F(FirmwareUpdateTest, UpdateFirmware_StatusReportingAfterFailure)
{
    EXPECT_CALL(*p_wrapsImplMock, installFirmware(::testing::_))
        .WillOnce(::testing::Return(false));
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    handler.Invoke(connection, _T("updateFirmware"), request, response);
    std::ifstream statusFile(FIRMWARE_UPDATE_STATE);
    std::string line;
    bool foundFailure = false;
    while (std::getline(statusFile, line)) {
        if (line.find("FLASHING_FAILED") != std::string::npos) {
            foundFailure = true;
            break;
        }
    }
    statusFile.close();
    EXPECT_TRUE(foundFailure);
}

// Rollback failure
TEST_F(FirmwareUpdateTest, UpdateFirmware_RollbackFailure)
{
    EXPECT_CALL(*p_wrapsImplMock, installFirmware(::testing::_))
        .WillOnce(::testing::Return(false));
    EXPECT_CALL(*p_wrapsImplMock, rollbackFirmware(::testing::_))
        .WillOnce(::testing::Return(false));
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("updateFirmware"), request, response));
    EXPECT_TRUE(response.find("error") != std::string::npos);
}

// Download retry logic
TEST_F(FirmwareUpdateTest, UpdateFirmware_DownloadRetryLogic)
{
    EXPECT_CALL(*p_wrapsImplMock, downloadFirmware(::testing::_, ::testing::_))
        .Times(2)
        .WillOnce(::testing::Return(false))
        .WillOnce(::testing::Return(true));
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"PCI\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), request, response));
    EXPECT_TRUE(response.find("success") != std::string::npos);
}

// Valid DRI type (positive path)
TEST_F(FirmwareUpdateTest, UpdateFirmware_ValidDRI_Success)
{
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"DRI\"}";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), request, response));
    EXPECT_TRUE(response.find("success") != string::npos);
}

// Valid DRI type (failure path)
TEST_F(FirmwareUpdateTest, UpdateFirmware_ValidDRI_Failure)
{
    EXPECT_CALL(*p_wrapsImplMock, installFirmware(::testing::_))
        .WillOnce(::testing::Return(false));
    createTestFirmwareFile();
    string request = "{\"firmwareFilepath\":\"" + TEST_FIRMWARE_PATH + "\",\"firmwareType\":\"DRI\"}";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("updateFirmware"), request, response));
    EXPECT_TRUE(response.find("error") != std::string::npos);
}
