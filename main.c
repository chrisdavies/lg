/**
 * main
 *
 * This is the source for lgbrite, a command for controlling the brightness
 * on USB-C LG monitors.
 *
 * It is based heavily on https://github.com/csujedihy/LG-Ultrafine-Brightness
 */
#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <libusb.h>

// The LG vendor id is used to distinguish LG devices from
// those of other vendors.
const uint16_t LG_VENDOR_ID = 0x43e;

// These are the model ids that we recognize. If your monitor
// is not working properly, you may need to add its id here.
const int LG24MD4KL = 0x9a63;
const int LG27MD5KL = 0x9a70;
const int LG27MD5KA = 0x9a40;

const int LG_IFACE = 1;

// These values control the adjustment behavior.
static const uint16_t MAX_BRIGHTNESS = 0xd2f0;
static const uint16_t MIN_BRIGHTNESS = 0x0000;
static const uint8_t BRIGHTNESS_STEP = 1;

static const int HID_GET_REPORT = 0x01;
static const int HID_SET_REPORT = 0x09;
static const int HID_REPORT_TYPE_INPUT = 0x01;
static const int HID_REPORT_TYPE_OUTPUT = 0x02;
static const int HID_REPORT_TYPE_FEATURE = 0x03;

// Get the display brightness in absolute terms.
uint16_t getBrightness(libusb_device_handle *lg_handle) {
  u_char data[8] = {0x00};
  int res = libusb_control_transfer(
  	lg_handle,
   	LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
    HID_GET_REPORT,
    (HID_REPORT_TYPE_FEATURE << 8) | 0,
    1,
    data,
    sizeof(data),
    0
  );
  return data[0] + (data[1] << 8);
}

// Set the display brightness in absolute terms.
void set_brightness(libusb_device_handle *lg_handle, uint16_t val) {
  uint8_t data[6] = {
    (uint8_t)(val & 0x00ff),
    (uint8_t)((val >> 8) & 0x00ff),
    0x00,
    0x00,
    0x00,
    0x00
  };
  libusb_control_transfer(
    lg_handle,
    LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
    HID_SET_REPORT,
    (HID_REPORT_TYPE_FEATURE << 8) | 0,
    1,
    data,
    sizeof(data),
    0
  );
}

// Get the display brightness in percent.
uint8_t getBrightnessPercent(libusb_device_handle *lg_handle) {
  return (uint8_t)(((float)(getBrightness(lg_handle)) / 54000) * 100.0);
}

// Set the display brightness in percent.
int setBrightnessPercent(libusb_device_handle *lg_handle, int val) {
  if (val < 0) {
    val = 0;
  } else if (val > 100) {
    val = 100;
  } else {
    val = val - (val % 5);
  }
  uint16_t brightness = val * 54000 / 100;
  set_brightness(lg_handle, brightness);
  return val;
}

// Clean up ncurses and libusb handles.
void cleanup(int succeeded, libusb_device_handle *lg_handle) {
  endwin();
  if (succeeded) {
    libusb_release_interface(lg_handle, LG_IFACE);
    libusb_attach_kernel_driver(lg_handle, LG_IFACE);
  }
  libusb_close(lg_handle);
}

// Attempt to open the USB connection to the monitor.
libusb_device_handle *init(libusb_device **lgdevs) {
  libusb_device_handle *lg_handle;
  int libusb_success;

  initscr();
  timeout(-1);

  libusb_open(lgdevs[0], &lg_handle);
  libusb_set_auto_detach_kernel_driver(lg_handle, LG_IFACE);

  libusb_success = libusb_claim_interface(lg_handle, LG_IFACE);

  if (libusb_success != LIBUSB_SUCCESS) {
    printw("Failed to claim interface %d. Error: %d\n", LG_IFACE, libusb_success);
    printw("Error: %s\n", libusb_error_name(libusb_success));
    cleanup(false, lg_handle);
    return 0;
  }

  return lg_handle;
}

// Ajust the brightness as based on ch.
bool handleChar(libusb_device **lgdevs, char ch) {
  libusb_device_handle *lg_handle = init(lgdevs);
  if (!lg_handle) {
    return false;
  }

  int brightnessVolume = getBrightnessPercent(lg_handle);

  switch (ch) {
  case '+':
  case '=':
    // Brighten
    brightnessVolume = setBrightnessPercent(lg_handle, brightnessVolume += BRIGHTNESS_STEP);
    break;
  case '-':
  case '_':
    // Dim
    brightnessVolume = setBrightnessPercent(lg_handle, brightnessVolume -= BRIGHTNESS_STEP);
    break;
  }

  cleanup(true, lg_handle);

  printw("%d%\r", brightnessVolume);

  return true;
}

// Adjust the brightness interactively
void adjust(libusb_device **lgdevs) {
  printw("Press + / - to adjust brightness.\n");

  char ch;
  while (1) {
    ch = getch();
    clrtoeol();
    if (ch == 'q' || !handleChar(lgdevs, ch)) {
      return;
    }
  }
}

// Get a list of LG ultrafine usb devices. Return the count.
int getLGUltrafineUsbDevices(libusb_device **devs, int usb_cnt, libusb_device ***lg_devs) {
  int lg_cnt = 0;
  struct libusb_device_descriptor desc;

  for (int i = 0; i < usb_cnt; i++) {
    libusb_get_device_descriptor(devs[i], &desc);
    if (desc.idVendor == LG_VENDOR_ID
        && (desc.idProduct == LG24MD4KL || desc.idProduct == LG27MD5KA || desc.idProduct == LG27MD5KL)) {
      lg_cnt++;
    }
  }

  if (lg_cnt == 0) {
    return 0;
  }

  *lg_devs = (libusb_device **)malloc(sizeof(libusb_device *) * lg_cnt);

  int k = 0;
  for (int i = 0; i < usb_cnt; i++) {
    int r = libusb_get_device_descriptor(devs[i], &desc);
    if (desc.idProduct == LG24MD4KL || desc.idProduct == LG27MD5KA || desc.idProduct == LG27MD5KL) {
      (*lg_devs)[k++] = devs[i];
    }
  }

  return lg_cnt;
}

void printHelp() {
  printf("\n");
  printf("  lgbrite +     brighten the screen\n");
  printf("  lgbrite -     dim the screen\n");
  printf("  lgbrite       interactive mode for adjusting brightness\n");
  printf("\n");
}

int main(int argc, char *argv[]) {
  if (argc == 2 && *argv[1] != '+' && *argv[1] != '-') {
    printHelp();
    return 1;
  }

  if (argc > 2) {
    printf("lgbrite cannot be called with more than one argument.\n");
    printHelp();
    return 1;
  }

  libusb_device **devs;
  libusb_context *ctx = NULL;

  int r = libusb_init(&ctx);
  if (r < 0) {
    printf("Unable to initialize libusb.Exitting\n");
    return r;
  }

  int usb_cnt = libusb_get_device_list(ctx, &devs);
  if (usb_cnt < 0) {
    printf("Unable to get USB device list.Exitting\n");
    return -1;
  }

  struct libusb_device_descriptor desc;

  libusb_device **lgdevs;
  int lg_cnt = getLGUltrafineUsbDevices(devs, usb_cnt, &lgdevs);

  if (lg_cnt == 0) {
    printf("Could not find any LG monitors.");
  } else if (argc != 2) {
    adjust(lgdevs);
  } else {
    handleChar(lgdevs, *argv[1]);
  }
  return 0;
}