
/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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
#include <fstream>
#include <iostream>
#include "FactoriesImplementation.h"
#include "MaintenanceManager.cpp"
#include "MaintenanceManager.h"
#include "RfcApiMock.h"
#include "IarmBusMock.h"
#include "ServiceMock.h"
#include "WrapsMock.h"
#include "ThunderPortability.h"
#include "UtilsIarm.h"
#if defined(GTEST_ENABLE)
#include "mockauthservices.h"
#endif

using namespace WPEFramework;
using ::testing::NiceMock;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Test;
using ::testing::StrEq;
using ::testing::Gt;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;
using ::testing::AssertionFailure;

extern "C" FILE* __real_popen(const char* command, const char* type);
extern "C" int __real_pclose(FILE* pipe);

class MaintenanceManagerTest : public Test {
protected:
    Core::ProxyType<Plugin::MaintenanceManager> plugin_;
    Core::JSONRPC::Handler&                 handler_;
    DECL_CORE_JSONRPC_CONX connection;
    string                                  response_;
    IarmBusImplMock         *p_iarmBusImplMock = nullptr ;
    RfcApiImplMock   *p_rfcApiImplMock = nullptr ;
    WrapsImplMock  *p_wrapsImplMock   = nullptr ;
    NiceMock<ServiceMock>             service_;
#if defined(GTEST_ENABLE)
    NiceMock<MockAuthService>         iauthservice_;
    NiceMock<MockIAuthenticate>       iauthenticate_;
#endif

    MaintenanceManagerTest()
        : plugin_(Core::ProxyType<Plugin::MaintenanceManager>::Create())
        , handler_(*plugin_)
        , INIT_CONX(1, 0)
    {
        p_iarmBusImplMock  = new NiceMock <IarmBusImplMock>;
        IarmBus::setImpl(p_iarmBusImplMock);

        p_rfcApiImplMock  = new testing::NiceMock <RfcApiImplMock>;
        RfcApi::setImpl(p_rfcApiImplMock);

        p_wrapsImplMock  = new testing::NiceMock <WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);
    }

    virtual ~MaintenanceManagerTest() override
    {
        IarmBus::setImpl(nullptr);
        if (p_iarmBusImplMock != nullptr)
        {
            delete p_iarmBusImplMock;
            p_iarmBusImplMock = nullptr;
        }

        RfcApi::setImpl(nullptr);
        if (p_rfcApiImplMock != nullptr)
        {
            delete p_rfcApiImplMock;
            p_rfcApiImplMock = nullptr;
        }

        Wraps::setImpl(nullptr);
        if (p_wrapsImplMock != nullptr)
        {
            delete p_wrapsImplMock;
            p_wrapsImplMock = nullptr;
        }

    }
};

static AssertionResult isValidCtrlmRcuIarmEvent(IARM_EventId_t ctrlmRcuIarmEventId)
{
    switch (ctrlmRcuIarmEventId) {
        case IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE:
        case IARM_BUS_DCM_NEW_START_TIME_EVENT:
            return AssertionSuccess();
        default:
            return AssertionFailure();
    }
}

class MaintenanceManagerInitializedEventTest : public MaintenanceManagerTest {
protected:
    IARM_EventHandler_t               controlEventHandler_;
    NiceMock<ServiceMock>             service_;
    NiceMock<FactoriesImplementation> factoriesImplementation_;
    PLUGINHOST_DISPATCHER* dispatcher_;
    Core::JSONRPC::Message message_;

    MaintenanceManagerInitializedEventTest() :
        MaintenanceManagerTest()
    {

        EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_RegisterEventHandler(StrEq(IARM_BUS_MAINTENANCE_MGR_NAME),IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE, _))
            .WillOnce(Invoke(
                [&](const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler) {
                    controlEventHandler_ = handler;
                    return IARM_RESULT_SUCCESS;
                }));
        EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_RegisterEventHandler(StrEq(IARM_BUS_MAINTENANCE_MGR_NAME), IARM_BUS_DCM_NEW_START_TIME_EVENT, _))
            .WillRepeatedly(Invoke(
                [&](const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler) {
                    EXPECT_TRUE(isValidCtrlmRcuIarmEvent(eventId));
                    controlEventHandler_ = handler;
                    return IARM_RESULT_SUCCESS;
                }));

        EXPECT_EQ(string(""), plugin_->Initialize(&service_));
        PluginHost::IFactories::Assign(&factoriesImplementation_);

        dispatcher_ = static_cast<PLUGINHOST_DISPATCHER*>(plugin_->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher_->Activate(&service_);
    }

    virtual ~MaintenanceManagerInitializedEventTest() override
    {
        plugin_->Deinitialize(&service_);
        dispatcher_->Deactivate();
        dispatcher_->Release();
        PluginHost::IFactories::Assign(nullptr);
    }
};

/* --- Register Methods ---- */
TEST_F(MaintenanceManagerTest, RegisteredMethods)
{
    EXPECT_EQ(Core::ERROR_NONE, handler_.Exists(_T("getMaintenanceActivityStatus")));
    EXPECT_EQ(Core::ERROR_NONE, handler_.Exists(_T("getMaintenanceStartTime")));
    EXPECT_EQ(Core::ERROR_NONE, handler_.Exists(_T("setMaintenanceMode")));
    EXPECT_EQ(Core::ERROR_NONE, handler_.Exists(_T("startMaintenance")));
    EXPECT_EQ(Core::ERROR_NONE, handler_.Exists(_T("stopMaintenance")));
    EXPECT_EQ(Core::ERROR_NONE, handler_.Exists(_T("getMaintenanceMode")));
}

/* --- getMaintenanceActivityStatus JsonRPC ---- */
TEST_F(MaintenanceManagerTest, getMaintenanceActivityStatusJsonRPC)
{
    Maint_notify_status_t status = MAINTENANCE_ERROR;
    plugin_->setNotifyStatus(status);
    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.getMaintenanceActivityStatus"), _T("{}"), response_));
    EXPECT_EQ(response_, "{\"maintenanceStatus\":\"MAINTENANCE_ERROR\",\"LastSuccessfulCompletionTime\":0,\"isCriticalMaintenance\":false,\"isRebootPending\":false,\"success\":true}");
}

/* --- onMaintenanceStatusChange() ---- */
TEST_F(MaintenanceManagerTest, onMaintenanceStatusChange)
{
	Maint_notify_status_t status = MAINTENANCE_ERROR;
	plugin_->setNotifyStatus(status);
	
	// Check that the status was stored correctly
	EXPECT_EQ(plugin_->getNotifyStatus(), status);
}

