################################################################################
# Tests
################################################################################

file(GLOB test_sources CONFIGURE_DEPENDS "*.cpp")

add_executable(app_unit_tests ${test_sources})

target_link_libraries(app_unit_tests PUBLIC wmtk::toolkit wmtk::tetwild wmtk::triwild)
target_link_libraries(app_unit_tests PUBLIC wmtk::harmonic_tet)

include(catch2)
target_link_libraries(app_unit_tests PUBLIC Catch2::Catch2)

wmtk_copy_dll(app_unit_tests)


# data()
# set(DATA_DIR "${THIRD_PARTY_DIR}/data/")
# target_compile_definitions(app_unit_tests PUBLIC -DWILDMESHING_TOOLKIT_DATA_DIR=\"${DATA_DIR}\")
target_compile_definitions(app_unit_tests PUBLIC CATCH_CONFIG_ENABLE_BENCHMARKING)

################################################################################
# Register tests
################################################################################

foreach(source IN ITEMS ${test_sources})
    source_group("tests" FILES "${source}")
endforeach()

# Include Catch2 and provide function `catch_discover_tests` to register tests.
FetchContent_GetProperties(catch2)
include("${catch2_SOURCE_DIR}/contrib/Catch.cmake")

catch_discover_tests(app_unit_tests)
