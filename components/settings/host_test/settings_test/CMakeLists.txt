cmake_minimum_required(VERSION 3.16)

# Only build main + its explicit dependencies for the Linux host build,
# instead of pulling in every component from ESP-IDF.
set(COMPONENTS main)

# Make the components under test visible to this standalone test project.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../..")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(settings_test)
