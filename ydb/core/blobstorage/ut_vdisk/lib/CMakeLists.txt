
# This file was gererated by the build system used internally in the Yandex monorepo.
# Only simple modifications are allowed (adding source-files to targets, adding simple properties
# like target_include_directories). These modifications will be ported to original
# ya.make files by maintainers. Any complex modifications which can't be ported back to the
# original buildsystem will not be accepted.



add_library(blobstorage-ut_vdisk-lib)
target_link_libraries(blobstorage-ut_vdisk-lib PUBLIC
  contrib-libs-cxxsupp
  yutil
  cpp-actors-protos
  cpp-http-io
  cpp-openssl-io
  cpp-string_utils-url
  cpp-testing-unittest
  ydb-core-base
  ydb-core-blobstorage
  core-blobstorage-backpressure
  core-blobstorage-base
  core-blobstorage-crypto
  core-blobstorage-pdisk
  core-blobstorage-vdisk
  blobstorage-vdisk-common
  blobstorage-vdisk-huge
  vdisk-hulldb-base
  blobstorage-vdisk-repl
  blobstorage-vdisk-skeleton
  blobstorage-vdisk-synclog
  ydb-core-erasure
  ydb-library-pdisk_io
)
target_sources(blobstorage-ut_vdisk-lib PRIVATE
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/dataset.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/helpers.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/http_client.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/prepare.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/test_brokendevice.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/test_dbstat.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/test_defrag.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/test_faketablet.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/test_gc.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/test_huge.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/test_load.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/test_localrecovery.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/test_many.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/test_outofspace.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/test_repl.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/test_simplebs.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/test_synclog.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/ut_vdisk/lib/vdisk_mock.cpp
)
