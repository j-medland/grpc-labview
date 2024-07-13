# ----------------------------------------------------------------------
# ----------------------------------------------------------------------
cmake_minimum_required(VERSION 3.15)

project(grpc_labview C CXX)

math(EXPR _BITNESS "8*${CMAKE_SIZEOF_VOID_P}")

message(STATUS "Building for ${_BITNESS}-bit...")

set(CMAKE_CXX_STANDARD 14)
set(C_VISIBILITY_PRESET hidden)
set(CXX_VISIBILITY_PRESET hidden)
set(VISIBILITY_INLINES_HIDDEN ON)
set(WINDOWS_EXPORT_ALL_SYMBOLS OFF)
set(BUILD_SHARED_LIBS OFF)

if(MSVC)
  # Force Static Runtime for Protobuf build - required for protobuf version used by grpc V1.44
  # Fixed in https://github.com/protocolbuffers/protobuf/pull/9437
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  set(_GRPC_LV_PLATFORM_NAME Win${_BITNESS})
elseif(UNIX)
  if(CMAKE_CROSSCOMPILING)
    set(_GRPC_LV_PLATFORM_NAME LinuxRT)
  else()
    set(_GRPC_LV_PLATFORM_NAME Linux)
  endif()
endif()

find_package(Threads REQUIRED)

# ----------------------------------------------------------------------
# Get the project dependencies
# ----------------------------------------------------------------------
# Specify to build all libs that aren't explicitly shared as static
set(BUILD_SHARED_LIBS FALSE)

# Propagate C++ Standard Version - on by default for newer CMake Versions
set(ABSL_PROPAGATE_CXX_STD ON)

# Turn off the protobuf install build so we don't have to build Abseil's build targets
# https://github.com/protocolbuffers/protobuf/issues/12185
set(protobuf_INSTALL OFF)
set(utf8_range_ENABLE_INSTALL OFF)

# Set the CACHE values for building GRPC to remove unecssary builds
set(gRPC_BUILD_CODEGEN ON CACHE INTERNAL "CODEGEN ON")
set(gRPC_BUILD_CSHARP_EXT OFF CACHE INTERNAL "CSHARP_EXT OFF")
set(gRPC_BUILD_GRPC_CPP_PLUGIN ON CACHE INTERNAL "CPP_PLUGIN ON")
set(gRPC_BUILD_GRPC_CSHARP_PLUGIN OFF CACHE INTERNAL "CSHARP_PLUGIN OFF")
set(gRPC_BUILD_GRPC_NODE_PLUGIN OFF CACHE INTERNAL "NODE_PLUGIN OFF")
set(gRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN OFF CACHE INTERNAL "OBJECTIVE_C_PLUGIN OFF")
set(gRPC_BUILD_GRPC_PHP_PLUGIN OFF CACHE INTERNAL "PHP_PLUGIN OFF")
set(gRPC_BUILD_GRPC_PYTHON_PLUGIN OFF CACHE INTERNAL "PYTHON_PLUGIN OFF")
set(gRPC_BUILD_GRPC_RUBY_PLUGIN OFF CACHE INTERNAL "RUBY_PLUGIN OFF")

include(FetchContent)
FetchContent_Declare(
  gRPC
  GIT_REPOSITORY https://github.com/grpc/grpc
  GIT_TAG 591d56e1300b6d11948e1b821efac785a295989c # V1.44
  UPDATE_DISCONNECTED ON # prevents trying to download multiple times
)
set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(gRPC)

FetchContent_Declare(
  whereami
  GIT_REPOSITORY https://github.com/gpakosz/whereami.git
  GIT_TAG f5e3eac441acbb4ec1fe3e2c32646248ae463398 # latest commit as of Jun 9, 2024
  PATCH_COMMAND git apply ${CMAKE_CURRENT_SOURCE_DIR}/patches/whereami.patch # ensure patch UTF8 encoded!
  UPDATE_DISCONNECTED ON # prevents trying to patch multiple times
)
FetchContent_MakeAvailable(whereami)