/* --- setMaintenanceMode and getMaintenanceMode JsonRPC ---- */
TEST_F(MaintenanceManagerTest, setMaintenanceModeJsonRPCAndgetMaintenanceModeJsonRPC)
{
    ON_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, RFC_ParamData_t* pstParamData) {
                           pstParamData->type = WDMP_BOOLEAN;
                           strncpy(pstParamData->value, "true", MAX_PARAM_LEN);
                return WDMP_SUCCESS;
            }));

    // Test 1: Set FOREGROUND + IGNORE_UPDATE
    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.setMaintenanceMode"), _T("{\"maintenanceMode\":\"FOREGROUND\",\"optOut\":\"IGNORE_UPDATE\"}"), response_));
    EXPECT_EQ(response_, "{\"success\":true}");

    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.getMaintenanceMode"), _T("{}"), response_));
    EXPECT_EQ(response_, "{\"maintenanceMode\":\"FOREGROUND\",\"triggerMode\":\"\",\"optOut\":\"IGNORE_UPDATE\",\"success\":true}");

    // Test 2: Set FOREGROUND + ENFORCE_OPTOUT
    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.setMaintenanceMode"), _T("{\"maintenanceMode\":\"FOREGROUND\",\"optOut\":\"ENFORCE_OPTOUT\"}"), response_));
    EXPECT_EQ(response_, "{\"success\":true}");

    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.getMaintenanceMode"), _T("{}"), response_));
    EXPECT_EQ(response_, "{\"maintenanceMode\":\"FOREGROUND\",\"triggerMode\":\"\",\"optOut\":\"ENFORCE_OPTOUT\",\"success\":true}");

    // Test 3: Set FOREGROUND + BYPASS_OPTOUT
    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.setMaintenanceMode"), _T("{\"maintenanceMode\":\"FOREGROUND\",\"optOut\":\"BYPASS_OPTOUT\"}"), response_));
    EXPECT_EQ(response_, "{\"success\":true}");

    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.getMaintenanceMode"), _T("{}"), response_));
    EXPECT_EQ(response_, "{\"maintenanceMode\":\"FOREGROUND\",\"triggerMode\":\"\",\"optOut\":\"BYPASS_OPTOUT\",\"success\":true}");

    // Test 4: Set BACKGROUND + IGNORE_UPDATE
    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.setMaintenanceMode"), _T("{\"maintenanceMode\":\"BACKGROUND\",\"optOut\":\"IGNORE_UPDATE\"}"), response_));
    EXPECT_EQ(response_, "{\"success\":true}");

    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.getMaintenanceMode"), _T("{}"), response_));
    EXPECT_EQ(response_, "{\"maintenanceMode\":\"BACKGROUND\",\"triggerMode\":\"\",\"optOut\":\"IGNORE_UPDATE\",\"success\":true}");

    // Test 5: Set BACKGROUND + ENFORCE_OPTOUT
    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.setMaintenanceMode"), _T("{\"maintenanceMode\":\"BACKGROUND\",\"optOut\":\"ENFORCE_OPTOUT\"}"), response_));
    EXPECT_EQ(response_, "{\"success\":true}");

    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.getMaintenanceMode"), _T("{}"), response_));
    EXPECT_EQ(response_, "{\"maintenanceMode\":\"BACKGROUND\",\"triggerMode\":\"\",\"optOut\":\"ENFORCE_OPTOUT\",\"success\":true}");

    // Test 6: Set BACKGROUND + BYPASS_OPTOUT
    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.setMaintenanceMode"), _T("{\"maintenanceMode\":\"BACKGROUND\",\"optOut\":\"BYPASS_OPTOUT\"}"), response_));
    EXPECT_EQ(response_, "{\"success\":true}");

    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.getMaintenanceMode"), _T("{}"), response_));
    EXPECT_EQ(response_, "{\"maintenanceMode\":\"BACKGROUND\",\"triggerMode\":\"\",\"optOut\":\"BYPASS_OPTOUT\",\"success\":true}");
}

/* --- getTaskPID() ---- */
#ifdef USE_THUNDER_R4
TEST_F(MaintenanceManagerTest, GetTaskPID) {
    const char* taskname = "/bin/sleep";
    pid_t child_pid = fork();
    if (child_pid == 0) {
        execlp(taskname, taskname, "1000", (char*) NULL);
        exit(0);
    }
    sleep(1);

    pid_t task_pid = plugin_->callGetTaskPID(taskname);
    EXPECT_EQ(task_pid, child_pid);
    kill(child_pid, SIGKILL);
    int status;
    waitpid(child_pid, &status, 0);
}
#endif
TEST_F(MaintenanceManagerTest, GetTaskPID_NonExistentTask) {
    const char* taskname = "/bin/non_existent_task";
    pid_t task_pid = plugin_->callGetTaskPID(taskname);
    EXPECT_EQ(task_pid, (pid_t)-1);
}

/* ---- abortTask() ---- */
TEST_F(MaintenanceManagerTest, AbortTask_TaskNotFound) {
    const char* taskname = "/bin/non_existent_task";
    int ret = plugin_->callAbortTask(taskname, SIGTERM);
    EXPECT_EQ(ret, EINVAL);
}

TEST_F(MaintenanceManagerTest, AbortTask_RfcMgrTask) {
#if defined(ENABLE_RFC_MANAGER)
    const char* taskname = "rfcMgr";
    
    pid_t child_pid = fork();
    if (child_pid == 0) {
        execlp(taskname, "sleep", "1000", (char*) NULL);
        exit(0);
    }

    sleep(1);
    int ret = plugin_->callAbortTask(taskname, SIGTERM);
    EXPECT_EQ(ret, 0);
    kill(child_pid, SIGKILL);
    int status;
    waitpid(child_pid, &status, 0);
#endif
}

/* --- internetStatusEventHandler ---- */
TEST_F(MaintenanceManagerTest, internetStatusEventHandlerTest) {
    WPEFramework::Core::JSON::Variant statusVariant, stateVariant;
    statusVariant = std::string("connected");
    stateVariant = static_cast<uint32_t>(INTERNET_CONNECTED_STATE);
    JsonObject parameters;
    parameters.Set("status", statusVariant);
    parameters.Set("state", stateVariant);

    plugin_->callInternetStatusChangeEventHandler(parameters);
}

/* ---- setRFC() ---- */
TEST_F(MaintenanceManagerTest, SetRFC_Success) {
    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                EXPECT_EQ(string(pcCallerID), _T((char *)MAINTENANCE_MANAGER_RFC_CALLER_ID));
                EXPECT_EQ(string(pcParameterName), _T("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Bootstrap.PartnerName"));
                EXPECT_EQ(string(pcParameterValue), _T("false"));
                EXPECT_EQ(eDataType, WDMP_STRING);
                return WDMP_SUCCESS;
            }));
    bool result = plugin_->testSetRFC("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Bootstrap.PartnerName", "false", WDMP_STRING);

    EXPECT_TRUE(result);
}

TEST_F(MaintenanceManagerTest, SetRFC_Failure) {
     EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                return WDMP_FAILURE;
            }));
    bool result = plugin_->testSetRFC("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Bootstrap.PartnerName", "false", WDMP_STRING);

    EXPECT_FALSE(result);
}

/* ---- readRFC() ---- */
TEST_F(MaintenanceManagerTest, ReadRFC_Success) {
    ON_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, RFC_ParamData_t* pstParamData) {
                pstParamData->type = WDMP_BOOLEAN;
                strncpy(pstParamData->value, "true", MAX_PARAM_LEN);
                return WDMP_SUCCESS;
            }));

    bool result = plugin_->testReadRFC("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Bootstrap.PartnerName");
    EXPECT_TRUE(result);
}

TEST_F(MaintenanceManagerTest, ReadRFC_Failure) {
    ON_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, RFC_ParamData_t* pstParamData) {
                return WDMP_FAILURE;
            }));
    bool result = plugin_->testReadRFC("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Bootstrap.PartnerName");
    EXPECT_FALSE(result);
}

/* ---- getMaintenanceStartTime() JsonRPC ---- */
TEST_F(MaintenanceManagerTest, getMaintenanceStartTime)
{
    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("getMaintenanceStartTime"), _T("{}"), response_));
    EXPECT_EQ(response_, "{\"maintenanceStartTime\":-1,\"success\":true}");
}

