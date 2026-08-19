find_package(doctest CONFIG REQUIRED)

add_executable(engine_tests
    main.cpp
    core/EngineVersionTests.cpp
    ecs/WorldTests.cpp
    jobs/JobSystemTests.cpp
    math/CameraTests.cpp
    platform/InputSystemTests.cpp
    renderer/CookedMeshTests.cpp
    script/ComponentHandleTests.cpp
    script/ScriptRegistryTests.cpp
)

target_link_libraries(engine_tests PRIVATE engine_core doctest::doctest)
pi_engine_set_warnings(engine_tests)

add_test(NAME engine_tests COMMAND engine_tests)

# tests/ecs/, tests/jobs/, tests/math/ (M2) — World, JobSystem, Camera.
# tests/platform/, tests/script/ (M3) — InputSystem edge detection, ComponentHandle
# validity across ECS rearrangements, ScriptRegistry factory round-trip.
# tests/renderer/ (M6) — WriteCookedMesh/LoadCookedMesh round-trip + malformed-input
# rejection; doesn't touch tools/cooker's cgltf-parsing half (that needs a real .glb, only
# exercised through the samples themselves and the manual `cooker` CLI run).
# All of the above run on desktop x86_64, no Pi hardware needed.
