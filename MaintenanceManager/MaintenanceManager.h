/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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

#ifndef MAINTENANCEMANAGER_H
#define MAINTENANCEMANAGER_H

#include <stdint.h>
#include <thread>
#include <map>
#include <time.h>
#include <signal.h>
#include <dirent.h>

#include "Module.h"
#include "tracing/Logging.h"
#if defined(USE_IARMBUS) || defined(USE_IARM_BUS)
#include "libIARM.h"
#include "maintenanceMGR.h" /* IARM INTERFACE HELPER */
#endif /* USE_IARMBUS || USE_IARM_BUS */

#include "sysMgr.h"
#include "rfcapi.h"
#include "cSettings.h"

#include <interfaces/IAuthService.h>

/* ---- LOGGING ---- */
#ifdef ENABLE_JOURNAL_LOGGING
#include <systemd/sd-journal.h>
#define JOURNAL_IDENTIFIER "MaintenanceManager"
#define MM_FILE_NAME "MaintenanceManager.cpp"

#define MM_LOG(priority, priority_str, format, ...)                                 \
    sd_journal_send("MESSAGE=%s [%s:%d] %s: " format,                               \
                    priority_str, MM_FILE_NAME, __LINE__, __func__, ##__VA_ARGS__,  \
                    "PRIORITY=%i", priority,                                        \
                    "SYSLOG_IDENTIFIER=%s", JOURNAL_IDENTIFIER,                     \
                    NULL)

#define MM_LOGINFO(format, ...) MM_LOG(LOG_INFO, "INFO", format, ##__VA_ARGS__)
#define MM_LOGWARN(format, ...) MM_LOG(LOG_WARNING, "WARN", format, ##__VA_ARGS__)
#define MM_LOGERR(format, ...)  MM_LOG(LOG_ERR, "ERROR", format, ##__VA_ARGS__)

/* sendNotify() */
#define MM_SEND_NOTIFY(event, params)                \
{                                                    \
    std::string json;                                \
    params.ToString(json);                           \
    MM_LOGINFO("Notify %s %s", event, json.c_str()); \
}

/* returnResponse() */
#define MM_RETURN_RESPONSE(expr)                     \
{                                                    \
    bool s_Bool = expr;                              \
    response["success"] = s_Bool;                    \
    std::string json;                                \
    response.ToString(json);                         \
    MM_LOGINFO("response=%s", json.c_str());         \
}
#else /* else of ENABLE_JOURNAL_LOGGING */
#define MM_LOGINFO(format, ...) LOGINFO(format, ##__VA_ARGS__)
#define MM_LOGWARN(format, ...) LOGWARN(format, ##__VA_ARGS__)
#define MM_LOGERR(format, ...)  LOGERR(format, ##__VA_ARGS__)
#endif /* end of ENABLE_JOURNAL_LOGGING */

/* MaintenanceManager Services Triggered Events. */
#define EVT_ONMAINTMGRSAMPLEEVENT "onSampleEvent"
#define EVT_ONMAINTENANCSTATUSCHANGE "onMaintenanceStatusChange" /* Maintenance Status change */
/* we have a persistant file to hold the record */
#define MAINTENANCE_MGR_RECORD_FILE "/opt/maintenance_mgr_record.conf"

typedef enum
{
    MAINTENANCE_IDLE,
    MAINTENANCE_STARTED,
    MAINTENANCE_ERROR,
    MAINTENANCE_COMPLETE,
    MAINTENANCE_INCOMPLETE
} Maint_notify_status_t;

typedef enum
{
    ACTIVATION_CONNECT,
    ACTIVATION_READY,
    NOT_ACTIVATED,
    ACTIVATION_DISCONNECT,
    ACTIVATED,
    INVALID_ACTIVATION
} Auth_activation_status_t;

typedef enum
{
    SOLICITED_MAINTENANCE,
    UNSOLICITED_MAINTENANCE
} Maintenance_Type_t;

#define BASE_CLOCK CLOCK_BOOTTIME

#define WHOAMI_PROP_KEY "WHOAMI_SUPPORT"
#define DEVICE_PROP_FILE "/etc/device.properties"

#define FOREGROUND_MODE "FOREGROUND"
#define BACKGROUND_MODE "BACKGROUND"