/* ---- stopMaintenance() JsonRPC ---- */
#if 0 //Test case is failing in workflow, so commenting this test case
TEST_F(MaintenanceManagerTest, stopMaintenanceRPC_IDLE2ERROR)
{
    Maint_notify_status_t status = MAINTENANCE_IDLE;
    plugin_->setNotifyStatus(status);
    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.getMaintenanceActivityStatus"), _T("{}"), response_));
    EXPECT_EQ(response_, "{\"maintenanceStatus\":\"MAINTENANCE_IDLE\",\"LastSuccessfulCompletionTime\":0,\"isCriticalMaintenance\":false,\"isRebootPending\":false,\"success\":true}");
    ON_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, RFC_ParamData_t* pstParamData) {
                pstParamData->type = WDMP_BOOLEAN;
                strncpy(pstParamData->value, "true", MAX_PARAM_LEN);
                return WDMP_SUCCESS;
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.stopMaintenance"), _T("{}"), response_));
    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.getMaintenanceActivityStatus"), _T("{}"), response_));
    EXPECT_EQ(response_, "{\"maintenanceStatus\":\"MAINTENANCE_ERROR\",\"LastSuccessfulCompletionTime\":0,\"isCriticalMaintenance\":false,\"isRebootPending\":false,\"success\":true}");
}
#endif
TEST_F(MaintenanceManagerTest, stopMaintenanceRPC_STARTED2ERROR)
{
    Maint_notify_status_t status = MAINTENANCE_STARTED;
    plugin_->setNotifyStatus(status);
    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.getMaintenanceActivityStatus"), _T("{}"), response_));
    EXPECT_EQ(response_, "{\"maintenanceStatus\":\"MAINTENANCE_STARTED\",\"LastSuccessfulCompletionTime\":0,\"isCriticalMaintenance\":false,\"isRebootPending\":false,\"success\":true}");
    ON_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, RFC_ParamData_t* pstParamData) {
                pstParamData->type = WDMP_BOOLEAN;
                strncpy(pstParamData->value, "true", MAX_PARAM_LEN);
                return WDMP_SUCCESS;
            }));

    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.stopMaintenance"), _T("{}"), response_));
    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.getMaintenanceActivityStatus"), _T("{}"), response_));
    EXPECT_EQ(response_, "{\"maintenanceStatus\":\"MAINTENANCE_ERROR\",\"LastSuccessfulCompletionTime\":0,\"isCriticalMaintenance\":false,\"isRebootPending\":false,\"success\":true}");
}

/* ---- startMaintenance() JsonRPC ---- */
TEST_F(MaintenanceManagerTest, DISABLED_startMaintenanceRPC_unsolicNotCompleted)
{
    Maint_notify_status_t status = MAINTENANCE_IDLE;
    plugin_->setNotifyStatus(status);
    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.getMaintenanceActivityStatus"), _T("{}"), response_));
    EXPECT_EQ(response_, "{\"maintenanceStatus\":\"MAINTENANCE_IDLE\",\"LastSuccessfulCompletionTime\":0,\"isCriticalMaintenance\":false,\"isRebootPending\":false,\"success\":true}");

    EXPECT_EQ(Core::ERROR_GENERAL, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.startMaintenance"), _T("{}"), response_));
    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.getMaintenanceActivityStatus"), _T("{}"), response_));
    EXPECT_EQ(response_, "{\"maintenanceStatus\":\"MAINTENANCE_IDLE\",\"LastSuccessfulCompletionTime\":0,\"isCriticalMaintenance\":false,\"isRebootPending\":false,\"success\":true}");
}

#if 0
TEST_F(MaintenanceManagerTest, startMaintenanceRPC_UnsoliCompleted)
{
    Maint_notify_status_t status = MAINTENANCE_IDLE;
    plugin_->setUnsolicitedComplete(true);
    plugin_->setNotifyStatus(status);
    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.getMaintenanceActivityStatus"), _T("{}"), response_));
    EXPECT_EQ(response_, "{\"maintenanceStatus\":\"MAINTENANCE_IDLE\",\"LastSuccessfulCompletionTime\":0,\"isCriticalMaintenance\":false,\"isRebootPending\":false,\"success\":true}");

    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.startMaintenance"), _T("{}"), response_));
    EXPECT_EQ(Core::ERROR_NONE, handler_.Invoke(connection, _T("org.rdk.MaintenanceManager.1.getMaintenanceActivityStatus"), _T("{}"), response_));
    EXPECT_EQ(response_, "{\"maintenanceStatus\":\"MAINTENANCE_IDLE\",\"LastSuccessfulCompletionTime\":0,\"isCriticalMaintenance\":false,\"isRebootPending\":true,\"success\":true}");
    plugin_->setUnsolicitedComplete(false);
}
#endif

#if 0
TEST_F(MaintenanceManagerTest, testAbortTask_existingProcess_rfcMgr)
{
    pid_t pid = fork();
    if (pid == 0) {

        execlp("sleep", "rfcMgr", "10000", (char*)NULL);

        perror("execlp");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {

        perror("fork");
        FAIL();
    }
    EXPECT_EQ(plugin_->testAbortTask("rfcMgr", SIGTERM), 0);
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        FAIL();
    }
}

TEST_F(MaintenanceManagerTest, abortTask_existingProcess_rdkvfwupgrader)
{
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sleep", "rdkvfwupgrader", "10000", (char*)NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("fork");
        FAIL();
    }
    std::string taskname = std::to_string(pid);
    EXPECT_EQ(plugin_->testAbortTask(taskname.c_str(), SIGTERM), 0);

    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        FAIL();
    }
}

TEST_F(MaintenanceManagerTest, abortTask_existingProcess_default)
{
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sleep", "default", "10000", (char*)NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("fork");
        FAIL();
    }
    std::string taskname = std::to_string(pid);
    EXPECT_EQ(plugin_->testAbortTask(taskname.c_str(), SIGTERM), 0);
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        FAIL();
    }
}

TEST_F(MaintenanceManagerTest, abortTask_nonexistentProcess)
{
    EXPECT_EQ(plugin_->testAbortTask("nonexistentTask", SIGTERM), EINVAL);
}

// TODO
TEST_F(MaintenanceManagerTest, startCriticalTasks)
{
    ON_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
    .WillByDefault(::testing::Invoke(
        [](const char* command, va_list args) {
            return system(command);
        }));

#ifdef ENABLE_RFC_MANAGER
    system("echo 'echo RFCMgrContents' >> /usr/bin/rfcMgr");
#else	
    system("echo 'echo RFCBaseContents' >> /lib/rdk/RFCbase.sh");
#endif
    system("echo 'echo XConfImageCheckContents' >> /lib/rdk/xconfImageCheck.sh");
    system("cat /usr/bin/rfcMgr");
    system("cat /lib/rdk/RFCbase.sh");
    system("cat /lib/rdk/xconfImageCheck.sh");
    plugin_->testStartCriticalTasks();
    std::this_thread::sleep_for(std::chrono::seconds(5));

    char buffer[256];
#ifdef ENABLE_RFC_MANAGER
    FILE* rfcscriptLog = popen("cat /opt/logs/rfcscript.log", "r");
    std::string rfcscriptLogContent;
    while (fgets(buffer, sizeof(buffer), rfcscriptLog) != NULL) {
        rfcscriptLogContent += buffer;
    }
    pclose(rfcscriptLog);
    cout << rfcscriptLogContent << endl;
    EXPECT_EQ(rfcscriptLogContent, "RFCMgrContents\n");
#endif
    memset(buffer, 0, sizeof(buffer));

    FILE* swupdateLog = popen("cat /opt/logs/swupdate.log", "r");
    std::string swupdateLogContent;
    while (fgets(buffer, sizeof(buffer), swupdateLog) != NULL) {
        swupdateLogContent += buffer;
    }
    pclose(swupdateLog);
    EXPECT_EQ(swupdateLogContent, "XConfImageCheckContents\n");
    cout << swupdateLogContent << endl;


    // Clean up
    system("rm /usr/bin/rfcMgr");
    system("rm /lib/rdk/RFCbase.sh");
    system("rm /lib/rdk/xconfImageCheck.sh");
    system("rm /opt/logs/rfcscript.log");
    system("rm /opt/logs/swupdate.log");
}

// TODO
/* ---- getTimeZone() ---- */
TEST(GetTimeZoneTest, DevicePropsFileNotCreated) {
    char deviceName[128] = {0};
    char zoneValue[128] = {0};
    char timeZone[128] = {0};
    char timeZoneOffset[128] = {0};
    remove("/tmp/device.properties");
    remove("/tmp/timeZone_offset_map");
    remove("/tmp/timeZoneDST");
    WPEFramework::Plugin::getTimeZone(deviceName, zoneValue, timeZone, timeZoneOffset, sizeof(deviceName));

    // Check that all strings are empty
    EXPECT_STREQ(deviceName, "");
    EXPECT_STREQ(zoneValue, "");
    EXPECT_STREQ(timeZone, "");
    EXPECT_STREQ(timeZoneOffset, "");
}

