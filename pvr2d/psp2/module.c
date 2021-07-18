#include <kernel.h>

int __module_stop(SceSize argc, const void *args)
{
	return SCE_KERNEL_STOP_SUCCESS;
}

int __module_exit()
{
	return SCE_KERNEL_STOP_SUCCESS;
}

int __module_start(SceSize argc, void *args)
{
	return SCE_KERNEL_START_SUCCESS;
}