#define TASKS_COMPLETED                 0x2A
#define ALL_TASKS_SUCCESS               0x3F
#define MAINTENANCE_TASK_SKIPPED        0x200

#if defined(GTEST_ENABLE)
#define MAX_NETWORK_RETRIES             1
#else
#define MAX_NETWORK_RETRIES             4
#endif
#define INTERNET_CONNECTED_STATE        3
#define NETWORK_RETRY_INTERVAL          30

#define MAX_ACTIVATION_RETRIES          4
#define SECMGR_RETRY_INTERVAL           5

#define TASK_RETRY_COUNT                1
#define TASK_RETRY_DELAY                5
#ifndef TASK_TIMEOUT
#define TASK_TIMEOUT                    3600 /* Default Task Timeout (1 Hour i.e. 3600 seconds)  */
#endif

#define RFC_SUCCESS                     0
#define RFC_COMPLETE                    1
#define LOGUPLOAD_SUCCESS               2
#define LOGUPLOAD_COMPLETE              3
#define SWUPDATE_SUCCESS                4
#define SWUPDATE_COMPLETE               5
#define REBOOT_REQUIRED                 6
#define TASK_SKIPPED                    7
#define TASKS_STARTED                   8

#define BUFFER_SIZE                     50

#define SET_STATUS(VALUE, N) ((VALUE) |= (1 << (N)))
#define CLEAR_STATUS(VALUE, N) ((VALUE) &= ~(1 << (N)))
#define CHECK_STATUS(VALUE, N) ((VALUE) & (1 << (N)))

namespace WPEFramework
{
    namespace Plugin
    {

        /* This is a server for a JSONRPC communication channel.
         * For a plugin to be capable to handle JSONRPC, inherit from PluginHost::JSONRPC.
         * By inheriting from this class, the plugin realizes the interface PluginHost::IDispatcher.
         * This realization of this interface implements, by default, the following methods on this plugin
         * - exists
         * - register
         * - unregister
         * Any other methood to be handled by this plugin  can be added can be added by using the
         * templated methods Register on the PluginHost::JSONRPC class.
         * As the registration/unregistration of notifications is realized by the class PluginHost::JSONRPC,
         * this class exposes a public method called, Notify(), using this methods, all subscribed clients
         * will receive a JSONRPC message as a notification, in case this method is called.
         */

        class MaintenanceManager : public PluginHost::IPlugin, public PluginHost::JSONRPC
        {
#if defined(GTEST_ENABLE)
        public:
#else
        private:
#endif
            typedef Core::JSON::String JString;
            typedef Core::JSON::ArrayType<JString> JStringArray;
            typedef Core::JSON::Boolean JBool;

            string g_currentMode;
            string g_is_critical_maintenance;
            string g_is_reboot_pending;
            string g_lastSuccessful_maint_time;
            string g_epoch_time;

            IARM_Bus_MaintMGR_EventData_t *g_maintenance_data;
            Maint_notify_status_t m_notify_status;
            Maintenance_Type_t g_maintenance_type;
            static cSettings m_setting;
            bool m_abort_flag;
            uint16_t g_task_status;
            bool g_unsolicited_complete;
            bool g_listen_to_nwevents = false;
            bool g_subscribed_for_nwevents = false;
            bool g_listen_to_deviceContextUpdate = false;
            bool g_subscribed_for_deviceContextUpdate = false;
            bool g_whoami_support_enabled = false;
#if defined(SUPPRESS_MAINTENANCE)
            bool g_suppress_maintenance_enabled = true;
#else
            bool g_suppress_maintenance_enabled = false;
#endif
            std::mutex m_callMutex;
            std::mutex m_waiMutex;
            std::mutex m_statusMutex;
            std::condition_variable task_thread;
            std::thread m_thread;

            std::map<string, bool> m_task_map;
            std::map<string, string> m_param_map;
            std::map<string, DATA_TYPE> m_paramType_map;

            PluginHost::IShell *m_service = nullptr;
            Exchange::IAuthService *m_authservicePlugin;