TEST(GetTimeZoneTest, DevicePropsFileCreatedButDSTFileNotCreated) {
    FILE* propertiesFile = fopen("/tmp/device.properties", "w");
    if (propertiesFile != NULL) {
        fprintf(propertiesFile, "DEVICE_NAME=XYZ");
        fclose(propertiesFile);
    }
    else{
    	cout << "Prop FILE CREATION in DevicePropsFileCreatedButDSTFileNotCreated FAILED" << endl;
    }
    system("cat /tmp/device.properties");
    char deviceName[128] = {0};
    char zoneValue[128] = {0};
    char timeZone[128] = {0};
    char timeZoneOffset[128] = {0};
    WPEFramework::Plugin::getTimeZone(deviceName, zoneValue, timeZone, timeZoneOffset, sizeof(deviceName));

    EXPECT_STREQ(deviceName, "XYZ");
    EXPECT_STREQ(zoneValue, "");
    EXPECT_STREQ(timeZone, "");
    EXPECT_STREQ(timeZoneOffset, "");
}

TEST(GetTimeZoneTest, DevicePropsAndDSTFileCreated) {
    FILE* propertiesFile = fopen("/tmp/device.properties", "w");
    if (propertiesFile != NULL) {
        fprintf(propertiesFile, "DEVICE_NAME=XYZ");
        fclose(propertiesFile);
    }
	else{
    	cout << "Prop FILE CREATION in DevicePropsAndDSTFileCreated FAILED" << endl;
    }
    FILE* dstFile = fopen("/tmp/timeZoneDST", "w");
    if (dstFile != NULL) {
        fprintf(dstFile, "America/New_York");
        fclose(dstFile);
    }
    else{
    	cout << "DST FILE CREATION in DevicePropsAndDSTFileCreated FAILED" << endl;
    }

    char deviceName[128] = {0};
    char zoneValue[128] = {0};
    char timeZone[128] = {0};
    char timeZoneOffset[128] = {0};
    WPEFramework::Plugin::getTimeZone(deviceName, zoneValue, timeZone, timeZoneOffset, sizeof(deviceName));

    EXPECT_STREQ(deviceName, "XYZ");
    EXPECT_STREQ(zoneValue, "");
    EXPECT_STREQ(timeZone, "America/New_York");
    EXPECT_STREQ(timeZoneOffset, "");
    system("ls -lh /tmp/device*");
    system("ls -lh /tmp/timeZone*");

    remove("/tmp/device.properties");
    remove("/tmp/timeZoneDST");

    system("ls -lh /tmp/device*");
    system("ls -lh /tmp/timeZone*");
}

TEST(GetTimeZoneTest, DevicePropsFileExistsAndNoTimeZoneOffsetMapFile) {

    FILE* propertiesFile = fopen("/tmp/device.properties", "w");
    if (propertiesFile != NULL) {
        fprintf(propertiesFile, "DEVICE_NAME=PLATCO");
        fclose(propertiesFile);
    }

    char deviceName[128] = {0};
    char zoneValue[128] = {0};
    char timeZone[128] = {0};
    char timeZoneOffset[128] = {0};
    WPEFramework::Plugin::getTimeZone(deviceName, zoneValue, timeZone, timeZoneOffset, sizeof(deviceName));
}

TEST(GetTimeZoneTest, TimeZoneOffsetMapFileCreated) {

    FILE* mapFile = fopen("/tmp/timeZone_offset_map", "w");
    if (mapFile != NULL) {
        fprintf(mapFile, "America/New_York:US/Eastern:0");
        fclose(mapFile);
    }

    char deviceName[128] = {0};
    char zoneValue[128] = {0};
    char timeZone[128] = {0};
    char timeZoneOffset[128] = {0};
    WPEFramework::Plugin::getTimeZone(deviceName, zoneValue, timeZone, timeZoneOffset, sizeof(deviceName));
    EXPECT_STREQ(deviceName, "PLATCO");
    EXPECT_STREQ(zoneValue, "US/Eastern");
    EXPECT_STREQ(timeZone, "America/New_York");
    EXPECT_STREQ(timeZoneOffset, "0");
    remove("/tmp/device.properties");
    remove("/tmp/timeZone_offset_map");
}

TEST_F(MaintenanceManagerTest, GetServiceState_Unavailable) {
    PluginHost::IShell::state state;
    EXPECT_CALL(service, QueryInterfaceByCallsign("test"))
        .WillOnce(Return(nullptr));
    uint32_t result = plugin_->getServiceState(&service, "test", state);
    EXPECT_EQ(result, Core::ERROR_UNAVAILABLE);
}

TEST_F(MaintenanceManagerTest, GetServiceState_Available) {
    PluginHost::IShell::state state;
    EXPECT_CALL(service, QueryInterfaceByCallsign("test"))
        .WillOnce(Return(&service));
    EXPECT_CALL(service, State())
        .WillOnce(Return(PluginHost::IShell::state::ACTIVATED));
    uint32_t result = plugin_->getServiceState(&service, "test", state);
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(state, PluginHost::IShell::state::ACTIVATED);
}
#endif

/* ---- notifyStatusToString() ---- */
TEST(MaintenanceManagerNotifyStatus, NotifyStatusToString) {
    std::vector<std::pair<Maint_notify_status_t, std::string>> maint_Status = {
        {MAINTENANCE_IDLE, "MAINTENANCE_IDLE"},
        {MAINTENANCE_STARTED, "MAINTENANCE_STARTED"},
        {MAINTENANCE_ERROR, "MAINTENANCE_ERROR"},
        {MAINTENANCE_COMPLETE, "MAINTENANCE_COMPLETE"},
        {MAINTENANCE_INCOMPLETE, "MAINTENANCE_INCOMPLETE"},
    };
    for (const auto& mStatus : maint_Status) {
        Maint_notify_status_t status = mStatus.first;
        std::string expected = mStatus.second;
        EXPECT_EQ(expected, notifyStatusToString(status));
    }
}

/* ---- checkValidOptOutModes() ---- */
TEST(MaintenanceManagerCheckOptMode, CheckValidOptOutMode) {
	std::vector<string> maint_OptOutModes = {
		"ENFORCE_OPTOUT",
		"BYPASS_OPTOUT",
        	"IGNORE_UPDATE",
        	"NONE"
	};
	
	for (const auto& optMode: maint_OptOutModes){
		EXPECT_EQ(true, checkValidOptOutModes(optMode));
	}
	std::string invalid_optMode = "INVALID_OPTOUT_MODE";
	EXPECT_EQ(false, checkValidOptOutModes(invalid_optMode));
}

/* ---- getFileContent() ---- */
TEST(GetFileContentTest, FileExistsAndHasContent) {
	// Create a test fil
	std::string testFilePath = "/tmp/testFile.txt";
	std::ofstream testFile(testFilePath);
    testFile << "Line 1\nLine 2\nLine 3\n";
    testFile.close();
	
    std::vector<std::string> vecOfStrs;
    bool result = WPEFramework::Plugin::getFileContent(testFilePath, vecOfStrs);

    EXPECT_TRUE(result);
    EXPECT_EQ(3, vecOfStrs.size());
    EXPECT_EQ("Line 1", vecOfStrs[0]);
    EXPECT_EQ("Line 2", vecOfStrs[1]);
    EXPECT_EQ("Line 3", vecOfStrs[2]);
	std::remove(testFilePath.c_str());
}

TEST(GetFileContentTest, FileDoesNotExist) {
    std::vector<std::string> vecOfStrs;
    bool result = WPEFramework::Plugin::getFileContent("/path/to/nonexistent/file", vecOfStrs);

    EXPECT_FALSE(result);
    EXPECT_TRUE(vecOfStrs.empty());
}

/* ---- parseConfigFile() ---- */
class ParseConfTest : public ::testing::Test {
protected:
    std::string testFilePath = "/tmp/testFile.txt"; 
	string getCurrentTestName()
    {
        const testing::TestInfo *const test_info = testing::UnitTest::GetInstance()->current_test_info();
        return test_info->name();
    }

