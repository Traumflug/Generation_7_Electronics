
/*
  Tool to forward an USB-CDC device using Microchip's MCP2200 USB-UART bridge
  to a pseudo terminal. This is useful on Mac OS X prior to v10.7, where
  this chip connects, but Mac OS X doesn't know about the IAD protocol, so
  it doesn't recognize the available serial line.

  This tool is inspired by code written by Jay Carlson, found here:
  http://forum.pololu.com/viewtopic.php?t=3323#p15713 and was rewritten
  on the base of sample code coming with Apple's "USB Device Interface Guide".
  Earlier attempts tried to continue with using libusb, but even after many
  hours of debugging, libusb couldn't establish reliable connections.

  Copyright (c) 2014 Markus Hitter <mah@jump-ing.de>

  Permission to use, copy, modify, and/or distribute this software for
  any purpose with or without fee is hereby granted, provided that the
  above copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
  WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
  BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
  OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
  WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
  SOFTWARE.
*/

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/usb/USBSpec.h>
#include <Kernel/mach/mach_port.h>

// For PTY.
#include <util.h>
#include <fcntl.h>
#include <termios.h>

// For read() and write().
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>


// Configuration constants.
static const SInt32 kVendorID = 0x04d8;
static const SInt32 kProductID = 0x00df;

// Global variables.
static IONotificationPortRef   gNotifyPort;
static io_iterator_t           gAddedIter;
static io_iterator_t           gRemovedIter;
static IOUSBInterfaceInterface **gUsbInterruptInterface = NULL;
static int                     gInterruptPipe;
static IOUSBInterfaceInterface **gUsbBulkInterface = NULL;
static int                     gReadPipe;
static UInt32                  gReadBufferSize;
static char*                   gReadBuffer = NULL;
static int                     gWritePipe;
static UInt32                  gWriteBufferSize;
static char*                   gWriteBuffer = NULL;
static int                     gPtyPipe = -1;
static int                     gVerbosity = 0;
static char*                   gLinkPath = NULL;
static UInt32                  gBaudRate = 115200;


// From various headers in AppleUSBCDCDriver.
// https://www.opensource.apple.com/source/AppleUSBCDCDriver/
typedef struct {
  UInt32 dwDTERate;
  UInt8  bCharFormat;
  UInt8  bParityType;
  UInt8  bDataBits;
} __attribute__((packed)) LineCoding;

enum {
  dwDTERateOffset = 0,
  wValueOffset    = 2,
  wIndexOffset    = 4,
  wLengthOffset	  = 6
};

enum {
  kUSBSEND_ENCAPSULATED_COMMAND = 0, // Requests
  kUSBGET_ENCAPSULATED_RESPONSE = 1,
  kUSBSET_COMM_FEATURE          = 2,
  kUSBGET_COMM_FEATURE          = 3,
  kUSBCLEAR_COMM_FEATURE        = 4,
  kUSBRESET_FUNCTION            = 5,
  kUSBSET_LINE_CODING           = 0x20,
  kUSBGET_LINE_CODING           = 0x21,
  kUSBSET_CONTROL_LINE_STATE    = 0x22,
  kUSBSEND_BREAK                = 0x23
};


static IOReturn
configure_device(IOUSBDeviceInterface **dev) {
  UInt8                           numConfig;
  IOReturn                        kr;
  IOUSBConfigurationDescriptorPtr configDesc;

  // Get the number of configurations. The sample code always chooses
  // the first configuration (at index 0) but your code may need a
  // different one.
  kr = (*dev)->GetNumberOfConfigurations(dev, &numConfig);
  if ( ! numConfig)
    return -1;

  // Get the configuration descriptor for index 0.
  kr = (*dev)->GetConfigurationDescriptorPtr(dev, 0, &configDesc);
  if (kr) {
    fprintf(stderr,
            "Couldn’t get configuration descriptor for index %d. (%08x)\n",
            0, kr);
    return -1;
  }

  // Set the device’s configuration. The configuration value is found in
  // the bConfigurationValue field of the configuration descriptor.
  kr = (*dev)->SetConfiguration(dev, configDesc->bConfigurationValue);
  if (kr) {
    fprintf(stderr, "Couldn’t set configuration to value %d. (%08x)\n", 0, kr);
    return -1;
  }

  return kIOReturnSuccess;
}

