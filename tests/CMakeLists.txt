find_package(doctest CONFIG REQUIRED)

add_executable(engine_tests
    main.cpp
    core/EngineVersionTests.cpp
)

target_link_libraries(engine_tests PRIVATE engine_core doctest::doctest)
pi_engine_set_warnings(engine_tests)

add_test(NAME engine_tests COMMAND engine_tests)

# As modules land, mirror engine/src/<module>/ here:
#   tests/ecs/    — World, ComponentHandle validity across rearrangements (M2)
#   tests/jobs/   — JobSystem task graph execution/waiting (M2)
#   tests/math/   — camera/transform math (M1+), runs on desktop x86_64, no Pi hardware needed