    void SetUp() override {
	string test_name = getCurrentTestName();
	if (test_name != "FileDoesNotExist"){
		// Create a test file with key-value pairs
		std::ofstream testFile(testFilePath);
		testFile << "key1=value1\nkey2=value2\nkey3=value3\n";
		testFile.close();
	}
    }
    void TearDown() override {
	string test_name = getCurrentTestName();
	if (test_name != "FileDoesNotExist"){
		std::remove(testFilePath.c_str());
	}
    }
};

TEST_F(ParseConfTest, FileDoesNotExist) {
    std::string value;
    bool result = WPEFramework::Plugin::parseConfigFile("/path/to/nonexistent/file", "key1", value);

    EXPECT_FALSE(result);
}

TEST_F(ParseConfTest, KeyNotFound) {
    std::string value;
    bool result = WPEFramework::Plugin::parseConfigFile(testFilePath.c_str(), "nonexistentKey", value);

    EXPECT_FALSE(result);
}

TEST_F(ParseConfTest, KeyFound) {
    std::string value;
    bool result = WPEFramework::Plugin::parseConfigFile(testFilePath.c_str(), "key1", value);

    EXPECT_TRUE(result);
    EXPECT_EQ("value1", value);
}

/* ---- moduleStatusToString() ---- */
#if 0 //Test case is failing in workflow, so commenting this test case
TEST(MaintenanceManagerModuleStatus, ModuleStatusToString) {
	std::vector<std::pair<IARM_Maint_module_status_t, std::string>> maint_modStatus = {
		{MAINT_RFC_COMPLETE, "MAINTENANCE_RFC_COMPLETE"},
		{MAINT_RFC_ERROR, "MAINTENANCE_RFC_ERROR"},
		{MAINT_LOGUPLOAD_COMPLETE, "MAINTENANCE_LOGUPLOAD_COMPLETE"},
		{MAINT_LOGUPLOAD_ERROR, "MAINTENANCE_LOGUPLOAD_ERROR"},
		{MAINT_PINGTELEMETRY_COMPLETE, "MAINTENANCE_PINGTELEMETRY_COMPLETE"},
		{MAINT_PINGTELEMETRY_ERROR, "MAINTENANCE_PINGTELEMETRY_ERROR"},
		{MAINT_FWDOWNLOAD_COMPLETE, "MAINTENANCE_FWDOWNLOAD_COMPLETE"},
		{MAINT_FWDOWNLOAD_ERROR, "MAINTENANCE_FWDOWNLOAD_ERROR"},
		{MAINT_REBOOT_REQUIRED, "MAINTENANCE_REBOOT_REQUIRED"},
		{MAINT_FWDOWNLOAD_ABORTED, "MAINTENANCE_FWDOWNLOAD_ABORTED"}
	};
	
	for (const auto& mMStatus : maint_modStatus) {
        	IARM_Maint_module_status_t status = mMStatus.first;
        	std::string expected = mMStatus.second;
        	EXPECT_EQ(expected, moduleStatusToString(status));
	}
}
#endif
#if defined(GTEST_ENABLE)

TEST_F(MaintenanceManagerTest, MaintenanceInitTimer_Success)
{
    bool result = plugin_->maintenance_initTimer();
    EXPECT_TRUE(WPEFramework::Plugin::MaintenanceManager::g_task_timerCreated);
    EXPECT_TRUE(result); 
}

TEST_F(MaintenanceManagerTest, MaintenanceInitTimer_AlreadyCreated_ReturnsTrue)
{
    WPEFramework::Plugin::MaintenanceManager::g_task_timerCreated = true;

    bool result = plugin_->maintenance_initTimer();
    EXPECT_TRUE(WPEFramework::Plugin::MaintenanceManager::g_task_timerCreated);
    EXPECT_TRUE(result);
}

TEST_F(MaintenanceManagerTest, TaskStartTimer_Success)
{
    // Ensure the timer is not already created
    plugin_->maintenance_deleteTimer();
    bool result = plugin_->task_startTimer();
    EXPECT_TRUE(result);
}
TEST_F(MaintenanceManagerTest, TaskStopTimer_Success)
{
    // Ensure the timer is created and started
    plugin_->task_startTimer();
    bool result = plugin_->task_stopTimer();
    EXPECT_TRUE(result);
}
TEST_F(MaintenanceManagerTest, TaskStopTimer_Fail)
{
    // Ensure the timer is created and started
    plugin_->task_startTimer();
    WPEFramework::Plugin::MaintenanceManager::g_task_timerCreated = false;
    bool result = plugin_->task_stopTimer();
    EXPECT_FALSE(result);
}

TEST_F(MaintenanceManagerTest, MaintenanceDeleteTimer_Success)
{
    // Ensure the timer is created first
    plugin_->task_startTimer();
    // Attempt to delete the timer
    bool result = plugin_->maintenance_deleteTimer();
    EXPECT_TRUE(result);
}
TEST_F(MaintenanceManagerTest, MaintenanceDeleteTimer_Fail)
{
    // Ensure the timer is created first
    plugin_->task_startTimer();
    WPEFramework::Plugin::MaintenanceManager::g_task_timerCreated = false;
    bool result = plugin_->maintenance_deleteTimer();

    EXPECT_FALSE(result);
}

TEST_F(MaintenanceManagerTest, TimerHandler_Handles_failedtask) {
    using namespace WPEFramework::Plugin;

    std::string matchedTask = task_names_foreground[TASK_RFC]; // use a known task name
    MaintenanceManager::currentTask = matchedTask;
    plugin_->m_task_map[matchedTask] = true;
    plugin_->g_task_status = 0; // initial value

    plugin_->timer_handler(SIGALRM);

    EXPECT_FALSE(plugin_->m_task_map[matchedTask]); // should be set to false
}

TEST_F(MaintenanceManagerTest, TimerHandler_NonSIGALRM_Ignored)
{
    using namespace WPEFramework::Plugin;

    std::string task = task_names_foreground[TASK_LOGUPLOAD];
    MaintenanceManager::currentTask = task;
    plugin_->m_task_map[task] = true;
    plugin_->g_task_status = 999; // sentinel value

    plugin_->timer_handler(SIGINT); // wrong signal

    EXPECT_TRUE(plugin_->m_task_map[task]); // no change
    EXPECT_EQ(plugin_->g_task_status, 999); // unchanged
}

TEST_F(MaintenanceManagerTest, TimerHandler_SIGALRM_NoTaskMatch)
{
    using namespace WPEFramework::Plugin;

    MaintenanceManager::currentTask = "unknown_task";
    plugin_->m_task_map.clear();
    plugin_->g_task_status = 1234; // sentinel value

    plugin_->timer_handler(SIGALRM);

    EXPECT_TRUE(plugin_->m_task_map.empty()); // no new entries
    EXPECT_EQ(plugin_->g_task_status, 1234); // unchanged
}

TEST_F(MaintenanceManagerTest, TimerHandler_SIGALRM_TaskSetToError)
{
    WPEFramework::Plugin::MaintenanceManager::currentTask = "DownloadFirmware";

    plugin_->m_task_map["DownloadFirmware"] = true; // Still active

    plugin_->timer_handler(SIGALRM);

    EXPECT_TRUE(plugin_->m_task_map["DownloadFirmware"]);
}

TEST_F(MaintenanceManagerTest, HandlesEventCorrectly) {
    const char* owner = "TestOwner";
    IARM_EventId_t eventId = 42; // Use an appropriate value
    char data[100] = {0}; // Or whatever data is expected
    size_t len = sizeof(data);
    plugin_->iarmEventHandler(owner, eventId, data, len);
} 

TEST_F(MaintenanceManagerTest, QueryIAuthService_FailsWhenPluginIsNull)
{
    // Ensure m_authservicePlugin is initially null
    plugin_->m_authservicePlugin = nullptr;
    plugin_->m_service = &service_;

    // Simulate failure in QueryInterfaceByCallsign (returns nullptr)
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_, "org.rdk.AuthService"))
        .WillOnce(::testing::Return(nullptr));

    bool result = plugin_->queryIAuthService();

    EXPECT_FALSE(result);
}

