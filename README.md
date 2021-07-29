# PVR_PSP2
Driver layer GPU libraries and tests for PSP2

### Currently this project include:

1. Common and PSP2-specific GPU  driver headers.
2. Extension library for GPU driver (libgpu_es4_ext), which includes:
 - Full Display Class API implementation;
 - Lowlevel USE codegen code;
 - OS and kernel bridge extensions.
3. PVR2D port for PSP2.
4. Various unittests to check basic driver features.

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
| gles1test | WIP  |

# Library port status

| Library  | Status |
| ------------- | ------------- |
| PVR2D | Completed  |
| WSEGL | WIP  |
| IMGEGL | WIP  |
| OpenGLES1 | WIP  |
