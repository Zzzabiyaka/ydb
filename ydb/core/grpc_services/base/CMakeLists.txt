
# This file was gererated by the build system used internally in the Yandex monorepo.
# Only simple modifications are allowed (adding source-files to targets, adding simple properties
# like target_include_directories). These modifications will be ported to original
# ya.make files by maintainers. Any complex modifications which can't be ported back to the
# original buildsystem will not be accepted.



add_library(core-grpc_services-base INTERFACE)
target_link_libraries(core-grpc_services-base INTERFACE
  contrib-libs-cxxsupp
  yutil
  cpp-grpc-server
  cpp-string_utils-quote
  ydb-core-base
  ydb-core-grpc_streaming
  api-protos
  cpp-client-resources
  yql-public-issue
)