            bool isDeviceOnline();
            void task_execution_thread();
            void requestSystemReboot();
            void maintenanceManagerOnBootup();
            bool checkAutoRebootFlag();
            bool readRFC(const char *);
            bool setRFC(const char *, const char *, DATA_TYPE);
            void setPartnerId(string);
            WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement> *getThunderPluginHandle(const char *);
            bool stopMaintenanceTasks();
            bool subscribeForInternetStatusEvent(string);
            void internetStatusChangeEventHandler(const JsonObject &parameters);
            void deviceInitializationContextEventHandler(const JsonObject &parameters);
            void startCriticalTasks();
            bool checkNetwork();
            bool isWhoAmIEnabled();
            bool knowWhoAmI(string &activation_status);
            bool subscribeToDeviceInitializationEvent();
            bool setDeviceInitializationContext(JsonObject joGetResult);
            bool getActivatedStatus(bool &skipFirmwareCheck);
            const string checkActivatedStatus(void);
            bool queryIAuthService();
            int abortTask(const char *, int sig = SIGABRT);
            pid_t getTaskPID(const char *);
            string getLastRebootReason();
            void iarmEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
            static void _MaintenanceMgrEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
            /* We do not allow this plugin to be copied !! */
            MaintenanceManager(const MaintenanceManager &) = delete;
            MaintenanceManager &operator=(const MaintenanceManager &) = delete;

        private:
            class MaintenanceTask
            {
            private:
                std::string taskName;
                std::string taskScript;

            public:
                void startTask();
            };

        public:
            MaintenanceManager();
            virtual ~MaintenanceManager();

            static MaintenanceManager *_instance;
            virtual const string Initialize(PluginHost::IShell *service) override;
            virtual void Deinitialize(PluginHost::IShell *service) override;
            virtual string Information() const override { return {}; }
            static int runScript(const std::string &script, const std::string &args, string *output = NULL, string *error = NULL, int timeout = 30000);

            /* Timer Implementations */
            static void timer_handler(int signo);
            static timer_t timerid;
            static string currentTask;
            static bool g_task_timerCreated;

            bool maintenance_initTimer();
            bool task_startTimer();
            bool task_stopTimer();
            bool maintenance_deleteTimer();

            /* ---- Accessors ---- */
            bool testSetRFC(const char *rfc, const char *value, DATA_TYPE dataType) { return setRFC(rfc, value, dataType); }
            bool testReadRFC(const char *rfc) { return readRFC(rfc); }
            Maint_notify_status_t getNotifyStatus() { return m_notify_status; }
            void setNotifyStatus(Maint_notify_status_t status) { m_notify_status = status; }
            void testStartCriticalTasks() { startCriticalTasks(); }
            pid_t callGetTaskPID(const char *taskname) { return getTaskPID(taskname); }
            void callInternetStatusChangeEventHandler(const JsonObject &parameters) { internetStatusChangeEventHandler(parameters); }
            int callAbortTask(const char *taskname, int sig_to_send) { return abortTask(taskname, sig_to_send); }
            void setUnsolicitedComplete(bool value) { g_unsolicited_complete = value; }
            WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement> *PublicGetThunderPluginHandle(const char *callsign)
            {
                std::cout << "Inside PublicGetThunderPluginHandle" << std::endl;
                auto result = getThunderPluginHandle(callsign);
                std::cout << "Exiting PublicGetThunderPluginHandle" << std::endl;
                return result;
            }
            /* ---- Accessors ---- */
            BEGIN_INTERFACE_MAP(MaintenanceManager)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            END_INTERFACE_MAP

#if defined(USE_IARMBUS) || defined(USE_IARM_BUS)
            void InitializeIARM();
            void DeinitializeIARM();
#endif
            /* Events */
            void onMaintenanceStatusChange(Maint_notify_status_t status);

            /* Methods */
#ifdef DEBUG
            uint32_t sampleAPI(const JsonObject &parameters, JsonObject &response);
#endif /* DEBUG */

            uint32_t getMaintenanceActivityStatus(const JsonObject &parameters, JsonObject &response);
            uint32_t getMaintenanceStartTime(const JsonObject &parameters, JsonObject &response);
            uint32_t setMaintenanceMode(const JsonObject &parameters, JsonObject &response);
            uint32_t startMaintenance(const JsonObject &parameters, JsonObject &response);
            uint32_t stopMaintenance(const JsonObject &parameters, JsonObject &response);
            uint32_t getMaintenanceMode(const JsonObject &parameters, JsonObject &response);
        }; /* end of MaintenanceManager service class */
    } /* end of plugin */
} /* end of wpeframework */

#endif // MAINTENANCEMANAGER_H
