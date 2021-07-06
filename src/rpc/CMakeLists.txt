#
#  Copyright (c) 2021, The OpenThread Authors.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. Neither the name of the copyright holder nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#

# OPENTHREAD_RCP_RPC_CLIENT_ENABLE
# OPENTHREAD_RCP_RPC_SERVER_ENABLE
# openthread-ncp-ftd    --> openthread-ncp-ftd-rpc-server
# openthread-ncp-mtd    --> openthread-ncp-mtd-rpc-server
# openthread-rcp        --> openthread-rcp-rpc-server
#
# openthread-spinel-rcp --> openthread-rcp-rpc-client
#

include(${CMAKE_SOURCE_DIR}/third_party/pigweed/repo/pw_protobuf_compiler/proto.cmake)

set(COMMON_INCLUDES
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/core
)

set(COMMON_SOURCES
    rcp_client.cpp
    rcp_server.cpp
    rpc_decoder.cpp
)

set(PIGWEED_LIBRARIES
    pw_rpc.client
    pw_rpc.server
    pw_assert
    pw_bytes
    pw_containers
    pw_span
    pw_status
    pw_sys_io
    pw_sys_io_stdio
    pw_rpc.protos.pwpb
    pw_rpc.common
    pw_rpc.nanopb.common
    pw_rpc.nanopb.method
    pw_rpc.nanopb.client
    pw_rpc.rcp_proto.nanopb
    pw_rpc.rcp_proto.nanopb_rpc
    pw_protobuf
    pw_hdlc
    pw_checksum
    pw_stream
)

pw_proto_library(pw_rpc.rcp_proto
  SOURCES
    protos/rcp.proto
)

#=====================================================================
add_library(openthread-rcp-rpc-client)

set_target_properties(
    openthread-rcp-rpc-client
    PROPERTIES
        C_STANDARD 99
        CXX_STANDARD 17
)

target_compile_definitions(openthread-rcp-rpc-client PRIVATE
    OPENTHREAD_RCP_RPC_CLIENT_ENABLE=1
)

target_compile_options(openthread-rcp-rpc-client PRIVATE
    ${OT_CFLAGS}
)

target_include_directories(openthread-rcp-rpc-client PUBLIC ${OT_PUBLIC_INCLUDES} PRIVATE ${COMMON_INCLUDES})

target_sources(openthread-rcp-rpc-client PRIVATE ${COMMON_SOURCES})

target_link_libraries(openthread-rcp-rpc-client
    PUBLIC
        openthread-ftd
    PRIVATE
        ${OT_PLATFORM_RPC_LIB}
        ot-config
		${PIGWEED_LIBRARIES}
)

#=====================================================================
add_library(openthread-rcp-rpc-server)

set_target_properties(
    openthread-rcp-rpc-server
    PROPERTIES
        C_STANDARD 99
        CXX_STANDARD 17
)

target_compile_definitions(openthread-rcp-rpc-server PRIVATE
    OPENTHREAD_RCP_RPC_SERVER_ENABLE=1
)

target_compile_options(openthread-rcp-rpc-server PRIVATE
    ${OT_CFLAGS}
)

target_include_directories(openthread-rcp-rpc-server PUBLIC ${OT_PUBLIC_INCLUDES} PRIVATE ${COMMON_INCLUDES})

target_sources(openthread-rcp-rpc-server PRIVATE ${COMMON_SOURCES})

target_link_libraries(openthread-rcp-rpc-server
    #PUBLIC
    #    openthread-ftd
    PRIVATE
        ${OT_PLATFORM_RPC_LIB}
        ot-config
		${PIGWEED_LIBRARIES}
)
