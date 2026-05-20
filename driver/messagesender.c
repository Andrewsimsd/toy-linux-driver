// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

#define MSGBUF_WORDS 4U
#define MSGBUF_WRITE_SIZE (MSGBUF_WORDS * sizeof(u32))

struct messagesender_device {
	void __iomem *regs;
	struct mutex lock;
};

static struct messagesender_device *g_device;

static ssize_t messagesender_write(struct file *file, const char __user *buf,
				   size_t count, loff_t *ppos)
{
	u32 words[MSGBUF_WORDS];
	u32 index;

	(void)file;

	if (count != MSGBUF_WRITE_SIZE)
		return -EINVAL;

	if (g_device == NULL || g_device->regs == NULL)
		return -ENODEV;

	if (copy_from_user(&words[0], buf, MSGBUF_WRITE_SIZE) != 0U)
		return -EFAULT;

	mutex_lock(&g_device->lock);
	for (index = 0U; index < MSGBUF_WORDS; index++)
		iowrite32(words[index], g_device->regs + (index * sizeof(u32)));
	mutex_unlock(&g_device->lock);

	*ppos += (loff_t)MSGBUF_WRITE_SIZE;
	return (ssize_t)MSGBUF_WRITE_SIZE;
}

static const struct file_operations messagesender_fops = {
	.owner = THIS_MODULE,
	.write = messagesender_write,
};

static struct miscdevice messagesender_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "messagesender",
	.fops = &messagesender_fops,
	.mode = 0220,
};

static int messagesender_probe(struct platform_device *pdev)
{
	int rc;
	struct resource *resource;
	void __iomem *mapped;

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (resource == NULL)
		return -ENODEV;

	mapped = devm_ioremap_resource(&pdev->dev, resource);
	if (IS_ERR(mapped))
		return PTR_ERR(mapped);

	g_device = devm_kzalloc(&pdev->dev, sizeof(*g_device), GFP_KERNEL);
	if (g_device == NULL)
		return -ENOMEM;

	g_device->regs = mapped;
	mutex_init(&g_device->lock);

	rc = misc_register(&messagesender_miscdev);
	if (rc != 0)
		return rc;

	platform_set_drvdata(pdev, g_device);
	dev_info(&pdev->dev, "messagesender: mapped register window and registered /dev/messagesender\n");
	return 0;
}

static void messagesender_remove(struct platform_device *pdev)
{
	(void)pdev;
	misc_deregister(&messagesender_miscdev);
	g_device = NULL;
}

static const struct of_device_id messagesender_of_match[] = {
	{ .compatible = "toy,messagesender" },
	{ }
};
MODULE_DEVICE_TABLE(of, messagesender_of_match);

static struct platform_driver messagesender_platform_driver = {
	.driver = {
		.name = "messagesender",
		.of_match_table = messagesender_of_match,
	},
	.probe = messagesender_probe,
	.remove_new = messagesender_remove,
};

module_platform_driver(messagesender_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Toy Driver Example");
MODULE_DESCRIPTION("Toy message sender platform+misc driver writing four u32 values to MMIO registers");
