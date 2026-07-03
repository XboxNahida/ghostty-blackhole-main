# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "CMakeFiles\\appBlakholeUI_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\appBlakholeUI_autogen.dir\\ParseCache.txt"
  "appBlakholeUI_autogen"
  )
endif()
