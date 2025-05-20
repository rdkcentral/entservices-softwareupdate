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


#include "gtest/gtest.h"
#include "FactoriesImplementation.h"
#include "Packager.h"
#include "PackagerImplementation.h"
#include "ServiceMock.h"
#include "COMLinkMock.h"
#include "IarmBusMock.h"
#include <fstream>
#include <iostream>
#include "pkg.h"
#include "ThunderPortability.h"

using namespace WPEFramework;
using ::testing::NiceMock;
using ::testing::Return;

extern opkg_conf_t* opkg_config;

namespace {
const string config = _T("Packager");
const string callSign = _T("Packager");
const string webPrefix = _T("/Service/Packager");
const string volatilePath = _T("/tmp/");
const string dataPath = _T("/tmp/");
}

class PackagerTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::Packager> plugin;
    PluginHost::IWeb* interface;

    PackagerTest()
        : plugin(Core::ProxyType<Plugin::Packager>::Create())
    {
        interface = static_cast<PluginHost::IWeb*>(plugin->QueryInterface(PluginHost::IWeb::ID));
    }
    virtual ~PackagerTest()
    {
        interface->Release();
		plugin.Release();
    }

    virtual void SetUp()
    {
        ASSERT_TRUE(interface != nullptr);
    }

    virtual void TearDown()
    {
        ASSERT_TRUE(interface != nullptr);
    }
};

class PackagerInitializedTest : public PackagerTest {
protected:
    NiceMock<FactoriesImplementation> factoriesImplementation;
    NiceMock<ServiceMock> service;
    NiceMock<COMLinkMock> comLinkMock;
	Core::ProxyType<Plugin::PackagerImplementation> PackagerImplementation;

    PackagerInitializedTest()
        : PackagerTest()
    {
		PackagerImplementation = Core::ProxyType<Plugin::PackagerImplementation>::Create();
        ON_CALL(service, ConfigLine())
            .WillByDefault(::testing::Return("{}"));
        ON_CALL(service, WebPrefix())
            .WillByDefault(::testing::Return(webPrefix));
        ON_CALL(service, VolatilePath())
            .WillByDefault(::testing::Return(volatilePath));
        ON_CALL(service, Callsign())
            .WillByDefault(::testing::Return(callSign));
		ON_CALL(service, DataPath())
                .WillByDefault(::testing::Return(dataPath));
        ON_CALL(service, COMLink())
            .WillByDefault(::testing::Return(&comLinkMock));
#ifdef USE_THUNDER_R4
        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
			.WillByDefault(::testing::Return(&PackagerImplementation));
#else
	  ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
	    .WillByDefault(::testing::Return(PackagerImplementation));
#endif /*USE_THUNDER_R4 */
        PluginHost::IFactories::Assign(&factoriesImplementation);
        EXPECT_EQ(string(""), plugin->Initialize(&service));

        // Ensures servicePI is correctly initialized instead of nullptr in R4.
        #ifdef USE_THUNDER_R4
            PackagerImplementation->Configure(&service);
        #endif
		opkg_config->lists_dir = strdup("/tmp/test");
    }
    virtual ~PackagerInitializedTest() override
    {
        //plugin->Deinitialize(&service);
		free(opkg_config->lists_dir);
        PluginHost::IFactories::Assign(nullptr);
    }
};

/* HTTP Install Test */
TEST_F(PackagerInitializedTest, httpGetPutInstall)
{
    //HTTP_GET - Get status for all modules
    Web::Request request;
    request.Verb = Web::Request::HTTP_PUT;
    request.Path = webPrefix + _T("/Install");
    auto httpResponse = interface->Process(request);
    ASSERT_TRUE(httpResponse.IsValid());
}

/* HTTP Sync Test */
TEST_F(PackagerInitializedTest, httpGetPutSynchornize)
{
    //HTTP_GET - Get status for all modules
    Web::Request request;
    request.Verb = Web::Request::HTTP_PUT;
    request.Path = webPrefix + _T("/SynchronizeRepository");
    auto httpResponse = interface->Process(request);
    ASSERT_TRUE(httpResponse.IsValid());
}

/* Install() Test */
TEST_F(PackagerInitializedTest, InstallTest)
{
	EXPECT_EQ(Core::ERROR_NONE, PackagerImplementation->Install("Test", "1.0", "arm"));
}

/* SynchronizeRepository() Test */
TEST_F(PackagerInitializedTest, SynchronizeRepositoryTest)
{
	EXPECT_EQ(Core::ERROR_NONE, PackagerImplementation->SynchronizeRepository());
}

/* DoWork() Test */
TEST_F(PackagerInitializedTest, DoWorkTest)
{
    // Create std::string objects for the arguments
    std::string name = "Test";
    std::string version = "1.0";
    std::string arch = "arm";

    // Call the public method using references to the strings (not pointers)
    EXPECT_EQ(Core::ERROR_NONE, PackagerImplementation->TestDoWork(name, version, arch));
}

/* FreeOPKG() Test */
TEST_F(PackagerInitializedTest, TestFreeOPKG) {
    // Case 1: _opkgInitialized = true
    PackagerImplementation->SetOpkgInitialized(true);
    EXPECT_TRUE(PackagerImplementation->IsOpkgInitialized());
    PackagerImplementation->TestFreeOPKG();
    EXPECT_FALSE(PackagerImplementation->IsOpkgInitialized());

    // Case 2: _opkgInitialized = false
    PackagerImplementation->SetOpkgInitialized(false);
    EXPECT_FALSE(PackagerImplementation->IsOpkgInitialized());
    PackagerImplementation->TestFreeOPKG();
    EXPECT_FALSE(PackagerImplementation->IsOpkgInitialized());
}

