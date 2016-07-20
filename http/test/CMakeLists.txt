# add selftest subdirectory
if(MODULE_service_FOUND)
  add_subdirectory(selftest EXCLUDE_FROM_ALL)
endif()
