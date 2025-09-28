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

#pragma once

#include "Module.h"
#include <mutex>


namespace WPEFramework
{
    namespace Plugin
    {
        class MockSecManagerPlugin : public PluginHost::IPlugin,
                                        public PluginHost::JSONRPC
                                        {
        public:
            MockSecManagerPlugin();
            ~MockSecManagerPlugin() override;             
            static MockSecManagerPlugin *_instance;


            MockSecManagerPlugin(const MockSecManagerPlugin &) = delete;
            MockSecManagerPlugin &operator=(const MockSecManagerPlugin &) = delete;

            //all the supported interfaces are to be added here
            BEGIN_INTERFACE_MAP(MockSecManagerPlugin)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            END_INTERFACE_MAP

            // Implement the basic IPlugin interface 
            const std::string Initialize(PluginHost::IShell *service) override;
            void Deinitialize(PluginHost::IShell *service) override;
            std::string Information() const override;


        private:
            //plugin common
            uint32_t _connectionId;
            PluginHost::IShell *_service;
            std::mutex _notificationMutex;

        };
    }
}
