#include <kernel.h>

int module_stop(SceSize argc, const void *args)
{
	return SCE_KERNEL_STOP_SUCCESS;
}

int module_exit()
{
	return SCE_KERNEL_STOP_SUCCESS;
}

int module_start(SceSize argc, void *args)
{
	return SCE_KERNEL_START_SUCCESS;
}