static void
close_interface(void) {

  if (gVerbosity >= 3)
    printf("close_interface()\n");

  if (gReadBuffer != NULL) {
    free(gReadBuffer);
    gReadBuffer = NULL;
  }
  if (gWriteBuffer != NULL) {
    free(gWriteBuffer);
    gWriteBuffer = NULL;
  }
  if (gUsbBulkInterface != NULL) {
    (void)(*gUsbBulkInterface)->USBInterfaceClose(gUsbBulkInterface);
    (void)(*gUsbBulkInterface)->Release(gUsbBulkInterface);
    gUsbBulkInterface = NULL;
  }
  if (gUsbInterruptInterface != NULL) {
    (void)(*gUsbInterruptInterface)->USBInterfaceClose(gUsbInterruptInterface);
    (void)(*gUsbInterruptInterface)->Release(gUsbInterruptInterface);
    gUsbInterruptInterface = NULL;
  }
}

/**
  Send USB-UART bridge setup. Instead of wading though 100s of pages of
  documentation, this was sniffed from a working setup. Inspiration also from
  Apple Open Source ...
    https://www.opensource.apple.com/source/AppleUSBCDCDriver/
            AppleUSBCDCDriver-4201.2.5/AppleUSBCDCACM/ControlDriver/
            Classes/AppleUSBCDCACMControl.cpp
  ... and from libusb-0.1.12, file darwin.c
*/
static IOReturn
send_bridge_setup(IOUSBDeviceInterface **device) {
  IOUSBDevRequest request;
  LineCoding lineParams;
  IOReturn kr;

  if (gVerbosity >= 3)
    printf("send_bridge_setup()\n");

  if (gUsbInterruptInterface == NULL) {
    fprintf(stderr, "send_bridge_setup() without interface available.\n");
    return kIOUSBConfigNotFound;
  }

  // Step one: take DTE & RTS down.
  request.bmRequestType = 0x21; // USBmakebmRequestType(...some constants...);
  request.bRequest = kUSBSET_CONTROL_LINE_STATE;
  request.wValue = 0x00;
  request.wIndex = gInterruptPipe;
  request.wLength = 0;
  request.pData = NULL;

  kr = (*device)->DeviceRequest(device, &request);
  if (kr != kIOReturnSuccess) {
    fprintf(stderr, "Failed to take down DTE & RTS. Ignoring. (%08x)\n", kr);
  }

  // Step two: send baud rate & co.
  OSWriteLittleInt32(&lineParams, dwDTERateOffset, gBaudRate);
  lineParams.bCharFormat = 0x00;
  lineParams.bParityType = 0x00;
  lineParams.bDataBits = 0x08;

  request.bmRequestType = 0x21;
  request.bRequest = kUSBSET_LINE_CODING;
  request.wValue = 0x00;
  request.wIndex = gInterruptPipe;
  request.wLength = sizeof(lineParams);
  request.pData = &lineParams;

  kr = (*device)->DeviceRequest(device, &request);
  if (kr != kIOReturnSuccess) {
    fprintf(stderr, "Failed to set baud rate & co. Ignoring. (%08x)\n", kr);
  }

  // Step three: raise DTE and RTS again.
  request.bmRequestType = 0x21;
  request.bRequest = kUSBSET_CONTROL_LINE_STATE;
  request.wValue = 0x03; // kRTSOn | kDTROn
  request.wIndex = gInterruptPipe;
  request.wLength = 0;
  request.pData = NULL;

  kr = (*device)->DeviceRequest(device, &request);
  if (kr != kIOReturnSuccess) {
    fprintf(stderr, "Failed to raise DTE & RTS. Ignoring. (%08x)\n", kr);
  }

  return kr;
}

