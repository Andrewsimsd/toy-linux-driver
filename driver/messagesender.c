// SPDX-License-Identifier: GPL-2.0  // License tag required by Linux kernel tooling and legal policy.
#include <linux/fs.h>               // File operation types like struct file_operations.
#include <linux/init.h>             // __init and __exit markers for module lifecycle functions.
#include <linux/kernel.h>           // pr_info and core kernel helpers/macros.
#include <linux/miscdevice.h>       // misc_register/misc_deregister and struct miscdevice API.
#include <linux/module.h>           // module_init/module_exit and module metadata macros.
#include <linux/mutex.h>            // mutex primitives used to protect shared state.
#include <linux/uaccess.h>          // copy_from_user for safe user->kernel memory copy.

#define MSGBUF_CAPACITY 1024U                    // Ring-buffer slot count; bounded to cap memory use.
#define MSGBUF_WORDS 4U                          // Fixed protocol width: exactly four u32 values.
#define MSGBUF_WRITE_SIZE (MSGBUF_WORDS * sizeof(u32)) // Required byte count per write syscall (16 bytes on Linux).

struct message_slot {                // One logical message as stored inside kernel memory.
	u32 words[MSGBUF_WORDS];           // Fixed-size payload avoids variable-length parsing complexity.
};

struct message_sender_device {       // Entire mutable device state for this toy driver.
	struct message_slot slots[MSGBUF_CAPACITY]; // In-kernel storage for recent writes; fixed size prevents unbounded growth.
	u32 write_index;                   // Monotonic counter used for circular indexing into slots.
	struct mutex lock;                 // Mutex serializes concurrent writers and prevents data races.
};

static struct message_sender_device g_device; // Global singleton device instance for this module.

static ssize_t messagesender_write(struct file *file, const char __user *buf, // Write callback invoked by VFS on write(2).
				   size_t count, loff_t *ppos)                              // count is user byte length; ppos is file position pointer.
{
	struct message_slot incoming;      // Stack-local staging buffer avoids partial writes into global state.
	u32 slot_index;                    // Computed destination index inside bounded circular buffer.

	(void)file;                        // Explicitly mark unused in this toy driver implementation.

	if (count != MSGBUF_WRITE_SIZE)    // Strict length validation blocks malformed protocol frames.
		return -EINVAL;                 // -EINVAL signals caller sent invalid argument/size.

	if (copy_from_user(&incoming.words[0], buf, MSGBUF_WRITE_SIZE) != 0U) // Safe copy checks access permissions and faults.
		return -EFAULT;                 // -EFAULT indicates bad userspace pointer or inaccessible memory.

	mutex_lock(&g_device.lock);        // Enter critical section before touching shared ring-buffer state.
	slot_index = g_device.write_index % MSGBUF_CAPACITY; // Wrap index to stay within array bounds.
	g_device.slots[slot_index] = incoming; // Single struct assignment stores all four words atomically wrt lock.
	g_device.write_index++;             // Advance logical write cursor for next message.
	mutex_unlock(&g_device.lock);       // Exit critical section quickly to reduce contention.

	*ppos += (loff_t)MSGBUF_WRITE_SIZE; // Advance file position for conventional write semantics.
	return (ssize_t)MSGBUF_WRITE_SIZE;  // Report full success; no short writes in this fixed-size protocol.
}

static const struct file_operations messagesender_fops = { // VFS operation table for /dev/messagesender.
	.owner = THIS_MODULE,            // Pins module while file descriptor is open, preventing unload races.
	.write = messagesender_write,    // Connect write(2) to our handler above.
};

static struct miscdevice messagesender_miscdev = { // Misc character device descriptor used by misc subsystem.
	.minor = MISC_DYNAMIC_MINOR,     // Request dynamic minor allocation to avoid hard-coded conflicts.
	.name = "messagesender",         // Device node name appears as /dev/messagesender.
	.fops = &messagesender_fops,     // File operation dispatch table.
	.mode = 0220,                    // Owner/group write-only; no read permissions by default.
};

static int __init messagesender_init(void) // Module load hook called when inserted via insmod/modprobe.
{
	int rc;                           // Return code from misc_register.

	mutex_init(&g_device.lock);       // Initialize mutex before any concurrent access can occur.
	g_device.write_index = 0;         // Explicit reset makes startup state deterministic.

	rc = misc_register(&messagesender_miscdev); // Register misc device and create /dev node infrastructure.
	if (rc != 0)                      // Handle registration failure explicitly.
		return rc;                     // Propagate kernel error code to loader for diagnostics.

	pr_info("messagesender: registered /dev/messagesender\n"); // Informational kernel log for successful load.
	return 0;                         // Zero indicates successful module initialization.
}

static void __exit messagesender_exit(void) // Module unload hook called during rmmod.
{
	misc_deregister(&messagesender_miscdev); // Remove misc device and detach from VFS namespace.
	pr_info("messagesender: unloaded\n");    // Log unload event for operational observability.
}

module_init(messagesender_init);     // Declare init entrypoint symbol to module loader.
module_exit(messagesender_exit);     // Declare exit entrypoint symbol to module loader.

MODULE_LICENSE("GPL");               // Required to avoid kernel taint and enable GPL-only exports.
MODULE_AUTHOR("Toy Driver Example"); // Human-readable metadata shown by modinfo.
MODULE_DESCRIPTION("Toy message sender misc char device"); // Short functional summary shown by modinfo.
