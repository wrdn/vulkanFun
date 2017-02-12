REM Make output directory
mkdir output

REM Compiling HLSL to SPIR-V
glslangValidator -V -S vert -e VSMain -D hlsl_test_shaders.hlsl -o output/vert.spv
glslangValidator -V -S frag -e PSMain -D hlsl_test_shaders.hlsl -o output/frag.spv

REM Converting SPIR-V to GLSL and dumping resources to TTY
spirv-cross output/vert.spv --vulkan-semantics --remove-unused-variables --dump-resources --output output/shader.vert
spirv-cross output/frag.spv --vulkan-semantics --remove-unused-variables --dump-resources --output output/shader.frag

REM Running SPIR-V Remap to remove dead code and variables, among other stuff, from spv files
REM Consider passing --do-everything to the command in production to ensure names
REM are stripped aswell
cd output/
spirv-remap -vvvv --dce all --input vert.spv --output .
spirv-remap -vvvv --dce all --input frag.spv --output .

PAUSE