static void
read_completion(void *refCon, IOReturn result, void *arg0) {
  IOUSBInterfaceInterface **interface = (IOUSBInterfaceInterface **)refCon;
  // Wrongly documented, but shown in sample code: arg0 isn't a (void *),
  // but actually an UInt32 holding the number of bytes read.
  IOReturn kr;

  if (gVerbosity >= 3)
    printf("read_completion()\n");

  if (result != kIOReturnSuccess) {
    fprintf(stderr, "Error from async bulk read. (%08x)\n", result);
    close_interface();
    return;
  }

  // Do our duty.
  if (gPtyPipe >= 0) {
    if (write(gPtyPipe, gReadBuffer, (size_t)arg0) != (UInt32)arg0) {
      perror("Write to PTY");
    }
  }

  // Setting up a new read request over and over again is apparently the only
  // way to get continuous incoming data.
  // CAUTION: last argument to ReadPipeAsync() becomes the first argument to
  //          the callback, so both must be "interface" to keep the chain up.
  kr = (*interface)->ReadPipeAsync(interface, gReadPipe, gReadBuffer,
                                   gReadBufferSize, read_completion,
                                   interface);
  if (kr != kIOReturnSuccess) {
    fprintf(stderr, "Unable to perform asynchronous bulk read. (%08x)\n", kr);
    close_interface();
    return;
  }
}

