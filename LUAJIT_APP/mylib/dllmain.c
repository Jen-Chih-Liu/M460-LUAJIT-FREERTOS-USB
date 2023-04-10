#define _CRT_SECURE_NO_WARNINGS
#include "stdio.h"
#include "stdlib.h"
#include "string.h""
#include ".\libusb.h"
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

static char* uuid_to_string(const uint8_t * uuid)
{
	static char uuid_string[40];
	if (uuid == NULL) return NULL;
	snprintf(uuid_string, sizeof(uuid_string),
		"{%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
		uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
		uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
	return uuid_string;
}
static void perr(char const* format, ...)
{
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

libusb_device_handle* handle;

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
_declspec(dllexport)  void open_usb(void)
{
	const struct libusb_version* version;
	version = libusb_get_version();
	printf("Using libusb v%d.%d.%d.%d\n\n", version->major, version->minor, version->micro, version->nano);

	//libusb initial
	libusb_init(NULL);

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
}
//close usb
_declspec(dllexport)  void close_usb(void)
{
	libusb_exit(NULL);
}
//write usb data to device
_declspec(dllexport)  void write_usb(unsigned char* arr)
{
	int size;
	//for (int i = 0; i < 64; i++)
	//{
		//printf("0x%x\n\r",arr[i]);
	//}
	libusb_interrupt_transfer(handle, 0x05, arr, 64, &size, 5000);
}
//read usb data from device
_declspec(dllexport) int* read_usb() {
	int size;
	unsigned char* arr = malloc(64 * sizeof(unsigned char));
	libusb_interrupt_transfer(handle, 0x84, arr, 64, &size, 5000);
    return arr;
}
//free array
_declspec(dllexport) void free_array(unsigned char* arr) {
	free(arr);
}