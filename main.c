/**
 * main
 *
 * This is the source for lg, a command for controlling the brightness
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
const int MODELS[] = {
  0x9a63, // LG24MD4KL
  0x9a70, // LG27MD5KL
  0x9a40, // LG27MD5KA
};

const int LG_IFACE = 1;

// The maximum (absolute) brightness value we'll set.
static const uint16_t MAX_BRIGHTNESS = 0xd2f0;
// We won't allow the brightness to go below this.
static const uint16_t MIN_PERCENT = 1;
// The amount (in %) we dim / brighten per adjustment
static const uint8_t BRIGHTNESS_STEP = 2;

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
  return (uint8_t)(((float)(getBrightness(lg_handle)) / MAX_BRIGHTNESS) * 100.0);
}

// Increment / decrement brightness by the specified percent.
int adjustBrightnessPercent(libusb_device_handle *lg_handle, int percent) {
  int currentPercent = getBrightnessPercent(lg_handle);
  int newPercent = currentPercent + percent;
  int rem = newPercent % BRIGHTNESS_STEP;

  // If the newPercent is not an even multiple of
  // BRIGHTNESS_STEP, we'll adjust it to be one...
  if (rem) {
    newPercent += percent > 0 ? rem : -rem;
  }

  if (newPercent > 100) {
    newPercent = 100;
  } else if (newPercent < MIN_PERCENT) {
    newPercent = MIN_PERCENT;
  }

  uint16_t brightness = newPercent * MAX_BRIGHTNESS / 100;
  set_brightness(lg_handle, brightness);
  return newPercent;
}

// Clean up ncurses and libusb handles.
void cleanup(int succeeded, libusb_device_handle *lg_handle) {
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
int handleChar(libusb_device_handle *lg_handle, char ch) {
  switch (ch) {
  case '+':
  case '=':
    // Brighten
    return adjustBrightnessPercent(lg_handle, BRIGHTNESS_STEP);
  case '-':
  case '_':
    // Dim
    return adjustBrightnessPercent(lg_handle, -BRIGHTNESS_STEP);
  }
  return -1;
}

// Ajust the brightness as based on ch.
void adjustOnce(libusb_device **lgdevs, char ch) {
  libusb_device_handle *lg_handle = init(lgdevs);
  if (!lg_handle) {
    return;
  }

  int brightnessPercent = handleChar(lg_handle, ch);

  cleanup(true, lg_handle);

  printf("%d%\n", brightnessPercent);
}

// Adjust the brightness interactively
void adjust(libusb_device **lgdevs) {
  initscr();
  printw("Press + / - to adjust brightness.\n");

  libusb_device_handle *lg_handle = init(lgdevs);
  if (!lg_handle) {
    return;
  }

  int brightnessPercent = getBrightnessPercent(lg_handle);
  printw("Brightness volume is %d", brightnessPercent);

  char ch;
  while (1) {
    ch = getch();
    if (ch == 'q') {
      cleanup(true, lg_handle);
      endwin();
      return;
    }
    brightnessPercent = handleChar(lg_handle, ch);
    clrtoeol();
    printw("%d%\r", brightnessPercent);
  }
}

// Determine whether or not the specified USB device is an LG monitor.
bool isSupportedDevice(struct libusb_device_descriptor *desc) {
  if (desc->idVendor != LG_VENDOR_ID) {
    return false;
  }
  for (int i = 0; i < sizeof(MODELS)/sizeof(MODELS[0]); ++i) {
    if (desc->idProduct == MODELS[i]) {
      return true;
    }
  }
  return false;
}

// Get a list of LG ultrafine usb devices. Return the count.
int getLGUltrafineUsbDevices(libusb_device **devs, int usb_cnt, libusb_device ***lg_devs) {
  int lg_cnt = 0;
  struct libusb_device_descriptor desc;

  for (int i = 0; i < usb_cnt; i++) {
    libusb_get_device_descriptor(devs[i], &desc);
    if (isSupportedDevice(&desc)) {
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
    if (isSupportedDevice(&desc)) {
      (*lg_devs)[k++] = devs[i];
    }
  }

  return lg_cnt;
}

void printHelp() {
  printf("\n");
  printf("  lg +     brighten the screen\n");
  printf("  lg -     dim the screen\n");
  printf("  lg       interactive mode for adjusting brightness\n");
  printf("\n");
}

int main(int argc, char *argv[]) {
  if (argc == 2 && *argv[1] != '+' && *argv[1] != '-') {
    printHelp();
    return 1;
  }

  if (argc > 2) {
    printf("lg cannot be called with more than one argument.\n");
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
    adjustOnce(lgdevs, *argv[1]);
  }

  return 0;
}
