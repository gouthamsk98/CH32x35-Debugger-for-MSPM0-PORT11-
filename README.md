# CH32x35 Debugger - Debugger Tool for Port11 MSPM0 dev board

This tool is a work in progress.

## Prerequisite For Linux User

**Set udev Rules**

- Create a new udev rules file:
  ```sh
  sudo nano /etc/udev/rules.d/30-port11.rules
  ```
- Add the following line to the file:
  ```sh
  SUBSYSTEM=="usb", ATTRS{idVendor}=="6249", ATTRS{idProduct}=="7031", MODE="0666"
  ```
- Save and exit the file.

## Flashing via python

```
sudo apt-get install libusb-1.0-0-dev
pip3 install pyusb
python3 test_via_python/bsl_ch32x035.py -p /dev/tty.usbmodem2201 -b 115200 -W test_via_python/blink.hex
```

## Tested On

This tool should work on MSPM0G3507 chips. But I haven't tested it on any other chips.

- [x] MSPM0G3507

## How to Generate Hex image

**Pre Requisites**

- ARM-CGT_CLANG Compiler

[Download ARM-CGT-CLANG Compiler](https://www.ti.com/tool/download/ARM-CGT-CLANG/3.2.2.LTS).

Note: Download Latest version

**Build**

Build your MSPM0G3507 program using either GCC or Code Composer studio (CSS).

**Generate Hex Image**

```
pathToClangCompiler/bin/tiarmhex --diag_wrap=off --intel --memwidth 8 --romwidth 8 -o [.hex application_image] [.out application_image]
```

For example:

```
pathToClangCompiler/bin/tiarmhex  --diag_wrap=off --intel --memwidth 8 --romwidth 8 -o application_image.hex application_image.out
```

## Author

[Goutham S Krishna](https://www.linkedin.com/in/goutham-s-krishna-21ab151a0/)
