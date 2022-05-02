#define VERSION_MAJOR 2
#define VERSION_MINOR 0


#include <linux/usb.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/compat.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/hid.h>
#include <linux/hiddev.h>

MODULE_LICENSE("GPL");

/*
 * Or with defines, like this:
 */
MODULE_AUTHOR("Kromek");	/* Who wrote this module? */
MODULE_DESCRIPTION("Driver for Kromek HID based devices (gr1, tn15 etc..");	/* What does this module do */

/* Max entries that are buffered when data is recieved. Data comes in every 1ms max */
#define MAX_BUFFERED_VALUES 16000

/* Data is recieved from the device in fixed sizes */
#define INPUT_BUFFER_SIZE 63

/* Timeout for writing data to the device (blocking) */
#define WRITE_TIMEOUT_MS (HZ * 2)

#define KR_MINOR_BASE 0xAB

#define USB_VENDOR_ID		0x04d8

#define HIDREPORTNUMBER_SETLLD 0x01
#define HIDREPORTNUMBER_SETLLD_CHANNEL 0x09
#define HIDREPORTNUMBER_SETGAIN 0x02
#define HIDREPORTNUMBER_SETPOLARITY 0x03
#define HIDREPORTNUMBER_SETBIAS16 0x07

#define PRODUCT_K102 0x011
#define PRODUCT_GR1 0x000
#define PRODUCT_GR1A 0x101
#define PRODUCT_RADANGEL 0x100
#define PRODUCT_SIGMA25 0x022
#define PRODUCT_SIGMA50 0x023
#define PRODUCT_TN15 0x030


/* table of devices that work with this driver */
static const struct usb_device_id device_table[] = {
	{ USB_DEVICE(USB_VENDOR_ID, PRODUCT_K102) }, 
	{ USB_DEVICE(USB_VENDOR_ID, PRODUCT_GR1) }, 
	{ USB_DEVICE(USB_VENDOR_ID, PRODUCT_GR1A) }, 
	{ USB_DEVICE(USB_VENDOR_ID, PRODUCT_RADANGEL) },
	{ USB_DEVICE(USB_VENDOR_ID, PRODUCT_SIGMA25) },
	{ USB_DEVICE(USB_VENDOR_ID, PRODUCT_SIGMA50) },
	{ USB_DEVICE(USB_VENDOR_ID, PRODUCT_TN15) }, 
	{ }					/* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, device_table);

static const int K102_report_ids[] = {HIDREPORTNUMBER_SETLLD, HIDREPORTNUMBER_SETGAIN, HIDREPORTNUMBER_SETPOLARITY, 0 };
static const int GR1_report_ids[] = {HIDREPORTNUMBER_SETLLD, HIDREPORTNUMBER_SETGAIN, 0};
static const int GR1A_report_ids[] = {HIDREPORTNUMBER_SETLLD, HIDREPORTNUMBER_SETGAIN, 0};
static const int RadAngel_report_ids[] = {HIDREPORTNUMBER_SETLLD, HIDREPORTNUMBER_SETGAIN, 0};
static const int SIGMA25_report_ids[] = {HIDREPORTNUMBER_SETLLD_CHANNEL, HIDREPORTNUMBER_SETGAIN, HIDREPORTNUMBER_SETBIAS16, 0};
static const int SIGMA50_report_ids[] = {HIDREPORTNUMBER_SETLLD_CHANNEL, HIDREPORTNUMBER_SETGAIN, HIDREPORTNUMBER_SETBIAS16, 0};
static const int TN15_report_ids[] = {HIDREPORTNUMBER_SETLLD_CHANNEL, HIDREPORTNUMBER_SETGAIN, HIDREPORTNUMBER_SETBIAS16, 0};

static int kr_probe(struct usb_interface *interface, const struct usb_device_id *id);
static void kr_disconnect(struct usb_interface *interface);

static struct usb_driver kr_driver = 
{
	.name = "kromek",
	.probe = kr_probe,
	.disconnect = kr_disconnect,
	.id_table = device_table,
};

struct kr_device
{
	struct usb_device *usbdev;
	struct usb_interface *usb_interface;
	
	/* Reference counted object */
	struct kref refCount;

	/* Input */
	struct urb *inputUrb;
	struct urb *inputUrbB;
	unsigned char *input_buffer;
	unsigned char *input_bufferB;
	size_t input_buffer_size;
	__u8 interrupt_in_addr;
	int input_interval;

	/* Linked list for buffering input data */
	struct list_head input_list_head;
	spinlock_t input_list_lock;
	atomic_t input_list_length;
	atomic_t continueRunning;

	/* Wait queue for events from interrupt */
	wait_queue_head_t wait_queue;

	/* Output */
	__u8 interrupt_out_addr;
	
	/* List of accepted report IDs for output to the device */
	const int *validReportIds;
};

