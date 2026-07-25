# SDF 2D Decomposition Demo

![img](res/comparison.png)
*Original shape on the left, decomposed shape with smooth union on the right*

2D SDF Decomposition implementation for [this article](https://fiven1.github.io/web/blogs/sdf_decomposition/sdf_decomposition/) about SDF decomposition to n-spheres.

## Dependencies
- [Sokol](https://github.com/floooh/sokol)
- Vecmath
- [ImGui](https://github.com/ocornut/imgui)

To include these libraries from your own library directory, cheange the include directory in the `premake5.lua` folder.
Note that ImGui is wrapped by the [cimgui](https://github.com/cimgui/cimgui) wrapper and statically linked.
