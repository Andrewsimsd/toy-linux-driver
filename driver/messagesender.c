// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#define MSGBUF_CAPACITY 1024U
#define MSGBUF_WORDS 4U
#define MSGBUF_WRITE_SIZE (MSGBUF_WORDS * sizeof(u32))

struct message_slot {
	u32 words[MSGBUF_WORDS];
};

struct message_sender_device {
	struct message_slot slots[MSGBUF_CAPACITY];
	u32 write_index;
	struct mutex lock;
};

static struct message_sender_device g_device;

static ssize_t messagesender_write(struct file *file, const char __user *buf,
				   size_t count, loff_t *ppos)
{
	struct message_slot incoming;
	u32 slot_index;

	if (count != MSGBUF_WRITE_SIZE)
		return -EINVAL;

	if (copy_from_user(&incoming.words[0], buf, MSGBUF_WRITE_SIZE) != 0U)
		return -EFAULT;

	mutex_lock(&g_device.lock);
	slot_index = g_device.write_index % MSGBUF_CAPACITY;
	g_device.slots[slot_index] = incoming;
	g_device.write_index++;
	mutex_unlock(&g_device.lock);

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

static int __init messagesender_init(void)
{
	int rc;

	mutex_init(&g_device.lock);
	g_device.write_index = 0;

	rc = misc_register(&messagesender_miscdev);
	if (rc != 0)
		return rc;

	pr_info("messagesender: registered /dev/messagesender\n");
	return 0;
}

static void __exit messagesender_exit(void)
{
	misc_deregister(&messagesender_miscdev);
	pr_info("messagesender: unloaded\n");
}

module_init(messagesender_init);
module_exit(messagesender_exit);

MODULE_DESCRIPTION("Toy message sender sink driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("toy-linux-driver example");
