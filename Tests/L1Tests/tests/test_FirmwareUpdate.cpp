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
}

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

    FirmwareUpdateTest()
        : plugin(Core::ProxyType<Plugin::FirmwareUpdate>::Create())
        , handler(*(plugin))
        , INIT_CONX(1, 0)
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
    {
        
    	p_wrapsImplMock  = new testing::NiceMock <WrapsImplMock>;
    	Wraps::setImpl(p_wrapsImplMock);
	p_iarmBusImplMock  = new NiceMock <IarmBusImplMock>;
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
                        TEST_LOG("Pass created FirmwareUpdateImpl: %p &FirmwareUpdateImpl: %p", FirmwareUpdateImpl, &FirmwareUpdateImpl);
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

        PluginHost::IFactories::Assign(nullptr);
	IarmBus::setImpl(nullptr);
        if (p_iarmBusImplMock != nullptr)
        {
            delete p_iarmBusImplMock;
            p_iarmBusImplMock = nullptr;
        }

    }
};

TEST_F(FirmwareUpdateTest, RegisteredMethods)
{    
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("updateFirmware")));    
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getUpdateState")));
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

TEST_F(FirmwareUpdateTest, RegisteredMethods_SetAutoReboot)
{    
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setAutoReboot")));
}

TEST_F(FirmwareUpdateTest, getUpdateState_WithExistingState)
{
    std::ofstream statusFile(FIRMWARE_UPDATE_STATE);
    if (statusFile.is_open()) {
        statusFile << "state:FLASHING_STARTED\n";
        statusFile << "substate:NOT_APPLICABLE\n";
        statusFile.close();
    }
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("FLASHING_STARTED") != string::npos);
}

TEST_F(FirmwareUpdateTest, getUpdateState_WithValidationFailed)
{
    std::ofstream statusFile(FIRMWARE_UPDATE_STATE);
    if (statusFile.is_open()) {
        statusFile << "state:VALIDATION_FAILED\n";
        statusFile << "substate:FIRMWARE_NOT_FOUND\n";
        statusFile.close();
    }
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("VALIDATION_FAILED") != string::npos);
    EXPECT_TRUE(response.find("FIRMWARE_NOT_FOUND") != string::npos);
}

TEST_F(FirmwareUpdateTest, getUpdateState_WithFlashingSucceeded)
{
    std::ofstream statusFile(FIRMWARE_UPDATE_STATE);
    if (statusFile.is_open()) {
        statusFile << "state:FLASHING_SUCCEEDED\n";
        statusFile << "substate:NOT_APPLICABLE\n";
        statusFile.close();
    }
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("FLASHING_SUCCEEDED") != string::npos);
}

TEST_F(FirmwareUpdateTest, getUpdateState_WithFlashingFailed)
{
    std::ofstream statusFile(FIRMWARE_UPDATE_STATE);
    if (statusFile.is_open()) {
        statusFile << "state:FLASHING_FAILED\n";
        statusFile << "substate:FLASH_WRITE_FAILED\n";
        statusFile.close();
    }
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("FLASHING_FAILED") != string::npos);
    EXPECT_TRUE(response.find("FLASH_WRITE_FAILED") != string::npos);
}

TEST_F(FirmwareUpdateTest, getUpdateState_WithWaitingForReboot)
{
    std::ofstream statusFile(FIRMWARE_UPDATE_STATE);
    if (statusFile.is_open()) {
        statusFile << "state:WAITING_FOR_REBOOT\n";
        statusFile << "substate:NOT_APPLICABLE\n";
        statusFile.close();
    }
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("WAITING_FOR_REBOOT") != string::npos);
}

// TEST_F(FirmwareUpdateTest, UpdateFirmware_ValidPCIFirmware)
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

//     EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
//         .Times(::testing::AtLeast(1));

//     EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin\",\"firmwareType\":\"PCI\"}"), response));
//     EXPECT_TRUE(response.find("\"success\":true") != string::npos);
// }