static void
find_interfaces(IOUSBDeviceInterface **device) {
  IOReturn                  kr;
  IOUSBFindInterfaceRequest request;
  io_iterator_t             iterator;
  io_service_t              usbInterface;
  IOCFPlugInInterface       **plugInInterface = NULL;
  IOUSBInterfaceInterface   **interface = NULL;
  HRESULT                   result;
  SInt32                    score;
  UInt8                     interfaceClass;
  UInt8                     interfaceSubClass;
  UInt8                     interfaceNumEndpoints;
  int                       pipeRef;
  UInt8                     interfaceTransferType;
  CFRunLoopSourceRef        runLoopSource;

  if (gVerbosity >= 3)
    printf("find_interfaces()\n");

  // Placing the constant kIOUSBFindInterfaceDontCare into the following
  // fields of the IOUSBFindInterfaceRequest structure will allow you
  // to find all the interfaces.
  request.bInterfaceClass = kIOUSBFindInterfaceDontCare;
  request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
  request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
  request.bAlternateSetting = kIOUSBFindInterfaceDontCare;

  if (gVerbosity >= 1)
    printf("Scanning interfaces ...\n");

  // Get an iterator for the interfaces on the device.
  kr = (*device)->CreateInterfaceIterator(device, &request, &iterator);
  while ((usbInterface = IOIteratorNext(iterator)) != 0) {

    if (gVerbosity >= 1)
      printf("Interface 0x%08X.\n", usbInterface);

    // Create an intermediate plug-in.
    kr = IOCreatePlugInInterfaceForService(usbInterface,
                                           kIOUSBInterfaceUserClientTypeID,
                                           kIOCFPlugInInterfaceID,
                                           &plugInInterface, &score);

    // Release the usbInterface object after getting the plug-in.
    kr = IOObjectRelease(usbInterface);
    if ((kr != kIOReturnSuccess) || ! plugInInterface) {
      fprintf(stderr, "Unable to create a plug-in. (%08x)\n", kr);
      continue;
    }

    // Now create the device interface for the interface.
    result = (*plugInInterface)->QueryInterface(plugInInterface,
               CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID),
               (LPVOID *)&interface);

    // No longer need the intermediate plug-in.
    (*plugInInterface)->Release(plugInInterface);
    if (result || ! interface) {
      fprintf(stderr,
              "Couldn’t create a device interface for the interface. (%08x)\n",
              (int)result);
      continue;
    }

    // Get interface class and subclass.
    (*interface)->GetInterfaceClass(interface, &interfaceClass);
    (*interface)->GetInterfaceSubClass(interface, &interfaceSubClass);
    if (gVerbosity >= 1)
      printf("Interface class %d, subclass %d.\n",
             interfaceClass, interfaceSubClass);

    // Now open the interface. This will cause the pipes associated with
    // the endpoints in the interface descriptor to be instantiated.
    kr = (*interface)->USBInterfaceOpen(interface);
    if (kr != kIOReturnSuccess) {
      if (gVerbosity >= 1)
        printf("Unable to open interface. Discarding it. (%08x)\n", kr);
      (void)(*interface)->Release(interface);
      continue;
    }

    // Get the number of endpoints associated with this interface.
    kr = (*interface)->GetNumEndpoints(interface, &interfaceNumEndpoints);
    if (kr != kIOReturnSuccess) {
      fprintf(stderr, "Unable to get number of endpoints. (%08x)\n", kr);
      (void)(*interface)->USBInterfaceClose(interface);
      (void)(*interface)->Release(interface);
      continue;
    }
    if (gVerbosity >= 1)
      printf("Interface has %d endpoints.\n", interfaceNumEndpoints);

    // Access each pipe in turn, starting with the pipe at index 1.
    // The pipe at index 0 is the default control pipe and should be
    // accessed using (*usbDevice)->DeviceRequest() instead.
    for (pipeRef = 1; pipeRef <= interfaceNumEndpoints; pipeRef++) {
      IOReturn kr2;
      UInt8    direction;
      UInt8    number;
      UInt16   maxPacketSize;
      UInt8    interval;
      char     *message;

      kr2 = (*interface)->GetPipeProperties(interface,
                                            pipeRef, &direction,
                                            &number, &interfaceTransferType,
                                            &maxPacketSize, &interval);
      if (kr2 != kIOReturnSuccess)
        fprintf(stderr, "Unable to get properties of pipe %d. (%08x)\n",
                pipeRef, kr2);
      else {
        if (gVerbosity >= 1)
          printf("PipeRef %d: ", pipeRef);

        switch (direction) {
          case kUSBOut:
            message = "out";
            break;
          case kUSBIn:
            message = "in";
            break;
          case kUSBNone:
            message = "none";
            break;
          case kUSBAnyDirn:
            message = "any";
            break;
          default:
            message = "???";
        }
        if (gVerbosity >= 1)
          printf("direction %s, ", message);

        switch (interfaceTransferType) {
          case kUSBControl:
            message = "control";
            break;
          case kUSBIsoc:
            message = "isoc";
            break;
          case kUSBBulk:
            message = "bulk";
            break;
          case kUSBInterrupt:
            message = "interrupt";
            break;
          case kUSBAnyType:
            message = "any";
            break;
          default:
            message = "???";
        }
        if (gVerbosity >= 1)
          printf("transfer type %s, maxPacketSize %d.\n",
                 message, maxPacketSize);

        // Prematurely remember useful pipes.
        if (gUsbBulkInterface == NULL) {
          if (direction == kUSBIn) {
            gReadPipe = pipeRef;
            gReadBufferSize = maxPacketSize;
          }
          if (direction == kUSBOut) {
            gWritePipe = pipeRef;
            gWriteBufferSize = maxPacketSize;
          }
        }
        if (gUsbInterruptInterface == NULL &&
            interfaceTransferType == kUSBInterrupt) {
          gInterruptPipe = pipeRef;
        }
      }
    }

    // Pick interesting interfaces. Take the first of each (but there should
    // be only one of each type).
    if (gUsbBulkInterface == NULL && interfaceTransferType == kUSBBulk) {

      gUsbBulkInterface = interface;
      if (gVerbosity >= 1)
        printf("Choosing this interface as Bulk.\n");

      gReadBuffer = malloc(gReadBufferSize);
      if (gReadBuffer == NULL) {
        perror("Unable to allocate read buffer");
        close_interface();
        break;
      }
      gWriteBuffer = malloc(gWriteBufferSize);
      if (gWriteBuffer == NULL) {
        perror("Unable to allocate write buffer");
        close_interface();
        break;
      }

      // As with service matching notifications, to receive asynchronous
      // I/O completion notifications, you must create an event source and
      // add it to the run loop.
      kr = (*interface)->CreateInterfaceAsyncEventSource(interface,
                                                         &runLoopSource);
      if (kr != kIOReturnSuccess) {
        fprintf(stderr, "Unable to create asynchronous event source. (%08x)\n",
                kr);
        close_interface();
        break;
      }
      CFRunLoopAddSource(CFRunLoopGetCurrent(),
                         runLoopSource, kCFRunLoopDefaultMode);

      // Initiate our reading chain.
      // Hints on ReadPipeAsync() see read_completion().
      kr = (*interface)->ReadPipeAsync(interface, gReadPipe, gReadBuffer,
                                       gReadBufferSize, read_completion,
                                       interface);
      if (kr != kIOReturnSuccess) {
        fprintf(stderr, "Unable to perform first bulk read. (%08x)\n", kr);
        close_interface();
        break;
      }
    }
    else if (gUsbInterruptInterface == NULL &&
             interfaceTransferType == kUSBInterrupt) {
      gUsbInterruptInterface = interface;
      if (gVerbosity >= 1)
        printf("Choosing this interface as Interrupt.\n");
    }
    else {
      (void)(*interface)->USBInterfaceClose(interface);
      (void)(*interface)->Release(interface);
      if (gVerbosity >= 1)
        printf("Discarding this interface.\n");
    }
  }

  if (gVerbosity >= 1)
    printf("... interface scan done.\n");
}

