# PVR_PSP2
Driver layer GPU libraries and tests for PSP2

# Unittest status

| Test  | Status |
| ------------- | ------------- |
| services_test | Passed  |
| sgx_init_test | Passed  |
| sgx_flip_test | Passed  |
| sgx_render_flip_test | GPU crash (page fault) on SGXKickTA().<br />Either memory mapping issue or wrong SGX_KICKTA struct |
