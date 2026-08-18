find_package(doctest CONFIG REQUIRED)

add_executable(engine_tests
    main.cpp
    core/EngineVersionTests.cpp
    ecs/WorldTests.cpp
    jobs/JobSystemTests.cpp
    math/CameraTests.cpp
)

target_link_libraries(engine_tests PRIVATE engine_core doctest::doctest)
pi_engine_set_warnings(engine_tests)

add_test(NAME engine_tests COMMAND engine_tests)

# tests/ecs/, tests/jobs/, tests/math/ (M2) — World, JobSystem, Camera. ComponentHandle
# validity across rearrangements is script/ComponentHandle.h (M3), not yet in the tree.
# All of the above run on desktop x86_64, no Pi hardware needed.