TEST_F(FirmwareUpdateTest, UpdateFirmware_ValidDRIFirmware)
{
    const char* filePath = "/tmp/ELTE11MWR_DRI_DEV_default_20241122145614.bin";
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

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_DRI_DEV_default_20241122145614.bin\",\"firmwareType\":\"DRI\"}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_SameVersionImage)
{
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
    std::ofstream outfile(filePath);
    if (outfile.is_open()) {
        outfile << "dummy firmware content";
        outfile.close();
    }
    
    std::ofstream versionFile("/version.txt");
    if (versionFile.is_open()) {
        versionFile << "imagename:ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614\n";
        versionFile.close();
    }

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin\",\"firmwareType\":\"PCI\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_InvalidFilePath_SpecialCharacters)
{
    const char* filePath = "/tmp/ELTE11MWR_@#$%^&*.bin";
    std::ofstream outfile(filePath);
    if (outfile.is_open()) {
        outfile << "dummy firmware content";
        outfile.close();
    }

    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_@#$%^&*.bin\",\"firmwareType\":\"PCI\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_InvalidFilePath_WithSpaces)
{
    const char* filePath = "/tmp/ELTE11MWR MIDDLEWARE.bin";
    std::ofstream outfile(filePath);
    if (outfile.is_open()) {
        outfile << "dummy firmware content";
        outfile.close();
    }

    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR MIDDLEWARE.bin\",\"firmwareType\":\"PCI\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_FlashingAlreadyInProgress)
{
    const char* filePath1 = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_1.bin";
    const char* filePath2 = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_2.bin";
    
    std::ofstream outfile1(filePath1);
    if (outfile1.is_open()) {
        outfile1 << "dummy firmware content 1";
        outfile1.close();
    }
    
    std::ofstream outfile2(filePath2);
    if (outfile2.is_open()) {
        outfile2 << "dummy firmware content 2";
        outfile2.close();
    }
    
    std::ofstream versionFile("/version.txt");
    if (versionFile.is_open()) {
        versionFile << "imagename:DIFFERENT_IMAGE\n";
        versionFile.close();
    }

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_1.bin\",\"firmwareType\":\"PCI\"}"), response));
    
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_2.bin\",\"firmwareType\":\"PCI\"}"), response));
}

TEST_F(FirmwareUpdateTest, SetAutoReboot_Enable)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoReboot"), _T("{\"enable\":true}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

TEST_F(FirmwareUpdateTest, SetAutoReboot_Disable)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAutoReboot"), _T("{\"enable\":false}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_InvalidBinExtension)
{
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default.txt";
    std::ofstream outfile(filePath);
    if (outfile.is_open()) {
        outfile << "dummy firmware content";
        outfile.close();
    }

    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default.txt\",\"firmwareType\":\"PCI\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_PathTraversal)
{
    const char* filePath = "/tmp/../etc/passwd";
    
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/../etc/passwd\",\"firmwareType\":\"PCI\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_RootPathNotAllowed)
{
    const char* filePath = "/ELTE11MWR_MIDDLEWARE_DEV_default.bin";
    std::ofstream outfile(filePath);
    if (outfile.is_open()) {
        outfile << "dummy firmware content";
        outfile.close();
    }

    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/ELTE11MWR_MIDDLEWARE_DEV_default.bin\",\"firmwareType\":\"PCI\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_EmptyParameters)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_MissingFirmwareType)
{
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
    std::ofstream outfile(filePath);
    if (outfile.is_open()) {
        outfile << "dummy firmware content";
        outfile.close();
    }

    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_MissingFirmwareFilepath)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareType\":\"PCI\"}"), response));
}

TEST_F(FirmwareUpdateTest, getUpdateState_MultipleSubstates_FirmwareInvalid)
{
    std::ofstream statusFile(FIRMWARE_UPDATE_STATE);
    if (statusFile.is_open()) {
        statusFile << "state:VALIDATION_FAILED\n";
        statusFile << "substate:FIRMWARE_INVALID\n";
        statusFile.close();
    }
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("FIRMWARE_INVALID") != string::npos);
}

TEST_F(FirmwareUpdateTest, getUpdateState_MultipleSubstates_FirmwareOutdated)
{
    std::ofstream statusFile(FIRMWARE_UPDATE_STATE);
    if (statusFile.is_open()) {
        statusFile << "state:VALIDATION_FAILED\n";
        statusFile << "substate:FIRMWARE_OUTDATED\n";
        statusFile.close();
    }
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("FIRMWARE_OUTDATED") != string::npos);
}

TEST_F(FirmwareUpdateTest, getUpdateState_MultipleSubstates_FirmwareUptodate)
{
    std::ofstream statusFile(FIRMWARE_UPDATE_STATE);
    if (statusFile.is_open()) {
        statusFile << "state:VALIDATION_FAILED\n";
        statusFile << "substate:FIRMWARE_UPTODATE\n";
        statusFile.close();
    }
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("FIRMWARE_UPTODATE") != string::npos);
}

TEST_F(FirmwareUpdateTest, getUpdateState_MultipleSubstates_FirmwareIncompatible)
{
    std::ofstream statusFile(FIRMWARE_UPDATE_STATE);
    if (statusFile.is_open()) {
        statusFile << "state:VALIDATION_FAILED\n";
        statusFile << "substate:FIRMWARE_INCOMPATIBLE\n";
        statusFile.close();
    }
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("FIRMWARE_INCOMPATIBLE") != string::npos);
}

