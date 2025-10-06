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
#include <interfaces/IFirmwareDownload.h>
#include <interfaces/json/JFirmwareDownload.h>
#include <interfaces/json/JsonData_FirmwareDownload.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"

namespace WPEFramework 
{
    namespace Plugin
    {
        class FirmwareDownload : public PluginHost::IPlugin, public PluginHost::JSONRPC 
        {
            private:
                class Notification : public RPC::IRemoteConnection::INotification, public Exchange::IFirmwareDownload::INotification
                {
                    private:
                        Notification() = delete;
                        Notification(const Notification&) = delete;
                        Notification& operator=(const Notification&) = delete;

                    public:
                    explicit Notification(FirmwareDownload* parent) 
                        : _parent(*parent)
                        {
                            ASSERT(parent != nullptr);
                        }

                        virtual ~Notification()
                        {
                        }

                        BEGIN_INTERFACE_MAP(Notification)
                        INTERFACE_ENTRY(Exchange::IFirmwareDownload::INotification)
                        INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                        END_INTERFACE_MAP

                        void Activated(RPC::IRemoteConnection*) override
                        {
                           
                        }

                        void Deactivated(RPC::IRemoteConnection *connection) override
                        {
                            _parent.Deactivated(connection);
                        }

                        void OnFirmwareAvailable(const int searchStatus, const string& serverResponse, const bool firmwareAvailable, const string& firmwareVersion, const bool rebootImmediately) override
                        {
                            Exchange::JFirmwareDownload::Event::OnFirmwareAvailable(_parent, searchStatus, serverResponse, firmwareAvailable, firmwareVersion, rebootImmediately);
                        }

                    private:
                        FirmwareDownload& _parent;
                };

                public:
                    FirmwareDownload(const FirmwareDownload&) = delete;
                    FirmwareDownload& operator=(const FirmwareDownload&) = delete;

                    FirmwareDownload();
                    virtual ~FirmwareDownload();

                    BEGIN_INTERFACE_MAP(FirmwareDownload)
                    INTERFACE_ENTRY(PluginHost::IPlugin)
                    INTERFACE_ENTRY(PluginHost::IDispatcher)
                    INTERFACE_AGGREGATE(Exchange::IFirmwareDownload, _firmwareDownload)
                    END_INTERFACE_MAP

                    //  IPlugin methods
                    // -------------------------------------------------------------------------------------------------------
                    const string Initialize(PluginHost::IShell* service) override;
                    void Deinitialize(PluginHost::IShell* service) override;
                    string Information() const override;

                private:
                    void Deactivated(RPC::IRemoteConnection* connection);

                private:
                    PluginHost::IShell* _service{};
                    uint32_t _connectionId{};
                    Exchange::IFirmwareDownload* _firmwareDownload{};
                    Core::Sink<Notification> _firmwareDownloadNotification;
       };
    } // namespace Plugin
} // namespace WPEFramework
