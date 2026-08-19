find_package(doctest CONFIG REQUIRED)

add_executable(engine_tests
    main.cpp
    asset/AssetGuidTests.cpp
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
# tests/renderer/ (M6, extended M7) — WriteCookedMesh/LoadCookedMesh round-trip (now
# including the embedded Asset GUID) + malformed/stale-version-input rejection; doesn't
# touch tools/cooker's cgltf-parsing or .meta-sidecar halves (those need a real .glb/
# filesystem, only exercised through the samples themselves and the manual `cooker` CLI
# run).
# tests/asset/ (M7) — AssetGuid generation (distinctness), string round-trip, malformed
# input rejection.
# All of the above run on desktop x86_64, no Pi hardware needed.
