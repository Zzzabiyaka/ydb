
# This file was gererated by the build system used internally in the Yandex monorepo.
# Only simple modifications are allowed (adding source-files to targets, adding simple properties
# like target_include_directories). These modifications will be ported to original
# ya.make files by maintainers. Any complex modifications which can't be ported back to the
# original buildsystem will not be accepted.



add_library(blobstorage-vdisk-localrecovery)
target_link_libraries(blobstorage-vdisk-localrecovery PUBLIC
  contrib-libs-cxxsupp
  yutil
  ydb-core-base
  core-blobstorage-pdisk
  ydb-core-protos
)
target_sources(blobstorage-vdisk-localrecovery PRIVATE
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/vdisk/localrecovery/localrecovery_defs.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/vdisk/localrecovery/localrecovery_logreplay.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/vdisk/localrecovery/localrecovery_public.cpp
  ${CMAKE_SOURCE_DIR}/ydb/core/blobstorage/vdisk/localrecovery/localrecovery_readbulksst.cpp
)