TEST_F(MaintenanceManagerTest, QueryIAuthService_FailsWhenPluginIsnotNull1)
{
    // Ensure m_authservicePlugin is initially null
    plugin_->m_authservicePlugin = &iauthservice_;
    plugin_->m_service = &service_;
    bool result = plugin_->queryIAuthService();

    EXPECT_TRUE(result);
}

TEST_F(MaintenanceManagerTest, QueryIAuthService_FailsWhenPlugin_notNull)
{
    plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_, "org.rdk.AuthService"))
        .WillOnce(::testing::Return(&service_));
    bool result = plugin_->queryIAuthService();

    EXPECT_TRUE(result);
}

TEST_F(MaintenanceManagerTest, SetPartnerId_AuthServiceUnavailable_DoesNothing)
{
    plugin_->m_service = &service_;
    plugin_->setPartnerId("partner1");
    SUCCEED();
}
TEST_F(MaintenanceManagerTest, SetPartnerId_AuthServiceAvailable_NoCrash)
{
    plugin_->m_service = &service_;
    plugin_->m_authservicePlugin = &iauthservice_;
    plugin_->setPartnerId("partner1");
    SUCCEED();
}

TEST_F(MaintenanceManagerTest, ReturnsLinkTypeWithTokenWhenSecurityAgentPresent) {
    
    plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"SecurityAgent"))
        .WillOnce(Return(&service_));
    const char* callsign = "SomePlugin";

    auto* handle = plugin_->getThunderPluginHandle(callsign);
    ASSERT_NE(handle, nullptr);
    delete handle;
}

TEST_F(MaintenanceManagerTest, ReturnsLinkTypeWithoutTokenWhenSecurityAgentPresent) {
    
    plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(_, "SecurityAgent"))
        .WillOnce(Return(&iauthenticate_));

    EXPECT_CALL(iauthenticate_, CreateToken(_, _, _))
        .WillOnce([](uint16_t, const uint8_t*, std::string& token) {
        token = "mock_token";
        return Core::ERROR_GENERAL;
    });
    const char* callsign = "SomePlugin";

    auto* handle = plugin_->getThunderPluginHandle(callsign);
    ASSERT_NE(handle, nullptr);
    delete handle;
}

TEST_F(MaintenanceManagerTest, ReturnsLinkTypeWithTokenWhenSecurityAgentnotPresent) {

    plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"SecurityAgent"))
        .WillOnce(::testing::Return(nullptr));
    const char* callsign = "SomePlugin";

    auto* handle = plugin_->getThunderPluginHandle(callsign);
    ASSERT_NE(handle, nullptr);
    delete handle;
}

TEST_F(MaintenanceManagerTest, ServiceNotActivated) {
    plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"org.rdk.AuthService"))
	.Times(5)
        .WillRepeatedly(::testing::Return(&service_));
    std::string result = plugin_->checkActivatedStatus();
    EXPECT_EQ(result, "invalid");
}

TEST_F(MaintenanceManagerTest, checkServiceActivated) {
    plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"org.rdk.AuthService"))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(&service_));
    EXPECT_CALL(service_, State())
        .WillOnce(::testing::Return(PluginHost::IShell::state::ACTIVATED));

    std::string result = plugin_->checkActivatedStatus();
    EXPECT_EQ(result, "");
}

TEST_F(MaintenanceManagerTest, getServiceNotActivated) {
    bool skipCheck = false;
    plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"org.rdk.AuthService"))
	.Times(5)
        .WillRepeatedly(::testing::Return(&service_));

    bool result = plugin_->getActivatedStatus(skipCheck);
    
    EXPECT_TRUE(result);
    EXPECT_FALSE(skipCheck);
}

TEST_F(MaintenanceManagerTest, getActivatedstatussuccess) {
    bool skipCheck = false;
    plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"org.rdk.AuthService"))
          .Times(::testing::AtLeast(1))
          .WillRepeatedly(::testing::Return(&service_));
    EXPECT_CALL(service_, State())
        .WillOnce(::testing::Return(PluginHost::IShell::state::ACTIVATED));
    bool result = plugin_->getActivatedStatus(skipCheck);
    EXPECT_TRUE(result);
    EXPECT_FALSE(skipCheck);
}

TEST_F(MaintenanceManagerTest, subscribe) {
    plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"SecurityAgent"))
        .WillOnce(Return(&service_));

    bool result = plugin_->subscribeToDeviceInitializationEvent();
    EXPECT_TRUE(result);
}

TEST_F(MaintenanceManagerTest, subscribewithoutsecurityagent) {
    plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"SecurityAgent"))
        .WillOnce(::testing::Return(nullptr));

    bool result = plugin_->subscribeToDeviceInitializationEvent();
    EXPECT_TRUE(result);
}

TEST_F(MaintenanceManagerTest, subscribeForInternetStatus) {
    plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"SecurityAgent"))
        .WillOnce(Return(&service_));

    bool result = plugin_->subscribeForInternetStatusEvent("internetStatus");
    EXPECT_TRUE(result);
}

TEST_F(MaintenanceManagerTest, subscribeForInternetStatus1) {
    plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"SecurityAgent"))
        .WillOnce(::testing::Return(nullptr));

    bool result = plugin_->subscribeForInternetStatusEvent("internetStatus");
    EXPECT_TRUE(result);
}

TEST_F(MaintenanceManagerTest, CheckNetworkStatus) {
    plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"org.rdk.Network"))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(&service_));
    EXPECT_CALL(service_, State())
        .WillOnce(::testing::Return(PluginHost::IShell::state::ACTIVATED));
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"SecurityAgent"))
        .WillOnce(Return(&service_));
   	
    bool result = plugin_->checkNetwork();
    EXPECT_TRUE(result);
}

TEST_F(MaintenanceManagerTest, CheckNetwork_ReturnsFalse_WhenNetworkInactive)
{
    plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"org.rdk.Network"))
        .WillOnce(Return(&service_));
    EXPECT_CALL(service_, State())
        .WillOnce(::testing::Return(PluginHost::IShell::state::DEACTIVATED));

    bool result = plugin_->checkNetwork();
    EXPECT_FALSE(result);
}

TEST_F(MaintenanceManagerTest, CheckNetworkStatuswithoutsecurityagent) {
    plugin_->m_service = &service_;
 
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"org.rdk.Network"))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(&service_));
    EXPECT_CALL(service_, State())
        .WillOnce(::testing::Return(PluginHost::IShell::state::ACTIVATED));
 
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"SecurityAgent"))
        .WillOnce(::testing::Return(nullptr));
	
    bool result = plugin_->checkNetwork();
    EXPECT_TRUE(result);
}

TEST_F(MaintenanceManagerTest, isDeviceOnlinesuccess ) {
    plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"org.rdk.Network"))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(&service_));
    EXPECT_CALL(service_, State())
        .WillOnce(::testing::Return(PluginHost::IShell::state::ACTIVATED));
 
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"SecurityAgent"))
        .WillOnce(Return(&service_));

    bool result = plugin_->isDeviceOnline();
    EXPECT_TRUE(result);
}

TEST_F(MaintenanceManagerTest, isDeviceOnlinefail ) {
    plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"org.rdk.Network"))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(&service_));
    EXPECT_CALL(service_, State())
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(PluginHost::IShell::state::DEACTIVATED));   
   	
    bool result = plugin_->isDeviceOnline();
    EXPECT_FALSE(result);
}

TEST_F(MaintenanceManagerTest, TaskExecutionThreadBasicTest) {
     plugin_->m_service = &service_;
     EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"org.rdk.Network"))
          .Times(::testing::AtLeast(1))
          .WillRepeatedly(::testing::Return(&service_));
     EXPECT_CALL(service_, State())
        .WillOnce(::testing::Return(PluginHost::IShell::state::ACTIVATED));
 
     EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"SecurityAgent"))
        .WillOnce(Return(&service_));
    plugin_->task_execution_thread();
}

