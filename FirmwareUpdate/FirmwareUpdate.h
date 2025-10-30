/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
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
*/

#pragma once

#include "Module.h"
#include <interfaces/json/JsonData_FirmwareUpdate.h>
#include <interfaces/json/JFirmwareUpdate.h>
#include <interfaces/IFirmwareUpdate.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <mutex>

namespace WPEFramework {
namespace Plugin {

    class FirmwareUpdate: public PluginHost::IPlugin, public PluginHost::JSONRPCErrorAssessor<PluginHost::JSONRPCErrorAssessorTypes::FunctionCallbackType>
    {
     private:
        class Notification : public RPC::IRemoteConnection::INotification,
                             public Exchange::IFirmwareUpdate::INotification
        {
            private:
                Notification() = delete;
                Notification(const Notification&) = delete;
                Notification& operator=(const Notification&) = delete;

            public:
                explicit Notification(FirmwareUpdate* parent)
                    : _parent(*parent)
                {
                    ASSERT(parent != nullptr);
                }

                virtual ~Notification()
                {
                }

                BEGIN_INTERFACE_MAP(Notification)
                    INTERFACE_ENTRY(Exchange::IFirmwareUpdate::INotification)
                    INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                    END_INTERFACE_MAP

                    void Activated(RPC::IRemoteConnection*) override
                    {
                        LOGINFO("FirmwareUpdate Notification Activated");
                    }

                void Deactivated(RPC::IRemoteConnection *connection) override
                {
                    LOGINFO("FirmwareUpdate Notification Deactivated");
                    _parent.Deactivated(connection);
                }

                void OnUpdateStateChange (const WPEFramework::Exchange::IFirmwareUpdate::State state , const WPEFramework::Exchange::IFirmwareUpdate::SubState substate) override
                {
                    LOGINFO("OnUpdateStateChange state: %u , substate: %u \n", state,substate);
                    Exchange::JFirmwareUpdate::Event::OnUpdateStateChange(_parent, state, substate);
                }

                void OnFlashingStateChange (const uint32_t percentageComplete) override
                {
                    LOGINFO ("OnFlashingStateChange percentageComplete : %u \n", percentageComplete);
                    Exchange::JFirmwareUpdate::Event::OnFlashingStateChange(_parent,percentageComplete);
                }

            private:
                FirmwareUpdate& _parent;
        };

        public:
            // We do not allow this plugin to be copied !!
            FirmwareUpdate(const FirmwareUpdate&) = delete;
            FirmwareUpdate& operator=(const FirmwareUpdate&) = delete;

            FirmwareUpdate();
            virtual ~FirmwareUpdate();

            BEGIN_INTERFACE_MAP(FirmwareUpdate)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            INTERFACE_AGGREGATE(Exchange::IFirmwareUpdate, _firmwareUpdate)
            END_INTERFACE_MAP

            //  IPlugin methods
            // -------------------------------------------------------------------------------------------------------
            const string Initialize(PluginHost::IShell* service) override;
            void Deinitialize(PluginHost::IShell* service) override;
            string Information() const override;

            static uint32_t OnJSONRPCError(const Core::JSONRPC::Context& context, const string& method, const string& parameters, const uint32_t errorcode, string& errormessage);

        private:
            void Deactivated(RPC::IRemoteConnection* connection);

        private:
            PluginHost::IShell* _service{};
            uint32_t _connectionId{};
            Exchange::IFirmwareUpdate* _firmwareUpdate{};
            Core::Sink<Notification> _FirmwareUpdateNotification;
    };

} // namespace Plugin
} // namespace WPEFramework
