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

