# ----------------------------------------------------------------------
# ----------------------------------------------------------------------
cmake_minimum_required(VERSION 3.21)

cmake_policy(SET CMP0091 NEW)

project(grpc_labview C CXX)

math(EXPR _BITNESS "8*${CMAKE_SIZEOF_VOID_P}")

message(STATUS "Building for ${_BITNESS}-bit...")

set(CMAKE_CXX_STANDARD 14)
set(ABSL_PROPAGATE_CXX_STD=ON)
set(C_VISIBILITY_PRESET hidden)
set(CXX_VISIBILITY_PRESET hidden)
set(VISIBILITY_INLINES_HIDDEN ON)
set(WINDOWS_EXPORT_ALL_SYMBOLS OFF)

if(MSVC)
  add_definitions(-D_WIN32_WINNT=0x600)
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

set(gRPC_BUILD_CODEGEN ON CACHE INTERNAL "CODEGEN ON")
set(gRPC_BUILD_CSHARP_EXT OFF CACHE INTERNAL "CSHARP_EXT OFF")
set(gRPC_BUILD_GRPC_CPP_PLUGIN ON CACHE INTERNAL "CPP_PLUGIN ON")
set(gRPC_BUILD_GRPC_CSHARP_PLUGIN OFF CACHE INTERNAL "CSHARP_PLUGIN OFF")
set(gRPC_BUILD_GRPC_NODE_PLUGIN OFF CACHE INTERNAL "NODE_PLUGIN OFF")
set(gRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN OFF CACHE INTERNAL "OBJECTIVE_C_PLUGIN OFF")
set(gRPC_BUILD_GRPC_PHP_PLUGIN OFF CACHE INTERNAL "PHP_PLUGIN OFF")
set(gRPC_BUILD_GRPC_PYTHON_PLUGIN ON CACHE INTERNAL "PYTHON_PLUGIN ON")
set(gRPC_BUILD_GRPC_RUBY_PLUGIN OFF CACHE INTERNAL "RUBY_PLUGIN OFF")

find_package(grpc REQUIRED)

FetchContent_Declare(
  whereami
  GIT_REPOSITORY    https://github.com/gpakosz/whereami.git
  GIT_TAG f5e3eac441acbb4ec1fe3e2c32646248ae463398 # latest commit as of Jun 9, 2024
  PATCH_COMMAND git apply ${CMAKE_CURRENT_SOURCE_DIR}/patches/whereami.patch # ensure patch UTF8 encoded! Use cmd /c "git diff > whereami.patch" on Windows
  UPDATE_DISCONNECTED ON # prevents trying to patch multiple times
)
FetchContent_MakeAvailable(whereami)


# ----------------------------------------------------------------------
# Use the grpc targets directly from this build.
# ----------------------------------------------------------------------
set(_PROTOBUF_LIBPROTOBUF libprotobuf)
set(_REFLECTION grpc++_reflection)

if(CMAKE_CROSSCOMPILING)
  find_program(_PROTOBUF_PROTOC protoc)
else()
  set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
endif()

set(_GRPC_GRPCPP grpc++)

if(CMAKE_CROSSCOMPILING)
  find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
else()
  set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
endif()