TEST_F(FirmwareUpdateTest, getUpdateState_MultipleSubstates_PrewriteSignatureCheckFailed)
{
    std::ofstream statusFile(FIRMWARE_UPDATE_STATE);
    if (statusFile.is_open()) {
        statusFile << "state:VALIDATION_FAILED\n";
        statusFile << "substate:PREWRITE_SIGNATURE_CHECK_FAILED\n";
        statusFile.close();
    }
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("PREWRITE_SIGNATURE_CHECK_FAILED") != string::npos);
}

TEST_F(FirmwareUpdateTest, getUpdateState_MultipleSubstates_PostwriteFirmwareCheckFailed)
{
    std::ofstream statusFile(FIRMWARE_UPDATE_STATE);
    if (statusFile.is_open()) {
        statusFile << "state:FLASHING_FAILED\n";
        statusFile << "substate:POSTWRITE_FIRMWARE_CHECK_FAILED\n";
        statusFile.close();
    }
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("POSTWRITE_FIRMWARE_CHECK_FAILED") != string::npos);
}

TEST_F(FirmwareUpdateTest, getUpdateState_MultipleSubstates_PostwriteSignatureCheckFailed)
{
    std::ofstream statusFile(FIRMWARE_UPDATE_STATE);
    if (statusFile.is_open()) {
        statusFile << "state:FLASHING_FAILED\n";
        statusFile << "substate:POSTWRITE_SIGNATURE_CHECK_FAILED\n";
        statusFile.close();
    }
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("POSTWRITE_SIGNATURE_CHECK_FAILED") != string::npos);
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_LongFilePath)
{
    std::string longPath = "/tmp/";
    for (int i = 0; i < 50; i++) {
        longPath += "subdir/";
    }
    longPath += "firmware.bin";

    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), string("{\"firmwareFilepath\":\"") + longPath + string("\",\"firmwareType\":\"PCI\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_CaseInsensitiveFirmwareType_Lowercase)
{
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin";
    std::ofstream outfile(filePath);
    if (outfile.is_open()) {
        outfile << "dummy firmware content";
        outfile.close();
    }

    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin\",\"firmwareType\":\"pci\"}"), response));
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_NullCharacterInPath)
{
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/firmware\\u0000.bin\",\"firmwareType\":\"PCI\"}"), response));
}

TEST_F(FirmwareUpdateTest, SetAutoReboot_MissingParameter)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setAutoReboot"), _T("{}"), response));
}

TEST_F(FirmwareUpdateTest, SetAutoReboot_InvalidBooleanValue)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setAutoReboot"), _T("{\"enable\":\"invalid\"}"), response));
}

TEST_F(FirmwareUpdateTest, getUpdateState_CorruptedStatusFile)
{
    std::ofstream statusFile(FIRMWARE_UPDATE_STATE);
    if (statusFile.is_open()) {
        statusFile << "corrupted data without proper format\n";
        statusFile.close();
    }
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"state\":\"\",\"substate\":\"NOT_APPLICABLE\"}"));
}

TEST_F(FirmwareUpdateTest, getUpdateState_PartialStatusFile)
{
    std::ofstream statusFile(FIRMWARE_UPDATE_STATE);
    if (statusFile.is_open()) {
        statusFile << "state:FLASHING_STARTED\n";
        statusFile.close();
    }
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getUpdateState"), _T("{}"), response));
    EXPECT_TRUE(response.find("FLASHING_STARTED") != string::npos);
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_SymbolicLink)
{
    const char* realPath = "/tmp/real_firmware.bin";
    const char* linkPath = "/tmp/link_firmware.bin";
    
    std::ofstream outfile(realPath);
    if (outfile.is_open()) {
        outfile << "dummy firmware content";
        outfile.close();
    }
    
    symlink(realPath, linkPath);

    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/link_firmware.bin\",\"firmwareType\":\"PCI\"}"), response));
    
    unlink(linkPath);
    unlink(realPath);
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_DirectoryInsteadOfFile)
{
    mkdir("/tmp/firmware_dir", 0755);

    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/firmware_dir\",\"firmwareType\":\"PCI\"}"), response));
    
    rmdir("/tmp/firmware_dir");
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_ValidFileWithHyphen)
{
    const char* filePath = "/tmp/ELTE11MWR-MIDDLEWARE-DEV-default-20241122145614.bin";
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

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR-MIDDLEWARE-DEV-default-20241122145614.bin\",\"firmwareType\":\"PCI\"}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_ValidFileWithUnderscore)
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

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE_DEV_default_20241122145614.bin\",\"firmwareType\":\"PCI\"}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}

TEST_F(FirmwareUpdateTest, UpdateFirmware_FileWithDoubleBinExtension)
{
    const char* filePath = "/tmp/ELTE11MWR_MIDDLEWARE.bin.bin";
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

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{\"firmwareFilepath\":\"/tmp/ELTE11MWR_MIDDLEWARE.bin.bin\",\"firmwareType\":\"PCI\"}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != string::npos);
}
