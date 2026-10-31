- add tornado axis illustration
- add floor and its mesh -> ok
- load chicken and creeper GLB models -> ok

- how to make objects interact with each other, e.g. the floor -> ok
- make the tornado shrink until the bottom, to form a shape like trumpet (v-shape)
- eleminate the force near the center of the tornado so that objects having a free fall here till the bottom, then being blew up back


## GLB models

Build and run from the repository root:

```sh
cmake --preset default
cmake --build --preset default
./build/default/quacknado
```

The scene spawns 100 chickens and 100 creepers, alternating between
`assets/models/chicken.glb` and `assets/models/creeper.glb`. CMake copies these
files next to the executable; launching from a different directory also works.

Change `visual_height_`, collision `half_extent_`, and `mass_` in
`src/simulation/simulation.cpp` to configure each type. Visual height preserves
the GLB's proportions; collision boxes remain hand-authored and independent.
The renderer centers the imported scene bounds on the Jolt body's position and
applies its rotation. Each model's GPU buffers and textures are shared by all
instances.

`src/assets/glb_asset.cpp` reads static triangle meshes, node matrix/TRS
transforms, float positions/UVs, unsigned indices and embedded PNG/JPEG base
color textures. cgltf reads the GLB structure; stb_image decodes the images. metal-cpp uploads
RGBA pixels to sRGB Metal textures and generates mipmaps. The model
shader uses nearest filtering and supports alpha masks (the chicken material).
This small loader covers the supplied assets; it does not implement animation,
skinning, PBR lighting, blended materials, external images or glTF extensions.
Unsupported required features fail during initialization with a log message.