struct kr_data_list_item
{
	struct list_head list_head;	/* Linked list required header */
	unsigned char data[INPUT_BUFFER_SIZE];
};

/* Release method for the reference counted device object */
void device_release(struct kref *ref)
{
	struct kr_device *data = container_of(ref, struct kr_device, refCount);
	kfree(data);
}

/* Data recieved via the interrupt. Add to the list and resubmit for another set of data */
static void kr_in_complete(struct urb *urb)
{
	struct kr_device *device;
	struct kr_data_list_item *newItem;
	unsigned long flags;

	if (urb->status != 0 || urb->actual_length != INPUT_BUFFER_SIZE)
	{
		/* If not cancelled then display the error in the output log */
		if (urb->status != -2)
		{
			printk("Kromek: URB error - %d", urb->status); 
		}
		return;
	}

	device = urb->context;

	/* Create a container for the data */
	newItem = kmalloc(sizeof(struct kr_data_list_item), GFP_ATOMIC);
	if (!newItem)
	{
		return;
	}

	memcpy(newItem->data, urb->transfer_buffer, INPUT_BUFFER_SIZE);

	/* Add the data to the list */
	spin_lock_irqsave(&device->input_list_lock, flags);

	/* Prevent killing the system in cases where the data is not being read out. If the buffer is full
	then drop this data */
	if (atomic_read(&device->input_list_length) < 60000)
	{
		list_add_tail(&newItem->list_head, &device->input_list_head);
		atomic_inc(&device->input_list_length);
	}
	spin_unlock_irqrestore(&device->input_list_lock, flags);

	usb_submit_urb(urb, GFP_ATOMIC);
	wake_up_interruptible(&device->wait_queue);
}

static int kr_open(struct inode *inode, struct file *file)
{
	int ret;
	struct usb_interface *interface;
	struct kr_device *device;
	
	interface = usb_find_interface(&kr_driver, iminor(inode));
	device = usb_get_intfdata(interface);
	
	/* Increment the reference counter and keep a reference to the device in the file structs private data*/
	kref_get(&device->refCount);
	file->private_data = device;

	/* Start recieving data from the interrupt input endpoint */
	device->inputUrb = usb_alloc_urb(0, GFP_KERNEL);
	usb_fill_int_urb(device->inputUrb, 
			 device->usbdev,
			 usb_rcvintpipe(device->usbdev, device->interrupt_in_addr),
			 device->input_buffer,
			 device->input_buffer_size,
			 kr_in_complete,
			 device,
			 device->input_interval);

	ret = usb_submit_urb(device->inputUrb, GFP_KERNEL);
	if (ret != 0)
	{
		printk("kromek: Unable to submit input interrupt urb\n");
		goto err_release_urb;
	}

	/* Make a second request for data into the queue so that one is always waiting even when one is
	   being processed */
	device->inputUrbB = usb_alloc_urb(0, GFP_KERNEL);
	usb_fill_int_urb(device->inputUrbB, 
			 device->usbdev,
			 usb_rcvintpipe(device->usbdev, device->interrupt_in_addr),
			 device->input_bufferB,
			 device->input_buffer_size,
			 kr_in_complete,
			 device,
			 device->input_interval);

	ret = usb_submit_urb(device->inputUrbB, GFP_KERNEL);
	if (ret != 0)
	{
		printk("kromek: Unable to submit input interrupt urb B\n");
		goto err_release_urb;
	}
	
	atomic_set(&device->continueRunning, 1);
	
	return 0;

err_release_urb:
	usb_free_urb(device->inputUrb);
	usb_free_urb(device->inputUrbB);
	device->inputUrb = NULL;
	
	file->private_data = NULL;
	kref_put(&device->refCount, device_release);
	
	return ret;
}

/* Close the device and clear the input data */
static int CloseDevice(struct kr_device *device)
{
	struct kr_data_list_item *temp = NULL;
	struct kr_data_list_item *iterator = NULL;
	unsigned long spin_flags;

	if (device == NULL)
	{
		return 0;
	}
	
	/* It the input is being read, cancel it */
	if (device->inputUrb)
	{
		/* Wait until the input is fully cancelled before continuing */
		usb_kill_urb(device->inputUrb);
		usb_free_urb(device->inputUrb);
		device->inputUrb = NULL;
	}
	
	if (device->inputUrbB)
	{
		/* Wait until the input is fully cancelled before continuing */
		usb_kill_urb(device->inputUrbB);
		usb_free_urb(device->inputUrbB);
		device->inputUrbB = NULL;
	}
	
	/* Make sure the input list is empty */	
	spin_lock_irqsave(&device->input_list_lock, spin_flags);
	list_for_each_entry_safe(iterator, temp, &device->input_list_head, list_head)
	{
		list_del(&iterator->list_head);
		kfree(iterator);
	}

	atomic_set(&device->input_list_length, 0);
	spin_unlock_irqrestore(&device->input_list_lock, spin_flags);

	// Wake the wait queue in case of an outstanding blocking read
	atomic_set(&device->continueRunning, 0);
	wake_up_interruptible(&device->wait_queue);
	
	return 0;
}