static void
device_added(void *refCon, io_iterator_t iterator) {
  kern_return_t        kr;
  io_service_t         usbDevice;
  IOCFPlugInInterface  **plugInInterface = NULL;
  IOUSBDeviceInterface **device = NULL;
  HRESULT              result;
  SInt32               score;
  UInt16               vendor;
  UInt16               product;

  if (gVerbosity >= 3)
    printf("device_added()\n");

  while ((usbDevice = IOIteratorNext(iterator)) != 0) {
    if (gVerbosity >= 1)
      printf("Device added.\n");

    // Create an intermediate plug-in.
    kr = IOCreatePlugInInterfaceForService(usbDevice,
                                           kIOUSBDeviceUserClientTypeID,
                                           kIOCFPlugInInterfaceID,
                                           &plugInInterface, &score);

    // Don’t need the device object after intermediate plug-in is created.
    kr = IOObjectRelease(usbDevice);
    if ((kIOReturnSuccess != kr) || ! plugInInterface) {
      fprintf(stderr, "Unable to create a plug-in. (%08x)\n", kr);
      continue;
    }

    // Now create the device interface.
    result = (*plugInInterface)->QueryInterface(plugInInterface,
               CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID *)&device);

    // Don’t need the intermediate plug-in after device interface
    // is created.
    (*plugInInterface)->Release(plugInInterface);
    if (result || ! device) {
      fprintf(stderr, "Couldn’t create a device interface. (%08x)\n",
              (int)result);
      continue;
    }

    // Check these values for confirmation.
    kr = (*device)->GetDeviceVendor(device, &vendor);
    kr = (*device)->GetDeviceProduct(device, &product);
    if ((vendor != kVendorID) || (product != kProductID)) {
      fprintf(stderr,
              "Found unwanted device (vendor = 0x%04X, product = 0x%04X).\n",
              vendor, product);
      (void)(*device)->Release(device);
      continue;
    }

    // Open the device to change its state.
    kr = (*device)->USBDeviceOpen(device);
    if (kr != kIOReturnSuccess) {
      fprintf(stderr, "Unable to open device %08x.\n", kr);
      (void)(*device)->Release(device);
      continue;
    }

    // Configure device.
    kr = configure_device(device);
    if (kr != kIOReturnSuccess) {
      fprintf(stderr, "Unable to configure device %08x.\n", kr);
      (void)(*device)->USBDeviceClose(device);
      (void)(*device)->Release(device);
      continue;
    }

    // Get the interfaces.
    find_interfaces(device);
    if (gUsbBulkInterface == NULL || gUsbInterruptInterface == NULL) {
      fprintf(stderr, "Unable to find interfaces on device.\n");
      (*device)->USBDeviceClose(device);
      (*device)->Release(device);
      continue;
    }

    // Now we have interfaces, set up the USB-UART bridge.
    kr = send_bridge_setup(device);
    if (kr != kIOReturnSuccess) {
      fprintf(stderr, "Unable to set up USB-UART bridge. (%08x)\n", kr);
      close_interface();
      continue;
    }
  }
}