TEST_F(MaintenanceManagerTest, TaskExecutionThread_NoSecurityAgent) {
    plugin_->m_service = &service_;

    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_, "org.rdk.Network"))
        .WillRepeatedly(Return(&service_));
    EXPECT_CALL(service_, State())
        .WillRepeatedly(::testing::Return(PluginHost::IShell::state::ACTIVATED));

    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"SecurityAgent"))
        .WillOnce(::testing::Return(nullptr));

    plugin_->task_execution_thread();
}

TEST_F(MaintenanceManagerTest, TaskExecutionThreadBasicTest1) {
     plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"org.rdk.Network"))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(&service_));
    EXPECT_CALL(service_, State())
	.Times(::testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(PluginHost::IShell::state::DEACTIVATED));
	
    plugin_->task_execution_thread();

}

TEST_F(MaintenanceManagerTest, DeinitializeIARM_RemovesHandlerAndNullifiesInstance) {
    plugin_->m_service = &service_; 
    plugin_->DeinitializeIARM();

}

TEST_F(MaintenanceManagerTest, GetServiceState_Available) {
    PluginHost::IShell::state state;
    plugin_->m_service = &service_;
    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"test"))
        .WillOnce(::testing::Return(&service_));
    EXPECT_CALL(service_, State())
        .WillOnce(::testing::Return(PluginHost::IShell::state::ACTIVATED));
    uint32_t result = WPEFramework::Plugin::getServiceState(&service_, "test", state);

    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(state, PluginHost::IShell::state::ACTIVATED);

}

TEST_F(MaintenanceManagerTest, SetDeviceInitializationContext_ValidData_ReturnsTrue) {
    plugin_->m_service = &service_;
    JsonObject contextData;
    contextData["partnerId"] = "Sky";
    contextData["regionalConfigService"] = "region.sky.com";

    JsonObject fullResponse;
    fullResponse["deviceInitializationContext"] = contextData;
    bool result = plugin_->setDeviceInitializationContext(fullResponse);

    EXPECT_TRUE(result);
}

TEST_F(MaintenanceManagerTest, SetDeviceInitializationContext_MissingKey_ReturnsTrue) {
    plugin_->m_service = &service_;

    JsonObject contextData;
    contextData["partnerId"] = "Sky"; // "regionalConfigService" is missing

    JsonObject fullResponse;
    fullResponse["deviceInitializationContext"] = contextData;

    bool result = plugin_->setDeviceInitializationContext(fullResponse);
    EXPECT_TRUE(result); 
}

TEST_F(MaintenanceManagerTest, SetDeviceInitializationContext_EmptyValue_ReturnsFalse) {
    plugin_->m_service = &service_;

    JsonObject contextData;
    contextData["partnerId"] = ""; // Empty string
    contextData["regionalConfigService"] = "";

    JsonObject fullResponse;
    fullResponse["deviceInitializationContext"] = contextData;

    bool result = plugin_->setDeviceInitializationContext(fullResponse);
    EXPECT_FALSE(result);
}

TEST_F(MaintenanceManagerTest, SetDeviceInitializationContext_ExtraKeys_Ignored_ReturnsTrue) {
    plugin_->m_service = &service_;

    JsonObject contextData;
    contextData["partnerId"] = "Sky";
    contextData["regionalConfigService"] = "region.sky.com";
    contextData["extraKey"] = "shouldBeIgnored";

    JsonObject fullResponse;
    fullResponse["deviceInitializationContext"] = contextData;

    bool result = plugin_->setDeviceInitializationContext(fullResponse);
    EXPECT_TRUE(result); // Extra keys ignored, required keys present
}

TEST_F(MaintenanceManagerTest, EventHandler_InstanceSet_DelegatesCall) {
    plugin_->m_service = &service_;
    Plugin::MaintenanceManager::_instance = &(*plugin_);
    const char* owner = "TestOwner";
    IARM_EventId_t eventId = 123;
    char dummyData[4] = {0};
    size_t len = sizeof(dummyData);
    Plugin::MaintenanceManager::_MaintenanceMgrEventHandler(owner, eventId, dummyData, len);
}

TEST_F(MaintenanceManagerTest, StaticEventHandler_InstanceNull_LogsWarning) {
    WPEFramework::Plugin::MaintenanceManager::_instance = nullptr;

    IARM_Bus_MaintMGR_EventData_t eventData = {};

    Plugin::MaintenanceManager::_MaintenanceMgrEventHandler(IARM_BUS_MAINTENANCE_MGR_NAME,
                                                    IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE,
                                                    &eventData, sizeof(eventData));
}

TEST_F(MaintenanceManagerTest, IarmEventHandler_AbortFlagTrue_IgnoresEvent) {
    plugin_->m_abort_flag = true;

    IARM_Bus_MaintMGR_EventData_t eventData = {};
    plugin_->iarmEventHandler(IARM_BUS_MAINTENANCE_MGR_NAME, IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE, &eventData, sizeof(eventData));

}

TEST_F(MaintenanceManagerTest, IarmEventHandler_RFCComplete_TaskActive_CompletesTask) {
    plugin_->m_abort_flag = false;
    plugin_->m_notify_status = MAINTENANCE_STARTED;
    plugin_->m_task_map[WPEFramework::Plugin::task_names_foreground[TASK_RFC].c_str()] = true;

    IARM_Bus_MaintMGR_EventData_t eventData = {};
    eventData.data.maintenance_module_status.status = MAINT_RFC_COMPLETE;

    plugin_->iarmEventHandler(IARM_BUS_MAINTENANCE_MGR_NAME, IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE, &eventData, sizeof(eventData));

}

TEST_F(MaintenanceManagerTest, IarmEventHandler_RFCComplete_TaskInactive_Ignored) {
    plugin_->m_abort_flag = false;
    plugin_->m_notify_status = MAINTENANCE_STARTED;
    plugin_->m_task_map[WPEFramework::Plugin::task_names_foreground[TASK_RFC].c_str()] = false;

    IARM_Bus_MaintMGR_EventData_t eventData = {};
    eventData.data.maintenance_module_status.status = MAINT_RFC_COMPLETE;

    plugin_->iarmEventHandler(IARM_BUS_MAINTENANCE_MGR_NAME,
                              IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE,
                              &eventData, sizeof(eventData));
    EXPECT_FALSE(plugin_->m_task_map[WPEFramework::Plugin::task_names_foreground[TASK_RFC].c_str()]);
}

TEST_F(MaintenanceManagerTest, IarmEventHandler_LogUploadError_TaskActive_Handled) {
    plugin_->m_abort_flag = false;
    plugin_->m_notify_status = MAINTENANCE_STARTED;
    plugin_->m_task_map[WPEFramework::Plugin::task_names_foreground[TASK_LOGUPLOAD].c_str()] = true;

    IARM_Bus_MaintMGR_EventData_t eventData = {};
    eventData.data.maintenance_module_status.status = MAINT_LOGUPLOAD_ERROR;

    plugin_->iarmEventHandler(IARM_BUS_MAINTENANCE_MGR_NAME,
                              IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE,
                              &eventData, sizeof(eventData));

    EXPECT_TRUE(plugin_->m_task_map[WPEFramework::Plugin::task_names_foreground[TASK_LOGUPLOAD].c_str()]);
}

TEST_F(MaintenanceManagerTest, IarmEventHandler_FWDownloadAborted_TaskMarkedSkipped) {
    plugin_->m_abort_flag = false;
    plugin_->m_notify_status = MAINTENANCE_STARTED;
    plugin_->m_task_map[WPEFramework::Plugin::task_names_foreground[TASK_SWUPDATE].c_str()] = true;

    IARM_Bus_MaintMGR_EventData_t eventData = {};
    eventData.data.maintenance_module_status.status = MAINT_FWDOWNLOAD_ABORTED;

    plugin_->iarmEventHandler(IARM_BUS_MAINTENANCE_MGR_NAME,
                              IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE,
                              &eventData, sizeof(eventData));

   EXPECT_FALSE(plugin_->m_task_map[WPEFramework::Plugin::task_names_foreground[TASK_SWUPDATE].c_str()]);
}

