
# This file was gererated by the build system used internally in the Yandex monorepo.
# Only simple modifications are allowed (adding source-files to targets, adding simple properties
# like target_include_directories). These modifications will be ported to original
# ya.make files by maintainers. Any complex modifications which can't be ported back to the
# original buildsystem will not be accepted.



add_library(contrib-libs-farmhash)
target_include_directories(contrib-libs-farmhash PUBLIC
  ${CMAKE_SOURCE_DIR}/contrib/libs/farmhash/include
)
target_include_directories(contrib-libs-farmhash PRIVATE
  ${CMAKE_SOURCE_DIR}/contrib/libs/farmhash/include
)
target_link_libraries(contrib-libs-farmhash PUBLIC
  contrib-libs-cxxsupp
  yutil
  farmhash-arch-sse41
  farmhash-arch-sse42
  farmhash-arch-sse42_aesni
)
target_sources(contrib-libs-farmhash PRIVATE
  ${CMAKE_SOURCE_DIR}/contrib/libs/farmhash/farmhashuo.cc
  ${CMAKE_SOURCE_DIR}/contrib/libs/farmhash/farmhashxo.cc
  ${CMAKE_SOURCE_DIR}/contrib/libs/farmhash/farmhashna.cc
  ${CMAKE_SOURCE_DIR}/contrib/libs/farmhash/farmhashmk.cc
  ${CMAKE_SOURCE_DIR}/contrib/libs/farmhash/farmhashcc.cc
  ${CMAKE_SOURCE_DIR}/contrib/libs/farmhash/farmhash_iface.cc
)
