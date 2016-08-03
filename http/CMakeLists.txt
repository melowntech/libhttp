set(http_DEPENDS )

define_module(LIBRARY http
  DEPENDS utility dbglog Boost CURL)

set(http_SOURCES
  http.hpp http.cpp
  contentgenerator.hpp contentgenerator.cpp
  contentfetcher.hpp
  resourcefetcher.hpp resourcefetcher.cpp

  detail/types.hpp
  detail/detail.hpp
  detail/acceptor.hpp
  detail/serverconnection.hpp

  detail/client.cpp
)

add_library(http STATIC ${http_SOURCES})
buildsys_library(http)
target_link_libraries(http ${MODULE_LIBRARIES})
buildsys_target_compile_definitions(http ${MODULE_DEFINITIONS})

add_subdirectory(test)
