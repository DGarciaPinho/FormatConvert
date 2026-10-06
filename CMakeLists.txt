cmake_minimum_required(VERSION 3.20)
project(ForgeConvert VERSION 0.1.0 LANGUAGES CXX)
option(FORGECONVERT_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
add_library(forgeconvert
  src/codecs/zip.cpp src/xml/writer.cpp src/docx/writer.cpp
  src/pdf/parser.cpp src/pdf/reader.cpp src/pdf/text.cpp
  src/document/layout.cpp src/conversion/convert.cpp)
target_include_directories(forgeconvert PUBLIC include PRIVATE src)
target_compile_features(forgeconvert PUBLIC cxx_std_20)
set_target_properties(forgeconvert PROPERTIES CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(forgeconvert PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
  if(FORGECONVERT_SANITIZERS)
    target_compile_options(forgeconvert PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(forgeconvert PUBLIC -fsanitize=address,undefined)
  endif()
endif()
add_executable(forgeconvert_cli apps/cli/main.cpp)
target_link_libraries(forgeconvert_cli PRIVATE forgeconvert)
set_target_properties(forgeconvert_cli PROPERTIES OUTPUT_NAME forgeconvert)
include(CTest)
if(BUILD_TESTING)
  add_executable(forgeconvert_tests tests/unit/tests.cpp)
  target_link_libraries(forgeconvert_tests PRIVATE forgeconvert)
  target_include_directories(forgeconvert_tests PRIVATE src)
  add_test(NAME forgeconvert_unit COMMAND forgeconvert_tests)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_FOUND)
    add_test(NAME forgeconvert_independent COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/validate.py
      $<TARGET_FILE:forgeconvert_cli> ${CMAKE_CURRENT_BINARY_DIR}/integration)
    find_program(FORGECONVERT_GS gs)
    find_program(FORGECONVERT_PDFTOTEXT pdftotext)
    if(FORGECONVERT_GS AND FORGECONVERT_PDFTOTEXT)
      add_test(NAME forgeconvert_external_fixture COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/external_fixture.py
        $<TARGET_FILE:forgeconvert_cli> ${FORGECONVERT_GS} ${FORGECONVERT_PDFTOTEXT}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/independent.ps
        ${CMAKE_CURRENT_BINARY_DIR}/external)
    endif()
  endif()
endif()
