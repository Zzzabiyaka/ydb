
# This file was gererated by the build system used internally in the Yandex monorepo.
# Only simple modifications are allowed (adding source-files to targets, adding simple properties
# like target_include_directories). These modifications will be ported to original
# ya.make files by maintainers. Any complex modifications which can't be ported back to the
# original buildsystem will not be accepted.



add_library(common-stat-static)
target_compile_options(common-stat-static PRIVATE
  -DUDF_ABI_VERSION_MAJOR=2
  -DUDF_ABI_VERSION_MINOR=9
  -DUDF_ABI_VERSION_PATCH=0
)
target_link_libraries(common-stat-static PUBLIC
  contrib-libs-cxxsupp
  yutil
  contrib-libs-protobuf
  util-draft
  yql-public-udf
)
target_proto_messages(common-stat-static PRIVATE
  ${CMAKE_SOURCE_DIR}/ydb/library/yql/udfs/common/stat/static/tdigest.proto
)
target_sources(common-stat-static PRIVATE
  ${CMAKE_SOURCE_DIR}/ydb/library/yql/udfs/common/stat/static/static_udf.cpp
  ${CMAKE_SOURCE_DIR}/ydb/library/yql/udfs/common/stat/static/tdigest.cpp
)
target_proto_addincls(common-stat-static
  ./
  ${CMAKE_SOURCE_DIR}/
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/contrib/libs/protobuf/src
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/contrib/libs/protobuf/src
)
target_proto_outs(common-stat-static
  --cpp_out=${CMAKE_BINARY_DIR}/
  --cpp_styleguide_out=${CMAKE_BINARY_DIR}/
)