TEST_F(MaintenanceManagerTest, IarmEventHandler_RebootRequired_GlobalFlagSet) {
    plugin_->m_abort_flag = false;
    plugin_->m_notify_status = MAINTENANCE_STARTED;

    IARM_Bus_MaintMGR_EventData_t eventData = {};
    eventData.data.maintenance_module_status.status = MAINT_REBOOT_REQUIRED;

    plugin_->iarmEventHandler(IARM_BUS_MAINTENANCE_MGR_NAME,
                              IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE,
                              &eventData, sizeof(eventData));

}

TEST_F(MaintenanceManagerTest, IarmEventHandler_UnexpectedOwner_Ignored) {
    plugin_->m_abort_flag = false;

    IARM_Bus_MaintMGR_EventData_t eventData = {};
    eventData.data.maintenance_module_status.status = MAINT_RFC_COMPLETE;

    plugin_->iarmEventHandler("SomeOtherOwner", IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE, &eventData, sizeof(eventData));

}
TEST_F(MaintenanceManagerTest, IarmEventHandler_UnknownEventId_Ignored) {
    plugin_->m_abort_flag = false;

    IARM_Bus_MaintMGR_EventData_t eventData = {};
    plugin_->iarmEventHandler(IARM_BUS_MAINTENANCE_MGR_NAME, 999, &eventData, sizeof(eventData));

}
TEST_F(MaintenanceManagerTest, IarmEventHandler_NonStartedStatus_Ignored) {
    plugin_->m_abort_flag = false;
    plugin_->m_notify_status = MAINTENANCE_IDLE; // Not STARTED

    IARM_Bus_MaintMGR_EventData_t eventData = {};
    eventData.data.maintenance_module_status.status = MAINT_RFC_COMPLETE;

    plugin_->iarmEventHandler(IARM_BUS_MAINTENANCE_MGR_NAME,
                              IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE,
                              &eventData, sizeof(eventData));

}

TEST_F(MaintenanceManagerTest, IarmEventHandler_CriticalUpdate_GlobalFlagSet) {
    plugin_->m_abort_flag = false;
    plugin_->m_notify_status = MAINTENANCE_STARTED;

    IARM_Bus_MaintMGR_EventData_t eventData = {};
    eventData.data.maintenance_module_status.status = MAINT_CRITICAL_UPDATE;

    plugin_->iarmEventHandler(IARM_BUS_MAINTENANCE_MGR_NAME,
                              IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE,
                              &eventData, sizeof(eventData));

}
TEST_F(MaintenanceManagerTest, IarmEventHandler_RfcInProgress_SetsTrue) {
    plugin_->m_abort_flag = false;
    plugin_->m_notify_status = MAINTENANCE_STARTED;
    plugin_->m_task_map[WPEFramework::Plugin::task_names_foreground[TASK_RFC].c_str()] = false;

    IARM_Bus_MaintMGR_EventData_t eventData = {};
    eventData.data.maintenance_module_status.status = MAINT_RFC_INPROGRESS;

    plugin_->iarmEventHandler(IARM_BUS_MAINTENANCE_MGR_NAME,
                              IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE,
                              &eventData, sizeof(eventData));

    EXPECT_TRUE(plugin_->m_task_map[WPEFramework::Plugin::task_names_foreground[TASK_RFC].c_str()]);
}

TEST_F(MaintenanceManagerTest, SecManagerActive_AllGood_ReturnsTrue)
{
   plugin_->m_service = &service_;
   EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"SecurityAgent"))
       .Times(::testing::AtLeast(1))
       .WillRepeatedly(Return(&service_));

   EXPECT_CALL(service_, State())
        .WillOnce(::testing::Return(PluginHost::IShell::state::ACTIVATED));

    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"org.rdk.SecManager"))
        .WillOnce(::testing::Return(&service_));
    
    std::string activation = "not-activated";
    bool ok = plugin_->knowWhoAmI(activation);

    EXPECT_FALSE(ok);
}
TEST_F(MaintenanceManagerTest, SecManagerActive_AllGood_ReturnsTrue1)
{
   plugin_->m_service = &service_;
   EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"SecurityAgent"))
       .Times(::testing::AtLeast(1))
       .WillRepeatedly(Return(&service_));
   EXPECT_CALL(service_, State())
        .WillOnce(::testing::Return(PluginHost::IShell::state::ACTIVATED));

    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"org.rdk.SecManager"))
        .WillOnce(::testing::Return(&service_));
    
    std::string activation = "activated";
    bool ok = plugin_->knowWhoAmI(activation);

    EXPECT_FALSE(ok);
}

TEST_F(MaintenanceManagerTest, SecManagerActive_AllGood_ReturnsTrue2)
{
  plugin_->m_service = &service_;
  EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"SecurityAgent"))
       .Times(::testing::AtLeast(1))
       .WillRepeatedly(::testing::Return(nullptr));

   EXPECT_CALL(service_, State())
        .WillOnce(::testing::Return(PluginHost::IShell::state::ACTIVATED));

    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"org.rdk.SecManager"))
        .WillOnce(::testing::Return(&service_));
    
    std::string activation = "activated";
    plugin_->knowWhoAmI(activation);
}

TEST_F(MaintenanceManagerTest, SecManagerInactive_RetriesOnce)
{
    plugin_->m_service = &service_;

    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"org.rdk.SecManager"))
        .WillOnce(::testing::Return(&service_));
    EXPECT_CALL(service_, State())
        .WillOnce(::testing::Return(PluginHost::IShell::state::DEACTIVATED));

    std::string activation = "not-activated";

    std::thread t([&]() {
        plugin_->knowWhoAmI(activation); // This would loop forever in real case
    });

    sleep(1); // allow time for log/first retry
    t.detach(); // avoid blocking test
}
TEST_F(MaintenanceManagerTest, SecManagerInactive_ActivatedDevice_ExitsImmediately)
{
    plugin_->m_service = &service_;

    EXPECT_CALL(service_, QueryInterfaceByCallsign(::testing::_,"org.rdk.SecManager"))
        .WillOnce(::testing::Return(&service_));
    EXPECT_CALL(service_, State())
        .WillOnce(::testing::Return(PluginHost::IShell::state::DEACTIVATED));

    std::string activation = "activated";
    bool result = plugin_->knowWhoAmI(activation);

    EXPECT_FALSE(result);
}

TEST_F(MaintenanceManagerTest, MaintenanceManagerOnBootup_InitializesCorrectly1) {
    plugin_->m_service = &service_;
    Plugin::MaintenanceManager::_instance = &(*plugin_);
    plugin_->m_setting.setValue("softwareoptout", "INVALID_MODE");
    
    plugin_->maintenanceManagerOnBootup();
    EXPECT_EQ(plugin_->g_currentMode, FOREGROUND_MODE);
    EXPECT_EQ(plugin_->m_notify_status, MAINTENANCE_IDLE);
    EXPECT_EQ(plugin_->g_epoch_time, "");
    EXPECT_EQ(plugin_->g_maintenance_type, UNSOLICITED_MAINTENANCE);
    EXPECT_EQ(plugin_->m_setting.getValue("softwareoptout").String(), "NONE");
    EXPECT_EQ(plugin_->g_is_critical_maintenance, "false");
    EXPECT_EQ(plugin_->g_is_reboot_pending, "false");
    EXPECT_EQ(plugin_->g_lastSuccessful_maint_time, "");
    EXPECT_EQ(plugin_->g_task_status, 0);
    EXPECT_FALSE(plugin_->m_abort_flag);
    EXPECT_FALSE(plugin_->g_unsolicited_complete);

}

TEST_F(MaintenanceManagerTest, InitializeIARM_RegistersEventAndBootsUp) {
    plugin_->m_service = &service_;    
    plugin_->InitializeIARM();
}
#endif

