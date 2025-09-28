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
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "MockAuthServicePlugin.h"


#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

namespace WPEFramework
{
    namespace Plugin
    {
        // Register the MockAuthServicePlugin 
        SERVICE_REGISTRATION(MockAuthServicePlugin, 1, 0);
        MockAuthServicePlugin *MockAuthServicePlugin::_instance = nullptr;
        MockAuthServicePlugin::MockAuthServicePlugin()
            : PluginHost::JSONRPC(), _connectionId(0), _service(nullptr)
        {
            TEST_LOG("Inside Mock AuthService plugin constructor");
        }

        MockAuthServicePlugin::~MockAuthServicePlugin()
        {
            TEST_LOG("Inside Mock AuthService plugin destructor");
        }

       

        const std::string MockAuthServicePlugin::Initialize(PluginHost::IShell *service)
        {
            TEST_LOG("Inside Mock AuthService plugin Initialize");
            _service = service;
            return "";
        }

        void MockAuthServicePlugin::Deinitialize(PluginHost::IShell *service)
        {
            
            std::lock_guard<std::mutex> lock(_notificationMutex);
            _service = nullptr;
        }

        std::string MockAuthServicePlugin::Information() const
        {
            TEST_LOG("Inside Mock plugin Information");
            return "MockAuthServicePlugin Information";
        }


    }
}
