/*
 * SEGA Dreamcast keyboard driver
 * Based on drivers/usb/usbkbd.c
 * Copyright YAEGASHI Takeshi, 2001
 * Porting to 2.6 Copyright Adrian McMenamin, 2007
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/maple.h>
#include <asm/mach/maple.h>

/* Very simple mutex to ensure proper cleanup */
static DEFINE_MUTEX(maple_keyb_mutex);

#define NR_SCANCODES 256

MODULE_AUTHOR("YAEGASHI Takeshi, Adrian McMenamin");
MODULE_DESCRIPTION("SEGA Dreamcast keyboard driver");
MODULE_LICENSE("GPL");

struct dc_kbd {
	struct input_dev *dev;
	unsigned short keycode[NR_SCANCODES];
	unsigned char new[8];
	unsigned char old[8];
};

static const unsigned short dc_kbd_keycode[NR_SCANCODES] = {
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_A, KEY_B, KEY_C, KEY_D,
	KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L,
	KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
	KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z, KEY_1, KEY_2,
	KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,
	KEY_ENTER, KEY_ESC, KEY_BACKSPACE, KEY_TAB, KEY_SPACE, KEY_MINUS, KEY_EQUAL, KEY_LEFTBRACE,
	KEY_RIGHTBRACE, KEY_BACKSLASH, KEY_BACKSLASH, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_GRAVE, KEY_COMMA,
	KEY_DOT, KEY_SLASH, KEY_CAPSLOCK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
	KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12, KEY_SYSRQ,
	KEY_SCROLLLOCK, KEY_PAUSE, KEY_INSERT, KEY_HOME, KEY_PAGEUP, KEY_DELETE,
	KEY_END, KEY_PAGEDOWN, KEY_RIGHT, KEY_LEFT, KEY_DOWN, KEY_UP,
	KEY_NUMLOCK, KEY_KPSLASH, KEY_KPASTERISK, KEY_KPMINUS, KEY_KPPLUS, KEY_KPENTER, KEY_KP1, KEY_KP2,
	KEY_KP3, KEY_KP4, KEY_KP5, KEY_KP6, KEY_KP7, KEY_KP8, KEY_KP9, KEY_KP0, KEY_KPDOT,
	KEY_102ND, KEY_COMPOSE, KEY_POWER, KEY_KPEQUAL, KEY_F13, KEY_F14, KEY_F15,
	KEY_F16, KEY_F17, KEY_F18, KEY_F19, KEY_F20,
	KEY_F21, KEY_F22, KEY_F23, KEY_F24, KEY_OPEN, KEY_HELP, KEY_PROPS, KEY_FRONT,
	KEY_STOP, KEY_AGAIN, KEY_UNDO, KEY_CUT, KEY_COPY, KEY_PASTE, KEY_FIND, KEY_MUTE,
	KEY_VOLUMEUP, KEY_VOLUMEDOWN, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_KPCOMMA, KEY_RESERVED, KEY_RO, KEY_KATAKANAHIRAGANA , KEY_YEN,
	KEY_HENKAN, KEY_MUHENKAN, KEY_KPJPCOMMA, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_HANGEUL, KEY_HANJA, KEY_KATAKANA, KEY_HIRAGANA, KEY_ZENKAKUHANKAKU, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_LEFTALT, KEY_LEFTMETA, KEY_RIGHTCTRL, KEY_RIGHTSHIFT, KEY_RIGHTALT, KEY_RIGHTMETA,
	KEY_PLAYPAUSE, KEY_STOPCD, KEY_PREVIOUSSONG, KEY_NEXTSONG, KEY_EJECTCD, KEY_VOLUMEUP, KEY_VOLUMEDOWN, KEY_MUTE,
	KEY_WWW, KEY_BACK, KEY_FORWARD, KEY_STOP, KEY_FIND, KEY_SCROLLUP, KEY_SCROLLDOWN, KEY_EDIT, KEY_SLEEP,
	KEY_SCREENLOCK, KEY_REFRESH, KEY_CALC, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED
};