static void
device_removed(void *refCon, io_iterator_t iterator) {
  kern_return_t kr;
  io_service_t  object;

  if (gVerbosity >= 3)
    printf("device_removed()\n");

  // NOTE: all these events can be called for any matching device, not only
  // our wanted one. Luckily we support a single matching device only, so
  // we don't have to keep track of what belongs to what, but this function
  // is definitely called with an empty iterator, see main().

  while ((object = IOIteratorNext(iterator)) != 0) {
    kr = IOObjectRelease(object);
    if (kr != kIOReturnSuccess) {
      fprintf(stderr, "Couldn’t release device object %08x.\n", kr);
      continue;
    }
    close_interface();
    if (gVerbosity >= 1)
      printf("Device removed.\n");
  }
}

static void
pty_open(void) {
  // Our end is the "master", gPtyPipe. Pronterface will connect to the other
  // end (slave), giving us the data forwarding chain we want.
  // See https://blog.nelhage.com/images/posts/2009/12/termios.png
  int slave;

  if (gVerbosity >= 3)
    printf("pty_open()\n");

  if (openpty(&gPtyPipe, &slave, NULL, NULL, NULL)) {
    perror("openpty");
    gPtyPipe = -1;
    return;
  }

  // Set the master file descriptor to our needs. From here on, we report,
  // but ignore errors. Let's see how this works out.
  struct termios t;

  if (tcgetattr(gPtyPipe, &t)) {
    perror("tcgetattr gPtyPipe");
  }
  cfmakeraw(&t);
  if (tcsetattr(gPtyPipe, TCSANOW, &t)) {
    perror("tcsetattr gPtyPipe");
  }
  fcntl(gPtyPipe, F_SETFL, O_APPEND);
  if (grantpt(gPtyPipe)) {
    perror("grantpt gPtyPipe");
  }
  if (unlockpt(gPtyPipe)) {
    perror("unlockpt gPtyPipe");
  }
  // Pseudo terminals don't care about baud rate.

  // Just in case Pronterface forgets, adjust the other end, too.
  if (tcgetattr(slave, &t)) {
    perror("tcgetattr slave");
  }
  cfmakeraw(&t);
  if (tcsetattr(slave, TCSANOW, &t)) {
    perror("tcsetattr slave");
  }
  fcntl(slave, F_SETFL, O_APPEND);
}

static void
pty_read(CFReadStreamRef stream, CFStreamEventType eventType, void *info) {
  const CFIndex chunkSize = 10;
  UInt8 buffer[chunkSize + 1];
  CFIndex length;
  IOReturn kr;

  if (gVerbosity >= 3)
    printf("pty_read(), event 0x%04X\n", eventType);

  length = CFReadStreamRead(stream, (UInt8 *)&buffer, chunkSize);

  if (gUsbBulkInterface != NULL) {
    kr = (*gUsbBulkInterface)->WritePipe(gUsbBulkInterface,
                                         gWritePipe, buffer, length);
    if (kr != kIOReturnSuccess) {
      printf("Unable to perform bulk write (%08x)\n", kr);
      close_interface();
    }
  }
}

