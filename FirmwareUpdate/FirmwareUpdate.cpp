
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

#include "FirmwareUpdate.h"
#include <interfaces/IConfiguration.h>

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

namespace WPEFramework
{

    namespace {

        static Plugin::Metadata<Plugin::FirmwareUpdate> metadata(
            // Version (Major, Minor, Patch)
            API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
        );
    }

    namespace Plugin
    {

    /*
     *Register FirmwareUpdate module as wpeframework plugin
     **/
    SERVICE_REGISTRATION(FirmwareUpdate, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    FirmwareUpdate::FirmwareUpdate() : _service(nullptr), _connectionId(0), _firmwareUpdate(nullptr), _FirmwareUpdateNotification(this)
    {
        SYSLOG(Logging::Startup, (_T("FirmwareUpdate Constructor")));
    }

    FirmwareUpdate::~FirmwareUpdate()
    {
        SYSLOG(Logging::Shutdown, (string(_T("FirmwareUpdate Destructor"))));
    }

    const string FirmwareUpdate::Initialize(PluginHost::IShell* service)
    {
        string message="";

        ASSERT(nullptr != service);
        ASSERT(nullptr == _service);
        ASSERT(nullptr == _firmwareUpdate);
        ASSERT(0 == _connectionId);

        SYSLOG(Logging::Startup, (_T("FirmwareUpdate::Initialize: PID=%u"), getpid()));

        _service = service;
        _service->AddRef();
        _service->Register(&_FirmwareUpdateNotification);
        _firmwareUpdate = _service->Root<Exchange::IFirmwareUpdate>(_connectionId, 5000, _T("FirmwareUpdateImplementation"));

        if(nullptr != _firmwareUpdate)
        {
            auto configConnection = _firmwareUpdate->QueryInterface<Exchange::IConfiguration>();
            if (configConnection != nullptr) {
                configConnection->Configure(service);
                configConnection->Release();
            }
            // Register for notifications
            _firmwareUpdate->Register(&_FirmwareUpdateNotification);
            // Invoking Plugin API register to wpeframework
            Exchange::JFirmwareUpdate::Register(*this, _firmwareUpdate);
        }
        else
        {
            SYSLOG(Logging::Startup, (_T("FirmwareUpdate::Initialize: Failed to initialise FirmwareUpdate plugin")));
            message = _T("FirmwareUpdate plugin could not be initialised");
        }

        if (0 != message.length())
        {
           Deinitialize(service);
        }

        return message;
    }

    void FirmwareUpdate::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(_service == service);

        SYSLOG(Logging::Shutdown, (string(_T("FirmwareUpdate::Deinitialize"))));

        // Make sure the Activated and Deactivated are no longer called before we start cleaning up..
        if (_service != nullptr)
        {
            _service->Unregister(&_FirmwareUpdateNotification);
        }
        if (nullptr != _firmwareUpdate)
        {
            _firmwareUpdate->Unregister(&_FirmwareUpdateNotification);
            Exchange::JFirmwareUpdate::Unregister(*this);

            // Stop processing:
            RPC::IRemoteConnection* connection = service->RemoteConnection(_connectionId);
            VARIABLE_IS_NOT_USED uint32_t result = _firmwareUpdate->Release();

            _firmwareUpdate = nullptr;

            // It should have been the last reference we are releasing,
            // so it should endup in a DESTRUCTION_SUCCEEDED, if not we
            // are leaking...
            ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);

            // If this was running in a (container) process...
            if (nullptr != connection)
            {
               // Lets trigger the cleanup sequence for
               // out-of-process code. Which will guard
               // that unwilling processes, get shot if
               // not stopped friendly :-)
               connection->Terminate();
               connection->Release();
            }
        }

        _connectionId = 0;

        if (_service != nullptr)
        {
            _service->Release();
            _service = nullptr;
        }
        SYSLOG(Logging::Shutdown, (string(_T("FirmwareUpdate de-initialised"))));
    }

    string FirmwareUpdate::Information() const
    {
       // No additional info to report
       return (string());
    }

    void FirmwareUpdate::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == _connectionId) {
            ASSERT(nullptr != _service);
            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }
} // namespace Plugin
} // namespace WPEFramework
