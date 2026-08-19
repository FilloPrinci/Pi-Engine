# m7_lod

LOD generation (docs/01 section 12.2), the fifth and last of five Asset Pipeline steps
done after M6. `cooker mesh` now runs every source mesh through meshoptimizer: LOD0 is
just vertex-cache optimized (`meshopt_optimizeVertexCache`); LOD1/LOD2 are decimated to
roughly 50%/25% of LOD0's triangle count via `meshopt_simplify`, then vertex-cache
optimized too. All LODs share one vertex buffer (`engine::renderer::CookedMesh.h` version
4) -- simplification only ever needs to shrink the index buffer.

`assets/m7_lod_sphere.gltf` is a hand-generated subdivided icosphere (642 vertices, 1280
triangles) -- `assets/m1_cube.glb` and `assets/m7_quad.gltf` are both far too simple to
show a visible difference between LOD levels.

This sample loads every LOD's index buffer up front (one `RHIBuffer` each, all indexing
into the same shared vertex buffer) and lets **Space** cycle which one is bound and drawn
-- the window title shows the current LOD index and its triangle count. There is no
Hardware Profile System yet (CLAUDE.md section 8) to pick a LOD automatically by camera
distance or hardware tier; that selection logic is future work once one exists. This
milestone's exit criterion is just proving the Cooker can generate and store usable LODs,
and that the runtime can load and switch between them.