# ----------------------------------------------------------------------
# LabVIEW grpc_labview library
# ----------------------------------------------------------------------
add_library(${PROJECT_NAME} SHARED
  src/any_support.cc
  src/cluster_copier.cc
  src/event_data.cc
  src/feature_toggles.cc
  src/grpc_client.cc
  src/grpc_interop.cc
  src/grpc_load.cc
  src/grpc_server.cc
  src/lv_interop.cc
  src/lv_message.cc
  src/lv_message_efficient.cc
  src/lv_message_value.cc
  src/lv_proto_server_reflection_plugin.cc
  src/lv_proto_server_reflection_service.cc
  src/message_element_metadata_owner.cc
  src/unpacked_fields.cc
  src/grpc_entry_point.cc
)

target_include_directories(${PROJECT_NAME} PRIVATE
  # custom LV reflection service requires this grpc-internal include directory
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/gens"
  # add binary directory so auto-generated grpc_labview_export.h is found
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
  whereami
  grpc++
  grpc++_reflection
  libprotobuf)

if(MSVC)
  target_compile_definitions(${PROJECT_NAME} PRIVATE _PS_${CMAKE_SIZEOF_VOID_P})
else()
  # remove "lib" prefix from linux build
  set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
endif()

include(GenerateExportHeader)

# generate export header
generate_export_header(${PROJECT_NAME})

# ----------------------------------------------------------------------
# LabVIEW code generator support library to generate client and
# server VIs from a .proto file
# ----------------------------------------------------------------------
add_library(${PROJECT_NAME}_generator SHARED
  src/feature_toggles.cc
  src/lv_interop.cc
  src/proto_parser.cc
  src/grpc_entry_point.cc
)
target_link_libraries(${PROJECT_NAME}_generator
  PRIVATE
  whereami
  grpc++
  grpc++_reflection
  libprotobuf)

target_include_directories(${PROJECT_NAME}_generator PRIVATE
  # custom LV reflection service requires this grpc-internal include directory
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/gens"
  # add binary directory so auto-generated grpc_labview_export.h is found
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
)

# explicitly set gprc_labview_EXPORTS as it won't be set automatically for this target
target_compile_definitions(${PROJECT_NAME}_generator PRIVATE grpc_labview_EXPORTS)

if(MSVC)
  target_compile_definitions(${PROJECT_NAME}_generator PRIVATE _PS_${CMAKE_SIZEOF_VOID_P})
else()
  # remove "lib" prefix from linux build
  set_target_properties(${PROJECT_NAME}_generator PROPERTIES PREFIX "")
endif()

# ----------------------------------------------------------------------
# Add a labview-src local install directory to make locating binaries for packaging easier
# ----------------------------------------------------------------------
SET(_GRPC_LV_INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gRPC LabVIEW/Libraries/${_GRPC_LV_PLATFORM_NAME}")
install(TARGETS ${PROJECT_NAME}
  COMPONENT ${PROJECT_NAME}
  RUNTIME DESTINATION ${_GRPC_LV_INSTALL_DIR}
  LIBRARY DESTINATION ${_GRPC_LV_INSTALL_DIR}
)

SET(_GRPC_LV_GENERATOR_INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gRPC LabVIEW Generator/Libraries/${_GRPC_LV_PLATFORM_NAME}")
install(TARGETS ${PROJECT_NAME}_generator
  COMPONENT ${PROJECT_NAME}
  RUNTIME DESTINATION ${_GRPC_LV_GENERATOR_INSTALL_DIR}
  LIBRARY DESTINATION ${_GRPC_LV_GENERATOR_INSTALL_DIR}
)

# ----------------------------------------------------------------------
# LabVIEW Compatability Checks
# ----------------------------------------------------------------------
add_custom_target(Detect_Compatibility_Breaks
  COMMAND ${CMAKE_COMMAND} -E echo "Detecting backward compatibility breakage ..."
  COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeTests/run_test.py
  RESULT_VARIABLE shell_command_result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_dependencies(${PROJECT_NAME} Detect_Compatibility_Breaks)
add_dependencies(${PROJECT_NAME}_generator Detect_Compatibility_Breaks)