static void
version(void) {
  printf("MCP2200 Forwarder v0.9\n");
}

static void
usage(char * const argv0) {
  printf("\n");
  version();
  printf("\n");
  printf("Userspace driver for connecting USB-Serial adapters on Mac OS X\n");
  printf("prior to v10.7. If your adapter doesn't show up in /dev\n");
  printf("automatically, run this tool in background and connect to the\n");
  printf("file it prints, instead. Should work in most cases just like a\n");
  printf("real device.\n");
  printf("\n");
  printf("Compiled for vendor ID 0x%04X, product ID 0x%04X (factory fresh\n",
         (unsigned int)kVendorID, (unsigned int)kProductID);
  printf("Microchip MCP2200, like the one on a Generation 7 Electronics).\n");
  printf("\n");
  printf("Usage: %s [-hvvvV] [-b <baud rate>] [-l <path>]\n", argv0);
  printf("\n");
  printf("Options:\n");
  printf("  -b  Baud rate to configure the USB-UART bridge to (default 115200).\n");
  printf("  -h  Display this help and exit.\n");
  printf("  -l  Create a symbolic link at <path> to the PTY used. While the\n");
  printf("      exact PTY used depends on how many other applications use\n");
  printf("      PTYs, this path will be always the same.\n");
  printf("  -v  Increase verbosity (up to 3).\n");
  printf("  -V  Display version and exit.\n");
  printf("\n");
  printf("Copyright (c) 2014 Markus Hitter <mah@jump-ing.de>\n");
}

static void
remove_link(void) {
  if (unlink(gLinkPath))
    perror("Unlink error");
}

static void
signal_exit(int n) {
  remove_link();
  _exit(0);
}

