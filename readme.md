# lg

Adjust the brightness of an lg ultrafine display.

Based entirely off of [ycsos](https://github.com/ycsos/LG-ultrafine-brightness) source.

## Build

Dependencies

- gcc
- ncurses
- libusb

On Fedora, these can be installed via dnf:

```
sudo dnf install ncurses-devel libusbx-devel gcc 
```

To build, run the following (replacing the C_INCLUDE_PATH with your system's path, if not using Fedora).

```
C_INCLUDE_PATH=/usr/include/libusb-1.0/ gcc main.c -lncurses -lusb-1.0 -o lg
```

## Run

Sudo is required, or libusb segfaults.

```
sudo lg
```

Running with no arguments will ask you to press + / - to adjust the brightness.

It can also be called with an argument: `+` or `-` to increase or decrease the brightness and exit. This is handy for mapping keyboard shortcuts to brighten / dim your screen.


To avoid having to enter sudo all the time, add an exception to `visudo`:

```
sudo visudo
```

Add the following line (replace `REPLACEME` with your username):

```
REPLACEME ALL=NOPASSWD:/usr/local/bin/lg
```

This, assumes you've copied `lg` to `/usr/local/bin`.
