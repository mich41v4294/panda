/**
 * @file    panda.c
 * @author  Jeddy Diamond Exum
 * @date    16 June 2017
 * @version 0.1
 * @brief   Driver for the Comma.ai Panda CAN adapter to allow it to be controlled via
 * the Linux SocketCAN interface.
 * @see https://github.com/commaai/panda for the full project.
 */

#include <linux/can.h>
#include <linux/can/dev.h>
#include <linux/can/error.h>
#include <linux/can/led.h>
#include <linux/init.h>             // Macros used to mark up functions e.g., __init __exit
#include <linux/module.h>           // Core header for loading LKMs into the kernel
#include <linux/kernel.h>           // Contains types, macros, functions for the kernel
#include <linux/usb.h>

/* vendor and product id */
#define PANDA_MODULE_NAME "panda"
#define PANDA_VENDOR_ID 0XBBAA
#define PANDA_PRODUCT_ID 0XDDCC

static const struct usb_device_id panda_usb_table[] = {
  { USB_DEVICE(PANDA_VENDOR_ID, PANDA_PRODUCT_ID) },
  {} /* Terminating entry */
};

struct panda_priv {
  unsigned int id;
};

unsigned int curr_id = 0;

static int panda_usb_start(struct panda_priv *priv)
{
  return 0;
}

static int panda_usb_probe(struct usb_interface *intf,
			  const struct usb_device_id *id)
{
  int err = -ENOMEM;
  //struct usb_device *usbdev = interface_to_usbdev(intf);
  struct panda_priv *priv = NULL;

  /* Allocate space for device driver specific data */
  priv = kzalloc(sizeof(struct panda_priv), GFP_KERNEL);
  if (!priv)
    return -ENOMEM;

  priv->id = curr_id++;

  usb_set_intfdata(intf, priv);

  err = panda_usb_start(priv);
  if (err) {
    dev_info(&intf->dev, "Failed to initialize Comma.ai Panda CAN controller\n");
    goto cleanup;
  }

  dev_info(&intf->dev, "Comma.ai Panda CAN controller connected (ID: %u)\n", priv->id);

  return 0;

 cleanup:
  kfree(priv);
  return err;
}

/* Called by the usb core when driver is unloaded or device is removed */
static void panda_usb_disconnect(struct usb_interface *intf)
{
  struct panda_priv *priv = usb_get_intfdata(intf);

  usb_set_intfdata(intf, NULL);

  dev_info(&intf->dev, "Removed Comma.ai Panda CAN controller (ID: %u)\n", priv->id);

  //netdev_info(priv->netdev, "device disconnected\n");
  //
  //unregister_candev(priv->netdev);
  //free_candev(priv->netdev);
  //
  //mcba_urb_unlink(priv);
  kfree(priv);
}

static struct usb_driver panda_usb_driver = {
  .name = PANDA_MODULE_NAME,
  .probe = panda_usb_probe,
  .disconnect = panda_usb_disconnect,
  .id_table = panda_usb_table,
};

module_usb_driver(panda_usb_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jessy Diamond Exum <jessy.diamondman@gmail.com>");
MODULE_DESCRIPTION("SocketCAN driver for Comma.ai's Panda Adapter.");
MODULE_VERSION("0.1");
MODULE_DEVICE_TABLE(usb, panda_usb_table);
