# PVR_PSP2
Driver layer GPU libraries and tests for PSP2

### Currently this project include:

1. Common and PSP2-specific GPU  driver headers.
2. Extension library for GPU driver (libgpu_es4_ext), which includes:
 - Full Display Class API implementation;
 - Lowlevel USE codegen code;
 - OS and kernel bridge extensions.
3. PVR2D port for PSP2.
4. IMGEGL port for PSP2.
5. WSEGL port for PSP2.
6. OpenGL ES v1.1 port for PSP2.
7. OpenGL ES v2 port for PSP2.
8. Various unittests to check basic driver features.

# Usage specifics

- Check default apphint values before running your application and make sure everything is at expected values.
Default apphint values can be found [here](https://github.com/GrapheneCt/PVR_PSP2/blob/main/gpu_es4_ext/eurasia/services4/srvclient/env/playstation/common/pvr_apphint.c#L501)

# Known issues

### GLES1:

1. GLES1 and GLES2 libraries upload textures to separate GPU memory on demand after validation. That may cause slowdowns.
- Required solution: render textures directly from intermediate memory.

### GLES2:

1. GLES1 and GLES2 libraries upload textures to separate GPU memory on demand after validation. That may cause slowdowns.
- Required solution: render textures directly from intermediate memory.

2. Major slowdown during vector instruction optimizations in GLSL shader compiler, specifically GenerateVectorDualIssue() function.

# Unittest status

### Basic:

| Test  | Status |
| ------------- | ------------- |
| services_test | Passed  |
| sgx_init_test | Passed  |
| sgx_flip_test | Passed  |
| sgx_render_flip_test | Passed |

### Libs:

| Test  | Status |
| ------------- | ------------- |
| pvr2d_test | Passed  |
| gles1test1 | Passed  |
| gles2test1 | Passed  |

# Library port status

| Library  | Status |
| ------------- | ------------- |
| PVR2D | Completed  |
| WSEGL | Completed  |
| IMGEGL | Completed  |
| OpenGLES1 | Completed  |
| OpenGLES2 | Completed  |