int
main (int argc, char * const argv[]) {
  mach_port_t            masterPort;
  CFMutableDictionaryRef matchingDict;
  CFRunLoopSourceRef     runLoopSource;
  kern_return_t          kr;
  int                    ch;

  while ((ch = getopt(argc, argv, "b:hl:vV")) != -1) {
    switch (ch) {
      case 'b':
        gBaudRate = atol(optarg);
        break;
      case 'h':
        usage(argv[0]);
        exit(0);
        break;
      case 'l':
        gLinkPath = malloc(strlen(optarg)); // Free'd at program exit.
        if ( ! gLinkPath) {
          perror("Malloc for -l");
        }
        else {
          strcpy(gLinkPath, optarg);
        }
        break;
      case 'v':
        gVerbosity++;
        break;
      case 'V':
        version();
        exit(0);
        break;
      case '?':
      default:
        usage(argv[0]);
        exit(-1);
        break;
    }
  }

  // Create a master port for communication with the I/O Kit.
  kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
  if (kr || ! masterPort) {
    fprintf(stderr, "Couldn’t create a master I/O Kit port. (%08x)\n", kr);
    return -1;
  }

  // Set up matching dictionary for class IOUSBDevice and its subclasses.
  matchingDict = IOServiceMatching(kIOUSBDeviceClassName);
  if ( ! matchingDict) {
    fprintf(stderr, "Couldn’t create a USB matching dictionary.\n");
    mach_port_deallocate(mach_task_self(), masterPort);
    return -1;
  }

  // Add the vendor and product IDs to the matching dictionary.
  // This is the second key in the table of device-matching keys of the
  // USB Common Class Specification.
  CFDictionarySetValue(matchingDict, CFSTR(kUSBVendorName),
                       CFNumberCreate(kCFAllocatorDefault,
                                      kCFNumberSInt32Type,
                                      &kVendorID));
  CFDictionarySetValue(matchingDict, CFSTR(kUSBProductName),
                       CFNumberCreate(kCFAllocatorDefault,
                                      kCFNumberSInt32Type,
                                      &kProductID));

  // To set up asynchronous notifications, create a notification port and
  // add its run loop event source to the program’s run loop.
  gNotifyPort = IONotificationPortCreate(masterPort);
  runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);
  CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource,
                     kCFRunLoopDefaultMode);

  // Retain additional dictionary references because each call to
  // IOServiceAddMatchingNotification consumes one reference.
  matchingDict = (CFMutableDictionaryRef)CFRetain(matchingDict);
  matchingDict = (CFMutableDictionaryRef)CFRetain(matchingDict);
  matchingDict = (CFMutableDictionaryRef)CFRetain(matchingDict);

  // Now set up two notifications: one to be called when a matching device
  // is first matched by the I/O Kit and another to be called when the
  // device is terminated.

  // Notification of first match (USB device appeared):
  kr = IOServiceAddMatchingNotification(gNotifyPort, kIOFirstMatchNotification,
                                        matchingDict, device_added, NULL,
                                        &gAddedIter);
  // Iterate over set of matching devices to access already-present devices
  // and to arm the notification.
  device_added(NULL, gAddedIter);

  // Notification of termination (USB device disappeared):
  kr = IOServiceAddMatchingNotification(gNotifyPort, kIOTerminatedNotification,
                                        matchingDict, device_removed, NULL,
                                        &gRemovedIter);
  // Iterate over set of matching devices to release each one and to
  // arm the notification.
  device_removed(NULL, gRemovedIter);

  // Finished with master port.
  mach_port_deallocate(mach_task_self(), masterPort);
  masterPort = 0;


  // USB part done. Now connect to a pseudo terminal (PTY). We'll keep this
  // connection until program end.
  pty_open();
  if (gPtyPipe < 0) {
    return -1;
  }

  // Tell the user which PTY to connect with.
  printf("Terminal name: %s\n", ptsname(gPtyPipe));

  if (gLinkPath) {
    if (symlink(ptsname(gPtyPipe), gLinkPath)) {
      perror("Symlink error");
    }
    else {
      printf("Created symlink to %s\n", gLinkPath);
      atexit(remove_link);
      signal(SIGHUP, signal_exit);
      signal(SIGINT, signal_exit);
      signal(SIGTERM, signal_exit);
    }
  }


  // The whole purpose of this tool is to forward data between USB and PTY.
  // How do we do this forwarding?
  //
  // For USB -> PTY we do asynchronous reads, starting right after the device
  // got connected (or right at app launch). Such a read writes the data to
  // the PTY and requests the next async read immediately. Crossing fingers
  // this chain never breaks. See read_completion() above.
  //
  // For PTY -> USB we subscribe for "bytes available" events in the run loop
  // and actually forward the data in its callback (pty_read()). That's what we
  // set up here.
  CFReadStreamRef ptyToUsbStream;
  CFStreamClientContext streamContext = { 0, NULL, NULL, NULL, NULL };
  bool res;

  CFStreamCreatePairWithSocket(NULL, gPtyPipe, &ptyToUsbStream, NULL);
  
  res = CFReadStreamOpen(ptyToUsbStream);
  if ( ! res) {
    fprintf(stderr, "Failed to open stream.\n");
    CFStreamError err = CFReadStreamGetError(ptyToUsbStream);
    if (err.domain == kCFStreamErrorDomainPOSIX) {
      fprintf(stderr, "errno: %s\n", strerror(err.error));
    }
  }

  CFReadStreamSetClient(ptyToUsbStream, kCFStreamEventHasBytesAvailable,
                        pty_read, &streamContext);

  CFReadStreamScheduleWithRunLoop(ptyToUsbStream, CFRunLoopGetCurrent(),
                                  kCFRunLoopDefaultMode);


  // Start the run loop so notifications and events will be received.
  CFRunLoopRun();

  // Because the run loop will run forever until interrupted,
  // the program should never reach this point.
  return 0;
}