# ----------------------------------------------------------------------
# Include generated *.pb.h files
# ----------------------------------------------------------------------
include_directories("${CMAKE_CURRENT_BINARY_DIR}" "./src" "./third_party/grpc" "${CMAKE_CURRENT_BINARY_DIR}/gens" 1`1)

# ----------------------------------------------------------------------
# LabVIEW support for grpc and protobuf
# ----------------------------------------------------------------------
add_custom_target(Detect_Compatibility_Breaks
  COMMAND ${CMAKE_COMMAND} -E echo "Detecting backward compatibility breakage ..."
  COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeTests/run_test.py
  RESULT_VARIABLE shell_command_result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

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
target_link_libraries(${PROJECT_NAME}
  PRIVATE
  whereami
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

if(MSVC)
  target_compile_definitions(${PROJECT_NAME} PRIVATE _PS_${CMAKE_SIZEOF_VOID_P})
  set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "lib")
endif()

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
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

if(MSVC)
  target_compile_definitions(${PROJECT_NAME}_generator PRIVATE _PS_${CMAKE_SIZEOF_VOID_P})
  set_target_properties(${PROJECT_NAME}_generator PROPERTIES PREFIX "lib")
endif()

# ######################################################################
# QueryServer Example
# ######################################################################

# ----------------------------------------------------------------------
# Proto file
# ----------------------------------------------------------------------
get_filename_component(qs_proto "examples/query_server/Protos/query_server.proto" ABSOLUTE)
get_filename_component(qs_proto_path "${qs_proto}" PATH)

# ----------------------------------------------------------------------
# Generated sources
# ----------------------------------------------------------------------
set(qs_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/query_server.pb.cc")
set(qs_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/query_server.pb.h")
set(qs_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/query_server.grpc.pb.cc")
set(qs_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/query_server.grpc.pb.h")
add_custom_command(
  OUTPUT "${qs_proto_srcs}" "${qs_proto_hdrs}" "${qs_grpc_srcs}" "${qs_grpc_hdrs}"
  COMMAND ${_PROTOBUF_PROTOC}
  ARGS --grpc_out="${CMAKE_CURRENT_BINARY_DIR}"
  --cpp_out="${CMAKE_CURRENT_BINARY_DIR}"
  -I="${qs_proto_path}"
  --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
  "${qs_proto}"
  DEPENDS "${qs_proto}")

# ----------------------------------------------------------------------
# Example C++ application to talk to the example query server
# ----------------------------------------------------------------------
add_executable(example_client
  "src/example_client.cc"
  ${qs_proto_srcs}
  ${qs_grpc_srcs}
)
target_link_libraries(example_client
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# ######################################################################
# Testing Project
# ######################################################################

# ----------------------------------------------------------------------
# Proto file
# ----------------------------------------------------------------------
get_filename_component(dm_proto "tests/Deprecated/Protos/data_marshal.proto" ABSOLUTE)
get_filename_component(dm_proto_path "${dm_proto}" PATH)

# ----------------------------------------------------------------------
# Generated sources
# ----------------------------------------------------------------------
set(dm_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/data_marshal.pb.cc")
set(dm_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/data_marshal.pb.h")
set(dm_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/data_marshal.grpc.pb.cc")
set(dm_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/data_marshal.grpc.pb.h")
add_custom_command(
  OUTPUT "${dm_proto_srcs}" "${dm_proto_hdrs}" "${dm_grpc_srcs}" "${dm_grpc_hdrs}"
  COMMAND ${_PROTOBUF_PROTOC}
  ARGS --grpc_out="${CMAKE_CURRENT_BINARY_DIR}"
  --cpp_out="${CMAKE_CURRENT_BINARY_DIR}"
  -I="${dm_proto_path}"
  --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
  "${dm_proto}"
  DEPENDS "${dm_proto}")

# ----------------------------------------------------------------------
# Test client application
# ----------------------------------------------------------------------
add_executable(test_client
  "src/test_client.cc"
  ${dm_proto_srcs}
  ${dm_grpc_srcs}
)
target_link_libraries(test_client
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# ----------------------------------------------------------------------
# Test server application
# ----------------------------------------------------------------------
add_executable(test_server
  "src/test_server.cc"
  ${dm_proto_srcs}
  ${dm_grpc_srcs}
)
target_link_libraries(test_server
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

add_dependencies(${PROJECT_NAME} Detect_Compatibility_Breaks)
add_dependencies(${PROJECT_NAME}_generator Detect_Compatibility_Breaks)
add_dependencies(test_client Detect_Compatibility_Breaks)
add_dependencies(test_server Detect_Compatibility_Breaks)
add_dependencies(example_client Detect_Compatibility_Breaks)


# ----------------------------------------------------------------------
# Add a labview-src local install directory for easier dll development/debugging
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