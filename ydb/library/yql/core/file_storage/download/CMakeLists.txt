
# This file was gererated by the build system used internally in the Yandex monorepo.
# Only simple modifications are allowed (adding source-files to targets, adding simple properties
# like target_include_directories). These modifications will be ported to original
# ya.make files by maintainers. Any complex modifications which can't be ported back to the
# original buildsystem will not be accepted.



add_library(core-file_storage-download)
target_link_libraries(core-file_storage-download PUBLIC
  contrib-libs-cxxsupp
  yutil
  core-file_storage-proto
  cpp-protobuf-util
)
target_sources(core-file_storage-download PRIVATE
  ${CMAKE_SOURCE_DIR}/ydb/library/yql/core/file_storage/download/download_config.cpp
  ${CMAKE_SOURCE_DIR}/ydb/library/yql/core/file_storage/download/download_stream.cpp
)
