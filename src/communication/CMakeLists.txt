#
# Copyright (C) 2024 Renesas Electronics Corporation.
# Copyright (C) 2024 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET communication)

# ######################################################################################################################
# Options
# ######################################################################################################################

option(WITH_VCHAN "build with vchan" ON)

# ######################################################################################################################
# Compiler flags
# ######################################################################################################################

add_definitions(-DLOG_MODULE="communication")

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES communicationchannel.cpp communicationmanager.cpp openchannel.cpp securechannel.cpp iamconnection.cpp
            cmconnection.cpp
)

if(WITH_VCHAN)
    list(APPEND SOURCES vchan.cpp vchanmanager.cpp)
else()
    list(APPEND SOURCES vchanmanagerstub.cpp)
endif()

# ######################################################################################################################
# Includes
# ######################################################################################################################

# ######################################################################################################################
# Target
# ######################################################################################################################

add_library(${TARGET} STATIC ${SOURCES})

# target_compile_definitions(communication PRIVATE OPENSSL_SUPPRESS_DEPRECATED)
target_compile_options(${TARGET} PRIVATE -Wno-deprecated-declarations)

# ######################################################################################################################
# Libraries
# ######################################################################################################################

target_link_libraries(
    ${TARGET}
    PUBLIC aosutils
           aoscommon
           aoscoreapi-gen-objects
           aosprotocol
           Poco::Foundation
           OpenSSL::SSL
           OpenSSL::Crypto
           downloader
           imageunpacker
           filechunker
)
