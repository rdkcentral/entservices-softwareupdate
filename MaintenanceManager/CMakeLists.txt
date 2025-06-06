# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2021 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(PLUGIN_NAME MaintenanceManager)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(PLUGIN_MAINTENANCEMGR_STARTUPORDER "" CACHE STRING "To configure startup order of MaintenanceManager plugin")

find_package(${NAMESPACE}Plugins REQUIRED)

add_library(${MODULE_NAME} SHARED
        MaintenanceManager.cpp
        Module.cpp)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

target_compile_definitions(${MODULE_NAME} PRIVATE MODULE_NAME=Plugin_${PLUGIN_NAME})

if (NOT RDK_SERVICES_L1_TEST)
    target_compile_options(${MODULE_NAME} PRIVATE -Wno-error)
endif ()

list(APPEND CMAKE_MODULE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

find_package(IARMBus)
if (IARMBus_FOUND)
    target_include_directories(${MODULE_NAME} PRIVATE ${IARMBUS_INCLUDE_DIRS})
    target_link_libraries(${MODULE_NAME} PRIVATE ${NAMESPACE}Plugins::${NAMESPACE}Plugins ${IARMBUS_LIBRARIES})
else (IARMBus_FOUND)
    message ("Module IARMBus required.")
    target_include_directories(${MODULE_NAME} PRIVATE ${IARMBUS_INCLUDE_DIRS})
    target_link_libraries(${MODULE_NAME} PRIVATE ${NAMESPACE}Plugins::${NAMESPACE}Plugins ${IARMBUS_LIBRARIES})
endif(IARMBus_FOUND)

target_include_directories(${MODULE_NAME} PRIVATE ../helpers)
target_include_directories(${MODULE_NAME} PRIVATE ./)

set_source_files_properties(MaintenanceManager.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")

# Check for ENABLE_JOURNAL_LOGGING option
if (ENABLE_JOURNAL_LOGGING)
    message("Journal Logging enabled.")
    target_compile_definitions(${MODULE_NAME} PRIVATE ENABLE_JOURNAL_LOGGING=ON)
    target_link_libraries(${MODULE_NAME} PRIVATE systemd)
endif()

# Add Task Timeout
if (TASK_TIMEOUT)
    message(STATUS "Task Timeout set to ${TASK_TIMEOUT}")
    target_compile_definitions(${MODULE_NAME} PRIVATE TASK_TIMEOUT=${TASK_TIMEOUT})
else()
    target_compile_definitions(${MODULE_NAME} PRIVATE TASK_TIMEOUT=3600) # default value
endif()

# Include and link for signal, csignal, time, ctime headers
target_link_libraries(${MODULE_NAME} PRIVATE pthread rt)

install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