/* Close device callback for the driver. Note: This can occur after the device has been disconnected if the application
 * has not detected the usb disconnect */
static int kr_close(struct inode *inode, struct file *file)
{
	struct kr_device *device = file->private_data;	
	
	if (device != NULL)
	{
		CloseDevice(device);
		
		// Finished with the device
		file->private_data = NULL;
		kref_put(&device->refCount, device_release);
	}
	
	return 0;
}

/* Get the next item out of the input buffer and remove it from the list */
static size_t kr_readNextItem(struct kr_device *device, char *buffer, size_t count, loff_t *ppos)
{
	int ret = 0;
	unsigned long spin_flags;
	struct kr_data_list_item *item = NULL;

	/* Grab the first item in the list */
	spin_lock_irqsave(&device->input_list_lock, spin_flags);
	if (atomic_read(&device->input_list_length) > 0)
	{
		item = list_first_entry(&device->input_list_head, struct kr_data_list_item, list_head);
		list_del(&item->list_head);
		atomic_dec(&device->input_list_length);
	}
	spin_unlock_irqrestore(&device->input_list_lock, spin_flags);

	if (!item)
	{
		return 0;
	}

	/* Copy the data from the list object and clean up */
	if (copy_to_user(buffer, (void*)item->data, INPUT_BUFFER_SIZE) != 0)
		ret = -EIO;
	else
		ret = INPUT_BUFFER_SIZE;

	kfree(item);
	return ret;
}

/* read requested on the file operations object. Supports both blocking and non blocking. */
static ssize_t kr_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	ssize_t retVal;
	struct kr_device *device = file->private_data;

	/* Increment the ref count in case of blocking */
	kref_get(&device->refCount);

	if (count < INPUT_BUFFER_SIZE)
	{
		retVal = -EIO;
	}
	else
	{
		if (file->f_flags & O_NONBLOCK)
		{
			retVal = kr_readNextItem(device, buffer, count, ppos);
		}
		else
		{
			/* If this is a blocking call make sure something is in the list before proceeding */	
			if (wait_event_interruptible(device->wait_queue, atomic_read(&device->input_list_length) != 0 || atomic_read(&device->continueRunning) == 0))
			{
				retVal = -ERESTARTSYS;
			}
			else if (atomic_read(&device->continueRunning) == 0)
			{
				retVal = 0;
			}
			else
			{
				retVal = kr_readNextItem(device, buffer, count, ppos);
			}
		}
	}
	
	/* Decrease the ref count that we added at the beginning of the function */
	kref_put(&device->refCount, device_release);
	return retVal;
	
}

/* Write request on the file operations object. Always blocking */
static ssize_t kr_write(struct file *file, const char *buffer, size_t write_count, loff_t *ppos)
{
	char *data = NULL;
	ssize_t ret = -1;
	struct kr_device *device = file->private_data;
	int report_id, i, valid;
	
	/* Ref count */
	kref_get(&device->refCount);
	
	/* Copy data out of user space */
	data = kmalloc(write_count, GFP_KERNEL);
	if (!data)
	{
		ret = 0;
		goto release_ref;	
	}

	if (copy_from_user(data, buffer, write_count))
	{
		ret = 0;
		goto free_data;
	}

	/* Validate that the report will be recognised by the device */
	report_id = data[0];
	i = 0;
	valid = 0;
	while (device->validReportIds[i] != 0)
	{
		if (report_id == device->validReportIds[i])
			valid = 1;
			
		++i;
	}
	
	/* If its a valid report, send the data */
	if (valid)
	{	 
		ret = usb_control_msg(device->usbdev, 
				  usb_sndctrlpipe(device->usbdev, 0),
				  HID_REQ_SET_REPORT,
				  USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				  ((HID_OUTPUT_REPORT + 1) << 8) | *data,
				  0,
				  data,
				  write_count,
				  WRITE_TIMEOUT_MS);	
	}
	
free_data:
	kfree(data);
	data = NULL;
	
release_ref:
	
	kref_put(&device->refCount, device_release);
	return ret;
}