/* InitOPKG() Test */
TEST_F(PackagerInitializedTest, TestInitOPKGInitialized) {
    static opkg_conf_t mock_opkg_config;
    static char mock_cache_dir[] = "/mock/cache/dir";
    mock_opkg_config.cache_dir = mock_cache_dir;
    opkg_config = &mock_opkg_config;
    PackagerImplementation->SetConfigFile("/mock/config/file");
    PackagerImplementation->SetTempPath("/mock/temp/path");
    PackagerImplementation->SetCachePath("/mock/cache/path");
    PackagerImplementation->SetVerbosity(3);
    PackagerImplementation->SetNoDeps(false);
    PackagerImplementation->SetVolatileCache(true);
    PackagerImplementation->SetSkipSignatureChecking(false);

    bool result = PackagerImplementation->TestInitOPKG();

    EXPECT_TRUE(result) << "Expected InitOPKG to return true, but it returned false";
}

/* GetMetaDataFile() Test */
TEST_F(PackagerInitializedTest, TestGetMetadataFileInitialized) {
    static opkg_conf_t mock_opkg_config;
    static char mock_cache_dir[] = "/mock/cache/dir";
    mock_opkg_config.cache_dir = mock_cache_dir;
    opkg_config = &mock_opkg_config;
    PackagerImplementation->SetOpkgInitialized(true);
    std::string appName = "testApp";
    std::string expectedPath = std::string(opkg_config->cache_dir) + "/" + appName + "/etc/apps/" + appName + "_package.json";

    std::string result = PackagerImplementation->TestGetMetadataFile(appName);

    EXPECT_EQ(result, expectedPath);
}

/* GetInstallationPath() Test */
TEST_F(PackagerInitializedTest, TestGetInstallationPathInitialized) {
    static opkg_conf_t mock_opkg_config;
    static char mock_cache_dir[] = "/mock/cache/dir";
    mock_opkg_config.cache_dir = mock_cache_dir;
    opkg_config = &mock_opkg_config;
    PackagerImplementation->SetOpkgInitialized(true);
    std::string appName = "testApp";
    std::string expectedPath = std::string(opkg_config->cache_dir) + "/" + appName;

    std::string result = PackagerImplementation->TestGetInstallationPath(appName);

    EXPECT_EQ(result, expectedPath);
}

/* GetCallSign() Test */
TEST_F(PackagerInitializedTest, TestGetCallsignWithFileCheck) {
    std::string dirPath = "/tmp/test/testApp/etc/apps";
    std::string filePath = dirPath + "/testApp_package.json";
    std::string jsonData = R"({"type": "plugin", "callsign": "yourPluginCallsign"})";
    std::ofstream outFile(filePath);
    ASSERT_TRUE(outFile.is_open()) << "Failed to create file: " << filePath;
    outFile << jsonData;
    outFile.close();
    std::string mfilename = filePath;
    std::cout << "Checking file: " << mfilename << std::endl;

    std::string result = PackagerImplementation->TestGetCallsign(mfilename);

    EXPECT_FALSE(result.empty()) << "Expected callsign to be present but it was empty!";
    EXPECT_EQ(result, "yourPluginCallsign");
}

/* UpdateConfig() Test */
TEST_F(PackagerInitializedTest, TestUpdateConfig) {
    PackagerImplementation->SetConfigFile("/mock/config/file");
    PackagerImplementation->SetTempPath("/mock/temp/path");
    PackagerImplementation->SetCachePath("/mock/cache/dir");
    PackagerImplementation->SetVerbosity(3);
    PackagerImplementation->SetNoDeps(true);
    PackagerImplementation->SetVolatileCache(true);
    PackagerImplementation->SetSkipSignatureChecking(false);

    PackagerImplementation->TestUpdateConfig();

    EXPECT_STREQ(opkg_config->conf_file, "/mock/config/file");
    EXPECT_STREQ(opkg_config->tmp_dir, "/mock/temp/path");
    EXPECT_STREQ(opkg_config->cache_dir, "/mock/cache/dir");
    EXPECT_EQ(opkg_config->verbosity, 3);
    EXPECT_EQ(opkg_config->nodeps, 1);  // true means 1
    EXPECT_EQ(opkg_config->volatile_cache, 1);  // true means 1
    EXPECT_EQ(opkg_config->check_pkg_signature, 1);  // skipSignatureChecking = false, so it should be 1
    EXPECT_STREQ(opkg_config->signature_type, "provision");
}

/* NotifyStatusChange() Test */
TEST_F(PackagerInitializedTest, TestNotifyStateChange) {
    // Call the public test method that invokes NotifyStateChange
    PackagerImplementation->TestNotifyStateChange();
}

/* NotifyRepoSynced() Test */
TEST_F(PackagerInitializedTest, TestNotifyRepoSynced) {
    uint32_t testStatus = 42;
    PackagerImplementation->TestNotifyRepoSynced(testStatus);

    EXPECT_FALSE(PackagerImplementation->GetIsSyncing());
}

/* BlockingSetuplocalRepoNoLock() Test */
TEST_F(PackagerInitializedTest, TestBlockingSetupLocalRepoNoLock) {
    WPEFramework::Plugin::PackagerImplementation::RepoSyncModeType mode = WPEFramework::Plugin::PackagerImplementation::RepoSyncModeType::SETUP;
    PackagerImplementation->SetRepoSyncMode(mode);
    PackagerImplementation->TestBlockingSetupLocalRepoNoLock(PackagerImplementation->GetRepoSyncMode());
    EXPECT_FALSE(PackagerImplementation->GetIsSyncing());
}


