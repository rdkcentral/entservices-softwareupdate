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

namespace WPEFramework {
    namespace Exchange {

        //This is the COM-RPC interface for the Mock Plugin
        //The requirement to make an interface a COMRPC interface is that it should inherit
        //from Core::IUnknown
        struct EXTERNAL IPowerManagerHal : virtual public Core::IUnknown {
            //TODO: to replace the magic number with ID_MOCK_PLUGIN in Ids.h at the completion of L2 OOP.
            enum { ID = 0xA31 };

            virtual uint32_t PLAT_INIT() = 0;
            virtual uint32_t PLAT_API_SetPowerState(uint32_t newState/* @in */) = 0;
            virtual uint32_t PLAT_API_GetPowerState(uint32_t &curState/* @out */) const = 0;
            virtual uint32_t PLAT_API_SetWakeupSrc(uint32_t srcType/* @in */, bool  enable/* @in */) = 0;
            virtual uint32_t PLAT_API_GetWakeupSrc(uint32_t srcType/* @in */, bool  &enable/* @out */) = 0;
            virtual uint32_t PLAT_Reset(uint32_t newState/* @in */) = 0;
            virtual uint32_t PLAT_TERM() = 0;

            virtual uint32_t PLAT_DS_INIT() = 0;
            virtual uint32_t PLAT_DS_SetDeepSleep(uint32_t deep_sleep_timeout/* @in */, bool& isGPIOWakeup/* @out */, bool networkStandby/* @in */) = 0;
            virtual uint32_t PLAT_DS_DeepSleepWakeup() = 0;
            virtual uint32_t PLAT_DS_GetLastWakeupReason(uint32_t& wakeupReason/* @out */) const = 0;
            virtual uint32_t PLAT_DS_GetLastWakeupKeyCode(uint32_t &keyCode/* @out */) const = 0;
            virtual uint32_t PLAT_DS_TERM() = 0;

            virtual ~IPowerManagerHal() = default;
        };
    }
}
