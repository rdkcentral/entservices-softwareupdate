/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
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
**/

#include "FirmwareDownloadImplementation.h"

#include "UtilsJsonRpc.h"

namespace WPEFramework
{
    namespace Plugin
    {
        SERVICE_REGISTRATION(FirmwareDownloadImplementation, 1, 0);
        FirmwareDownloadImplementation* FirmwareDownloadImplementation::_instance = nullptr;
    
        FirmwareDownloadImplementation::FirmwareDownloadImplementation() : _adminLock() , _service(nullptr)
        {
            LOGINFO("Create FirmwareDownloadImplementation Instance");

            FirmwareDownloadImplementation::_instance = this;
        }

        FirmwareDownloadImplementation::~FirmwareDownloadImplementation()
        {
            FirmwareDownloadImplementation::_instance = nullptr;
            _service = nullptr;
        }

        Core::hresult FirmwareDownloadImplementation::Register(Exchange::IFirmwareDownload::INotification *notification)
        {
            ASSERT (nullptr != notification);

            _adminLock.Lock();

            // Make sure we can't register the same notification callback multiple times
            if (std::find(_firmwareDownloadNotification.begin(), _firmwareDownloadNotification.end(), notification) == _firmwareDownloadNotification.end())
            {
                _firmwareDownloadNotification.push_back(notification);
                notification->AddRef();
            }
            else
            {
                LOGERR("same notification is registered already");
            }

            _adminLock.Unlock();

            return Core::ERROR_NONE;
        }

        Core::hresult FirmwareDownloadImplementation::Unregister(Exchange::IFirmwareDownload::INotification *notification )
        {
            Core::hresult status = Core::ERROR_GENERAL;

            ASSERT (nullptr != notification);

            _adminLock.Lock();

            // we just unregister one notification once
            auto itr = std::find(_firmwareDownloadNotification.begin(), _firmwareDownloadNotification.end(), notification);
            if (itr != _firmwareDownloadNotification.end())
            {
                (*itr)->Release();
                _firmwareDownloadNotification.erase(itr);
                status = Core::ERROR_NONE;
            }
            else
            {
                LOGERR("notification not found");
            }

            _adminLock.Unlock();

            return status;
        }
        
        void FirmwareDownloadImplementation::OnFirmwareAvailable(int searchStatus, string serverResponse, bool firmwareAvailable, string firmwareVersion, bool rebootImmediately)
        {
            JsonObject params;
            params["searchStatus"] = searchStatus;
            params["serverResponse"] = serverResponse;
            params["firmwareAvailable"] = firmwareAvailable;
            params["firmwareVersion"] = firmwareVersion;
            params["rebootImmediately"] = rebootImmediately;
            dispatchEvent(FIRMWAREDOWNLOAD_EVT_ONFIRMWAREAVAILABLE, params);
        }

        void FirmwareDownloadImplementation::dispatchEvent(Event event, const JsonObject &params)
        {
            Core::IWorkerPool::Instance().Submit(Job::Create(this, event, params));
        }

        void FirmwareDownloadImplementation::Dispatch(Event event, const JsonObject params)
        {
            _adminLock.Lock();
        
            std::list<Exchange::IFirmwareDownload::INotification*>::const_iterator index(_firmwareDownloadNotification.begin());
        
            switch(event)
            {
                case FIRMWAREDOWNLOAD_EVT_ONFIRMWAREAVAILABLE:
                    while (index != _firmwareDownloadNotification.end()) 
                    {
                        int searchStatus = params["searchStatus"].Number();
                        string serverResponse = params["serverResponse"].String();
                        bool firmwareAvailable = params["firmwareAvailable"].Boolean();
                        string firmwareVersion = params["firmwareVersion"].String();
                        bool rebootImmediately = params["rebootImmediately"].Boolean();
                        (*index)->OnFirmwareAvailable(searchStatus, serverResponse, firmwareAvailable, firmwareVersion, rebootImmediately);
                        ++index;
                    }
                    break;

                default:
                    LOGWARN("Event[%u] not handled", event);
                    break;
            }
            _adminLock.Unlock();
        }
        
        Core::hresult FirmwareDownloadImplementation::GetDownloadedFirmwareInfo(string& currentFWVersion, string& downloadedFWVersion, string& downloadedFWLocation, bool& isRebootDeferred)
        {
            return Core::ERROR_NONE;
        }
        
        Core::hresult FirmwareDownloadImplementation::GetFirmwareDownloadPercent(FirmwareDownloadPercent& firmwareDownloadPercent)
        {
            return Core::ERROR_NONE;
        }

        Core::hresult FirmwareDownloadImplementation::SearchFirmware()
        {
            return Core::ERROR_NONE;
        }
        
        Core::hresult FirmwareDownloadImplementation::GetDownloadState( FirmwareDownloadState& downloadState)
        {
            return Core::ERROR_NONE;
        }
        
        Core::hresult FirmwareDownloadImplementation::GetDownloadFailureReason( DownloadFailureReason& downloadFailureReason)
        {
            return Core::ERROR_NONE;
        }
    
    } // namespace Plugin
} // namespace WPEFramework
