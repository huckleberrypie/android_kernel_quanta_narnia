

#include "met_drv.h"


int met_tag_init(void)
{
	return 0;
}

int met_tag_uninit(void)
{
	return 0;
}

int met_tag_start(unsigned int class_id, const char *name)
{
	return 0;
}

int met_tag_end(unsigned int class_id, const char *name)
{
	return 0;
}

int met_tag_oneshot(unsigned int class_id, const char *name, unsigned int value)
{
	return 0;
}

int met_tag_dump(unsigned int class_id, const char *name, void *data, unsigned int length)
{
	return 0;
}

int met_tag_disable(unsigned int class_id)
{
	return 0;
}

int met_tag_enable(unsigned int class_id)
{
	return 0;
}

int met_set_dump_buffer(int size)
{
	return 0;
}

int met_save_dump_buffer(const char *pathname)
{
	return 0;
}

int met_save_log(const char *pathname)
{
	return 0;
}

#include <linux/module.h>
#include <asm/uaccess.h>

EXPORT_SYMBOL(met_tag_start);
EXPORT_SYMBOL(met_tag_end);
EXPORT_SYMBOL(met_tag_oneshot);
EXPORT_SYMBOL(met_tag_dump);
EXPORT_SYMBOL(met_tag_disable);
EXPORT_SYMBOL(met_tag_enable);
EXPORT_SYMBOL(met_set_dump_buffer);
EXPORT_SYMBOL(met_save_dump_buffer);
EXPORT_SYMBOL(met_save_log);

