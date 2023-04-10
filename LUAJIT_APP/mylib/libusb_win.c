/*
* Test suite program based of libusb-0.1-compat testlibusb
* Copyright (c) 2013 Nathan Hjelm <hjelmn@mac.ccom>
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <stdio.h>
#include <string.h>
#include ".\libusb.h"
#define usb_interface interface
BOOL extra_info = TRUE;

uint16_t vid = 0x0416;
uint16_t pid = 0xDC00;
BOOL force_device_request = FALSE;		// For WCID descriptor queries
#define MS_OS_DESC_STRING_INDEX		0xEE
#define MS_OS_DESC_STRING_LENGTH	0x12
#define CALL_CHECK_CLOSE(fcall, hdl) do { int _r=fcall; if (_r < 0) { libusb_close(hdl); ERR_EXIT(_r); } } while (0)
#define ERR_EXIT(errcode) do { perr("   %s\n", libusb_strerror((enum libusb_error)errcode)); return -1; } while (0)
#define snprintf _snprintf

static void display_buffer_hex(unsigned char* buffer, unsigned size)
{
	unsigned i, j, k;

	for (i = 0; i < size; i += 16) {
		printf("\n  %08x  ", i);
		for (j = 0, k = 0; k < 16; j++, k++) {
			if (i + j < size) {
				printf("%02x", buffer[i + j]);
			}
			else {
				printf("  ");
			}
			printf(" ");
		}
		printf(" ");
		for (j = 0, k = 0; k < 16; j++, k++) {
			if (i + j < size) {
				if ((buffer[i + j] < 32) || (buffer[i + j] > 126)) {
					printf(".");
				}
				else {
					printf("%c", buffer[i + j]);
				}
			}
		}
	}
	printf("\n");
}

static char* uuid_to_string(const uint8_t* uuid)
{
	static char uuid_string[40];
	if (uuid == NULL) return NULL;
	snprintf(uuid_string, sizeof(uuid_string),
		"{%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
		uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
		uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
	return uuid_string;
}
static void print_device_cap(struct libusb_bos_dev_capability_descriptor* dev_cap)
{
	switch (dev_cap->bDevCapabilityType) {
	case LIBUSB_BT_USB_2_0_EXTENSION: {
		struct libusb_usb_2_0_extension_descriptor* usb_2_0_ext = NULL;
		libusb_get_usb_2_0_extension_descriptor(NULL, dev_cap, &usb_2_0_ext);
		if (usb_2_0_ext) {
			printf("    USB 2.0 extension:\n");
			printf("      attributes             : %02X\n", usb_2_0_ext->bmAttributes);
			libusb_free_usb_2_0_extension_descriptor(usb_2_0_ext);
		}
		break;
	}
	case LIBUSB_BT_SS_USB_DEVICE_CAPABILITY: {
		struct libusb_ss_usb_device_capability_descriptor* ss_usb_device_cap = NULL;
		libusb_get_ss_usb_device_capability_descriptor(NULL, dev_cap, &ss_usb_device_cap);
		if (ss_usb_device_cap) {
			printf("    USB 3.0 capabilities:\n");
			printf("      attributes             : %02X\n", ss_usb_device_cap->bmAttributes);
			printf("      supported speeds       : %04X\n", ss_usb_device_cap->wSpeedSupported);
			printf("      supported functionality: %02X\n", ss_usb_device_cap->bFunctionalitySupport);
			libusb_free_ss_usb_device_capability_descriptor(ss_usb_device_cap);
		}
		break;
	}
	case LIBUSB_BT_CONTAINER_ID: {
		struct libusb_container_id_descriptor* container_id = NULL;
		libusb_get_container_id_descriptor(NULL, dev_cap, &container_id);
		if (container_id) {
			printf("    Container ID:\n      %s\n", uuid_to_string(container_id->ContainerID));
			libusb_free_container_id_descriptor(container_id);
		}
		break;
	}
	default:
		printf("    Unknown BOS device capability %02x:\n", dev_cap->bDevCapabilityType);
	}
}
static void perr(char const* format, ...)
{
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}
static void read_ms_winsub_feature_descriptors(libusb_device_handle* handle, uint8_t bRequest, int iface_number)
{
#define MAX_OS_FD_LENGTH 256
	int i, r;
	uint8_t os_desc[MAX_OS_FD_LENGTH];
	uint32_t length;
	void* le_type_punning_IS_fine;
	struct {
		const char* desc;
		uint8_t recipient;
		uint16_t index;
		uint16_t header_size;
	} os_fd[2] = {
		{ "Extended Compat ID", LIBUSB_RECIPIENT_DEVICE, 0x0004, 0x10 },
		{ "Extended Properties", LIBUSB_RECIPIENT_INTERFACE, 0x0005, 0x0A }
	};

	if (iface_number < 0) return;
	// WinUSB has a limitation that forces wIndex to the interface number when issuing
	// an Interface Request. To work around that, we can force a Device Request for
	// the Extended Properties, assuming the device answers both equally.
	if (force_device_request)
		os_fd[1].recipient = LIBUSB_RECIPIENT_DEVICE;

	for (i = 0; i < 2; i++) {
		printf("\nReading %s OS Feature Descriptor (wIndex = 0x%04d):\n", os_fd[i].desc, os_fd[i].index);

		// Read the header part
		r = libusb_control_transfer(handle, (uint8_t)(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | os_fd[i].recipient),
			bRequest, (uint16_t)(((iface_number) << 8) | 0x00), os_fd[i].index, os_desc, os_fd[i].header_size, 1000);
		if (r < os_fd[i].header_size) {
			perr("   Failed: %s", (r < 0) ? libusb_strerror((enum libusb_error)r) : "header size is too small");
			return;
		}
		le_type_punning_IS_fine = (void*)os_desc;
		length = *((uint32_t*)le_type_punning_IS_fine);
		if (length > MAX_OS_FD_LENGTH) {
			length = MAX_OS_FD_LENGTH;
		}

		// Read the full feature descriptor
		r = libusb_control_transfer(handle, (uint8_t)(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | os_fd[i].recipient),
			bRequest, (uint16_t)(((iface_number) << 8) | 0x00), os_fd[i].index, os_desc, (uint16_t)length, 1000);
		if (r < 0) {
			perr("   Failed: %s", libusb_strerror((enum libusb_error)r));
			return;
		}
		else {
			display_buffer_hex(os_desc, r);
		}
	}
}
static const uint8_t ms_os_desc_string[] = {
	MS_OS_DESC_STRING_LENGTH,
	LIBUSB_DT_STRING,
	'M', 0, 'S', 0, 'F', 0, 'T', 0, '1', 0, '0', 0, '0', 0,
};
#define MS_OS_DESC_VENDOR_CODE_OFFSET	0x10
unsigned int package_size;
int size;
unsigned char report_buffer[1024];
#define HID_GET_REPORT                0x01
#define HID_REPORT_TYPE_FEATURE       0x03

int main(void)
{
	const struct libusb_version* version;
	version = libusb_get_version();
	printf("Using libusb v%d.%d.%d.%d\n\n", version->major, version->minor, version->micro, version->nano);


	//libusb initial
	libusb_init(NULL);
	libusb_device_handle* handle;
	libusb_device* dev;
	uint8_t bus, port_path[8];
	struct libusb_bos_descriptor* bos_desc;
	struct libusb_config_descriptor* conf_desc;
	const struct libusb_endpoint_descriptor* endpoint;
	int i, j, k, r;
	int iface, nb_ifaces, first_iface = -1;
	struct libusb_device_descriptor dev_desc;
	const char* const speed_name[5] = { "Unknown", "1.5 Mbit/s (USB LowSpeed)", "12 Mbit/s (USB FullSpeed)",
		"480 Mbit/s (USB HighSpeed)", "5000 Mbit/s (USB SuperSpeed)" };
	char string[128];
	uint8_t string_index[3];	// indexes of the string descriptors
	uint8_t endpoint_in = 0, endpoint_out = 0;	// default IN and OUT endpoints

	printf("Opening device %04X:%04X...\n", vid, pid);
	handle = libusb_open_device_with_vid_pid(NULL, vid, pid);

	if (handle == NULL) {
		printf("  Failed.\n");
		return -1;
	}

	dev = libusb_get_device(handle);
	bus = libusb_get_bus_number(dev);

	if (extra_info) {
		r = libusb_get_port_numbers(dev, port_path, sizeof(port_path));
		if (r > 0) {
			printf("\nDevice properties:\n");
			printf("        bus number: %d\n", bus);
			printf("         port path: %d", port_path[0]);
			for (i = 1; i < r; i++) {
				printf("->%d", port_path[i]);
			}
			printf(" (from root hub)\n");
		}
		r = libusb_get_device_speed(dev);
		if ((r < 0) || (r > 4)) r = 0;
		printf("             speed: %s\n", speed_name[r]);
	}
	printf("\nReading device descriptor:\n");
	CALL_CHECK_CLOSE(libusb_get_device_descriptor(dev, &dev_desc), handle);
	printf("            length: %d\n", dev_desc.bLength);
	printf("      device class: %d\n", dev_desc.bDeviceClass);
	printf("               S/N: %d\n", dev_desc.iSerialNumber);
	printf("           VID:PID: %04X:%04X\n", dev_desc.idVendor, dev_desc.idProduct);
	printf("         bcdDevice: %04X\n", dev_desc.bcdDevice);
	printf("   iMan:iProd:iSer: %d:%d:%d\n", dev_desc.iManufacturer, dev_desc.iProduct, dev_desc.iSerialNumber);
	printf("          nb confs: %d\n", dev_desc.bNumConfigurations);
	// Copy the string descriptors for easier parsing
	string_index[0] = dev_desc.iManufacturer;
	string_index[1] = dev_desc.iProduct;
	string_index[2] = dev_desc.iSerialNumber;

	printf("\nReading BOS descriptor: ");
	if (libusb_get_bos_descriptor(handle, &bos_desc) == LIBUSB_SUCCESS) {
		printf("%d caps\n", bos_desc->bNumDeviceCaps);
		for (i = 0; i < bos_desc->bNumDeviceCaps; i++)
			print_device_cap(bos_desc->dev_capability[i]);
		libusb_free_bos_descriptor(bos_desc);
	}
	else {
		printf("no descriptor\n");
	}

	printf("\nReading first configuration descriptor:\n");
	CALL_CHECK_CLOSE(libusb_get_config_descriptor(dev, 0, &conf_desc), handle);
	nb_ifaces = conf_desc->bNumInterfaces;
#if 0
	printf("             nb interfaces: %d\n", nb_ifaces);
	if (nb_ifaces > 0)
		first_iface = conf_desc->usb_interface[0].altsetting[0].bInterfaceNumber;
	for (i = 0; i < nb_ifaces; i++) {
		printf("              interface[%d]: id = %d\n", i,
			conf_desc->usb_interface[i].altsetting[0].bInterfaceNumber);
		for (j = 0; j < conf_desc->usb_interface[i].num_altsetting; j++) {
			printf("interface[%d].altsetting[%d]: num endpoints = %d\n",
				i, j, conf_desc->usb_interface[i].altsetting[j].bNumEndpoints);
			printf("   Class.SubClass.Protocol: %02X.%02X.%02X\n",
				conf_desc->usb_interface[i].altsetting[j].bInterfaceClass,
				conf_desc->usb_interface[i].altsetting[j].bInterfaceSubClass,
				conf_desc->usb_interface[i].altsetting[j].bInterfaceProtocol);
			if ((conf_desc->usb_interface[i].altsetting[j].bInterfaceClass == LIBUSB_CLASS_MASS_STORAGE)
				&& ((conf_desc->usb_interface[i].altsetting[j].bInterfaceSubClass == 0x01)
					|| (conf_desc->usb_interface[i].altsetting[j].bInterfaceSubClass == 0x06))
				&& (conf_desc->usb_interface[i].altsetting[j].bInterfaceProtocol == 0x50)) {
				// Mass storage devices that can use basic SCSI commands
				//test_mode = USE_SCSI;
			}
			for (k = 0; k < conf_desc->usb_interface[i].altsetting[j].bNumEndpoints; k++) {
				struct libusb_ss_endpoint_companion_descriptor* ep_comp = NULL;
				endpoint = &conf_desc->usb_interface[i].altsetting[j].endpoint[k];
				printf("       endpoint[%d].address: %02X\n", k, endpoint->bEndpointAddress);
				// Use the first interrupt or bulk IN/OUT endpoints as default for testing
				if ((endpoint->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) & (LIBUSB_TRANSFER_TYPE_BULK | LIBUSB_TRANSFER_TYPE_INTERRUPT)) {
					if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN) {
						if (!endpoint_in)
							endpoint_in = endpoint->bEndpointAddress;
					}
					else {
						if (!endpoint_out)
							endpoint_out = endpoint->bEndpointAddress;
					}
				}
				package_size = endpoint->wMaxPacketSize;
				printf("           max packet size: %04X\n", endpoint->wMaxPacketSize);
				printf("          polling interval: %02X\n", endpoint->bInterval);
				libusb_get_ss_endpoint_companion_descriptor(NULL, endpoint, &ep_comp);
				if (ep_comp) {
					printf("                 max burst: %02X   (USB 3.0)\n", ep_comp->bMaxBurst);
					printf("        bytes per interval: %04X (USB 3.0)\n", ep_comp->wBytesPerInterval);
					libusb_free_ss_endpoint_companion_descriptor(ep_comp);
				}
			}
		}
	}
	libusb_free_config_descriptor(conf_desc);
#endif
	libusb_set_auto_detach_kernel_driver(handle, 1);
	for (iface = 0; iface < nb_ifaces; iface++)
	{
		printf("\nClaiming interface %d...\n", iface);
		r = libusb_claim_interface(handle, iface);
		if (r != LIBUSB_SUCCESS) {
			printf("   Failed.\n");
		}
		else {
			printf("   pass.\n");
		}

	}
#if 0
	printf("\nReading string descriptors:\n");
	for (i = 0; i < 3; i++) {
		if (string_index[i] == 0) {
			continue;
		}
		if (libusb_get_string_descriptor_ascii(handle, string_index[i], (unsigned char*)string, sizeof(string)) > 0) {
			printf("   String (0x%02X): \"%s\"\n", string_index[i], string);
		}
	}
	// Read the OS String Descriptor
	r = libusb_get_string_descriptor(handle, MS_OS_DESC_STRING_INDEX, 0, (unsigned char*)string, MS_OS_DESC_STRING_LENGTH);
	if (r == MS_OS_DESC_STRING_LENGTH && memcmp(ms_os_desc_string, string, sizeof(ms_os_desc_string)) == 0) {
		// If this is a Microsoft OS String Descriptor,
		// attempt to read the WinUSB extended Feature Descriptors
		read_ms_winsub_feature_descriptors(handle, string[MS_OS_DESC_VENDOR_CODE_OFFSET], first_iface);
	}
#endif
	size = 512;
	for (int i = 0; i < size; i++)
		report_buffer[i] = i & 0xff;
	int out_size = 512;
	int in_size = 512;
	libusb_interrupt_transfer(handle, 0x05, report_buffer, 64, &size, 5000);
	//libusb_interrupt_transfer(handle, 0x84, report_buffer, in_size, &size, 5000);
	//libusb_control_transfer(handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
	//	HID_GET_REPORT, (HID_REPORT_TYPE_FEATURE << 8) | 0, 0, report_buffer, (uint16_t)size, 5000);
	libusb_exit(NULL);
	return 0;
}