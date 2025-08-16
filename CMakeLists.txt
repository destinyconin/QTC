cmake_minimum_required(VERSION 3.16)

project(QTC_Blockchain VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost 1.70 COMPONENTS system filesystem thread OPTIONAL_COMPONENTS program_options)

set(PROJECT_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# collect sources that actually exist
set(_CANDIDATE_SOURCES
  src/blockchain/Block.cpp
  src/blockchain/Blockchain.cpp
  src/blockchain/Transaction.cpp
  src/wallet/Wallet.cpp
  src/network/Node.cpp
  src/rpc/RpcServer.cpp
  src/zk/Zk.cpp
  src/consensus/ProofOfWork.cpp
  src/crypto/Hash.cpp
  src/crypto/Signature.cpp
  src/utils/Logger.cpp
  src/vm/VM.cpp
)

set(CORE_SOURCES "")
foreach(F ${_CANDIDATE_SOURCES})
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${F}")
    list(APPEND CORE_SOURCES "${F}")
  else()
    message(STATUS "[QTC] Skipping missing source: ${F}")
  endif()
endforeach()

set(_CANDIDATE_HEADERS
  include/blockchain/Block.h
  include/blockchain/Blockchain.h
  include/blockchain/Transaction.h
  include/wallet/Wallet.h
  include/network/Node.h
  include/rpc/RpcServer.h
  include/zk/Zk.h
  include/config/Constants.h
  include/consensus/ProofOfWork.h
  include/crypto/Hash.h
  include/crypto/Signature.h
  include/utils/Logger.h
  include/vm/VM.h
)

set(CORE_HEADERS "")
foreach(H ${_CANDIDATE_HEADERS})
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${H}")
    list(APPEND CORE_HEADERS "${H}")
  else()
    message(STATUS "[QTC] Skipping missing header: ${H}")
  endif()
endforeach()

if(CORE_SOURCES STREQUAL "")
  message(FATAL_ERROR "No core sources found.")
endif()

add_library(qtc_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})
target_include_directories(qtc_core PUBLIC ${PROJECT_INCLUDE_DIRS})
target_link_libraries(qtc_core PRIVATE OpenSSL::Crypto Threads::Threads)
if(Boost_FOUND)
  target_include_directories(qtc_core PUBLIC ${Boost_INCLUDE_DIRS})
  target_link_libraries(qtc_core PRIVATE ${Boost_LIBRARIES})
  target_compile_definitions(qtc_core PUBLIC BOOST_AVAILABLE)
endif()
if(UNIX AND NOT APPLE)
  target_link_libraries(qtc_core PRIVATE ${CMAKE_DL_LIBS})
endif()
target_compile_options(qtc_core PRIVATE -Wall -Wextra -Wpedantic)
add_compile_options("$<$<CONFIG:Debug>:-g;-O0;-DDEBUG>" "$<$<CONFIG:Release>:-O3;-DNDEBUG>")

# ---------------- ZK (libsnark) ----------------
option(QTC_ENABLE_ZK "Enable zk-SNARK support" ON)
option(QTC_USE_LIBSNARK_SUBDIR "Build libsnark from extern/libsnark" ON)

if(QTC_ENABLE_ZK)
  # choose curve: ALT_BN128  (define CURVE_ALT_BN128 for libff/libsnark headers)
  set(WITH_PROCPS OFF CACHE BOOL "Disable procps in libsnark" FORCE)
  set(WITH_SUPERCOP OFF CACHE BOOL "Disable supercop in libsnark" FORCE)
  set(CURVE "ALT_BN128" CACHE STRING "Selected curve" FORCE)

  if(QTC_USE_LIBSNARK_SUBDIR AND EXISTS "${CMAKE_SOURCE_DIR}/extern/libsnark/CMakeLists.txt}")
    add_subdirectory(extern/libsnark)
    target_link_libraries(qtc_core PRIVATE snark)
    target_include_directories(qtc_core PRIVATE
      ${CMAKE_SOURCE_DIR}/extern/libsnark
      ${CMAKE_SOURCE_DIR}/extern/libsnark/depends/libff
      ${CMAKE_SOURCE_DIR}/extern/libsnark/depends/libfqfft
      ${CMAKE_SOURCE_DIR}/extern/libsnark/depends/gtest/googletest/include
    )
  else()
    find_path(LIBSNARK_INCLUDE_DIR libsnark/common/default_types/r1cs_ppzksnark_pp.hpp)
    find_library(LIBSNARK_LIB snark)
    if(LIBSNARK_INCLUDE_DIR AND LIBSNARK_LIB)
      target_include_directories(qtc_core PRIVATE ${LIBSNARK_INCLUDE_DIR})
      target_link_libraries(qtc_core PRIVATE ${LIBSNARK_LIB})
    else()
      message(WARNING "libsnark not found; building without ZK")
    endif()
  endif()

  # This macro is what fixes: "#error You must define one of the CURVE_* symbols"
  target_compile_definitions(qtc_core PRIVATE QTC_ENABLE_ZK CURVE_ALT_BN128)
endif()
# -------------- end ZK -------------------------

add_executable(qtc_node src/main.cpp)
target_include_directories(qtc_node PRIVATE ${PROJECT_INCLUDE_DIRS})
target_link_libraries(qtc_node PRIVATE qtc_core Threads::Threads)
if(UNIX AND NOT APPLE)
  target_link_libraries(qtc_node PRIVATE ${CMAKE_DL_LIBS})
endif()
target_compile_options(qtc_node PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_test(NAME qtc_version COMMAND ${CMAKE_BINARY_DIR}/qtc_node --version)

install(TARGETS qtc_node qtc_core
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
)
if(CORE_HEADERS)
  install(FILES ${CORE_HEADERS} DESTINATION include/qtc)
endif()

include(CPack)
set(CPACK_GENERATOR "TGZ;ZIP")
set(CPACK_PACKAGE_NAME "QTC-Blockchain")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "QTC - A privacy-focused cryptocurrency")
set(CPACK_PACKAGE_VENDOR "QTC Foundation")