static void assign_device_properties(struct kr_device *device)
{
	switch(device->usbdev->descriptor.idProduct)
	{
		case PRODUCT_K102:
			device->validReportIds = K102_report_ids;
			break;
		case PRODUCT_GR1:
			device->validReportIds = GR1_report_ids;
			break;
		case PRODUCT_GR1A:
			device->validReportIds = GR1A_report_ids;
			break;
		case PRODUCT_RADANGEL:
			device->validReportIds = RadAngel_report_ids;
			break;
		case PRODUCT_SIGMA25:
			device->validReportIds = SIGMA25_report_ids;
			break;
		case PRODUCT_SIGMA50:
			device->validReportIds = SIGMA50_report_ids;
			break;
		case PRODUCT_TN15:
			device->validReportIds = TN15_report_ids;
			break;
		default:
			printk("Unknown report ID list. Defaulting to GR1 reports list");
			device->validReportIds = GR1_report_ids;
			break;
	}
}

/* poll request on File operations object. Return whether data is ready to read */
static unsigned int kr_poll (struct file *file, struct poll_table_struct *table)
{
	unsigned int ret = 0;
	struct kr_device *device = file->private_data;
	
	if (atomic_read(&device->continueRunning) == 0)
	{
		return POLL_HUP;
	}

	/* poll_wait will block so make sure we keep a reference to device */
	kref_get(&device->refCount);
	
	poll_wait(file, &device->wait_queue, table);

	if (atomic_read(&device->input_list_length) > 0)
	{
		ret |= POLLIN | POLLRDNORM;
	}
	
	kref_put(&device->refCount, device_release);
	return ret;
}

static struct file_operations kr_fops =
{
	.owner = THIS_MODULE,
	.read = kr_read,
	.write = kr_write,
	.open = kr_open,
	.release = kr_close,
	.poll = kr_poll,
};

static struct usb_class_driver kr_class = 
{
	.name = "kromek%d",
	.fops = &kr_fops,
	.minor_base = KR_MINOR_BASE,
};

/* Device has been detected, determine if it has the appropriate endpoint and initialise */
static int kr_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct kr_device *device;
	int err = 0;
	int i;

	/* Create a device specific structure */
	device = kzalloc(sizeof(struct kr_device), GFP_KERNEL);
	kref_init(&device->refCount);
	
	device->usbdev = usb_get_dev(interface_to_usbdev(interface));
	device->usb_interface = interface;

	spin_lock_init(&device->input_list_lock);
	INIT_LIST_HEAD(&device->input_list_head);

	init_waitqueue_head(&device->wait_queue);
	atomic_set(&device->input_list_length, 0);
	atomic_set(&device->continueRunning, 0);

	/* Search the endpoints of this device to find the input interrupt (will normally be endpoint 1) */
	for (i = 0; i < interface->cur_altsetting->desc.bNumEndpoints; ++i)
	{
		struct usb_endpoint_descriptor *endpoint = &interface->cur_altsetting->endpoint[i].desc;
		
		if (usb_endpoint_is_int_in(endpoint))
		{
			device->input_buffer_size = le16_to_cpu(endpoint->wMaxPacketSize);
			device->interrupt_in_addr = endpoint->bEndpointAddress;
			device->input_buffer = kmalloc(INPUT_BUFFER_SIZE, GFP_KERNEL);
			device->input_bufferB = kmalloc(INPUT_BUFFER_SIZE, GFP_KERNEL);
			device->input_interval = endpoint->bInterval;
			break; /* We are only looking for this 1 interface */
		}
	}

	if (!device->interrupt_in_addr)
	{
		printk("kromek: interface does not have required endpoints\n");
		err = -ENOMEM;
		goto err_free_device;
	}

	/* Attach device struct to interface for reference later */
	usb_set_intfdata(interface, device);

	/* Register the usb device */
	err = usb_register_dev(interface, &kr_class);
	if (err)
	{
		printk("kromek: failed to register usb device\n");
		goto err_free_interface;
	}

	// Lookup details of this device
	assign_device_properties(device);

	printk("kromek: Interface initialised.");

	return 0;

err_free_interface:
	usb_set_intfdata(interface, NULL);
	kfree(device->input_buffer);

err_free_device:
	kref_put(&device->refCount, device_release);
	return err;
	
}

/* Device disconnected. Clean up */
static void kr_disconnect(struct usb_interface *interface)
{
	struct kr_device *device = usb_get_intfdata(interface);
	CloseDevice(device);
	usb_deregister_dev(interface, &kr_class);
		
	/* Free assoc memory */
	kfree(device->input_buffer);
	kfree(device->input_bufferB);
	
	usb_set_intfdata(interface, NULL);
	kref_put(&device->refCount, device_release);
}

/* Driver module init */
static int __init kr_init(void)
{
	int ret = 0;

	usb_register(&kr_driver);
	return ret;
}

/* Driver module exit */
static void __exit kr_exit(void)
{
	usb_deregister(&kr_driver);
}

module_init(kr_init);
module_exit(kr_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kromek");
MODULE_DESCRIPTION("Kromek Device Driver");
MODULE_VERSION("2");