static void dc_scan_kbd(struct dc_kbd *kbd)
{
	struct input_dev *dev = kbd->dev;
	void *ptr;
	int code, keycode;
	int i;

	for (i = 0; i < 8; i++) {
		code = i + 224;
		keycode = kbd->keycode[code];
		input_event(dev, EV_MSC, MSC_SCAN, code);
		input_report_key(dev, keycode, (kbd->new[0] >> i) & 1);
	}

	for (i = 2; i < 8; i++) {
		ptr = memchr(kbd->new + 2, kbd->old[i], 6);
		code = kbd->old[i];
		if (code > 3 && ptr == NULL) {
			keycode = kbd->keycode[code];
			if (keycode) {
				input_event(dev, EV_MSC, MSC_SCAN, code);
				input_report_key(dev, keycode, 0);
			} else
				printk(KERN_DEBUG "maple_keyb: "
					"Unknown key (scancode %#x) released.",
					code);
		}
		ptr = memchr(kbd->old + 2, kbd->new[i], 6);
		code = kbd->new[i];
		if (code > 3 && ptr) {
			keycode = kbd->keycode[code];
			if (keycode) {
				input_event(dev, EV_MSC, MSC_SCAN, code);
				input_report_key(dev, keycode, 1);
			} else
				printk(KERN_DEBUG "maple_keyb: "
					"Unknown key (scancode %#x) pressed.",
					code);
		}
	}
	input_sync(dev);
	memcpy(kbd->old, kbd->new, 8);
}

static void dc_kbd_callback(struct mapleq *mq)
{
	struct maple_device *mapledev = mq->dev;
	struct dc_kbd *kbd = mapledev->private_data;
	unsigned long *buf = mq->recvbuf;

	/*
	 * We should always be getting the lock because the only
	 * time it may be locked if driver is in cleanup phase.
	 */
	if (likely(mutex_trylock(&maple_keyb_mutex))) {

		if (buf[1] == mapledev->function) {
			memcpy(kbd->new, buf + 2, 8);
			dc_scan_kbd(kbd);
		}

		mutex_unlock(&maple_keyb_mutex);
	}
}

static int dc_kbd_connect(struct maple_device *mdev)
{
	int i, error;
	struct dc_kbd *kbd;
	struct input_dev *dev;

	if (!(mdev->function & MAPLE_FUNC_KEYBOARD))
		return -EINVAL;

	kbd = kzalloc(sizeof(struct dc_kbd), GFP_KERNEL);
	dev = input_allocate_device();
	if (!kbd || !dev) {
		error = -ENOMEM;
		goto fail;
	}

	mdev->private_data = kbd;

	kbd->dev = dev;
	memcpy(kbd->keycode, dc_kbd_keycode, sizeof(kbd->keycode));

	dev->name = mdev->product_name;
	dev->evbit[0] = BIT(EV_KEY) | BIT(EV_REP);
	dev->keycode = kbd->keycode;
	dev->keycodesize = sizeof (unsigned short);
	dev->keycodemax = ARRAY_SIZE(kbd->keycode);
	dev->id.bustype = BUS_HOST;
	dev->dev.parent = &mdev->dev;

	for (i = 0; i < NR_SCANCODES; i++)
		__set_bit(dc_kbd_keycode[i], dev->keybit);
	__clear_bit(KEY_RESERVED, dev->keybit);

	input_set_capability(dev, EV_MSC, MSC_SCAN);
	input_set_drvdata(dev, kbd);

	error = input_register_device(dev);
	if (error)
		goto fail;

	/* Maple polling is locked to VBLANK - which may be just 50/s */
	maple_getcond_callback(mdev, dc_kbd_callback, HZ/50, MAPLE_FUNC_KEYBOARD);
	return 0;

 fail:
	input_free_device(dev);
	kfree(kbd);
	mdev->private_data = NULL;
	return error;
}

static void dc_kbd_disconnect(struct maple_device *mdev)
{
	struct dc_kbd *kbd;

	mutex_lock(&maple_keyb_mutex);

	kbd = mdev->private_data;
	mdev->private_data = NULL;
	input_unregister_device(kbd->dev);
	kfree(kbd);

	mutex_unlock(&maple_keyb_mutex);
}

/* allow the keyboard to be used */
static int probe_maple_kbd(struct device *dev)
{
	struct maple_device *mdev = to_maple_dev(dev);
	struct maple_driver *mdrv = to_maple_driver(dev->driver);
	int error;

	error = dc_kbd_connect(mdev);
	if (error)
		return error;

	mdev->driver = mdrv;
	mdev->registered = 1;

	return 0;
}

static struct maple_driver dc_kbd_driver = {
	.function = MAPLE_FUNC_KEYBOARD,
	.connect = dc_kbd_connect,
	.disconnect = dc_kbd_disconnect,
	.drv = {
		.name = "Dreamcast_keyboard",
		.probe = probe_maple_kbd,
       },
};

static int __init dc_kbd_init(void)
{
	return maple_driver_register(&dc_kbd_driver.drv);
}

static void __exit dc_kbd_exit(void)
{
	driver_unregister(&dc_kbd_driver.drv);
}

module_init(dc_kbd_init);
module_exit(dc_kbd_exit);
