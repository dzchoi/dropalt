#### String descriptors
* The device descriptor assigns iSerialNumber, iManufacturer, and iProduct unique indices that correspond to specific string descriptors. All string descriptors are stored in a linked list using `usbus_add_string_descriptor()`. When the host requests a string descriptor by its index, `_req_descriptor()` in usbus_control.c searches the linked list for the specified index and sends back the corresponding string descriptor.

> Serial numbers are useful if users may have more than one identical device on the bus and the host needs to remember which device is which even after rebooting. A serial number also enables a host to determine whether a device is the same one used previously or a new installation of a device with the same Vendor ID and Product ID.

* See Table 4-2 "Device descriptor" in USB Complete 4th ed.
* See Table 4-12 "String descriptor" in USB Complete 4th ed. (p. 113)

* USB descriptor strings are required to be in UTF-16 format. In RIOT, these strings are stored as ASCII, and when the host requests a string descriptor, RIOT converts the ASCII representation to UTF-16 before sending the response (_cpy_str_to_utf16() in usbus_control.c).

* Identifying which /dev/ttyACMx corresponds to a specific iSerial:
  The /dev/serial/ directory contains symbolic links specifically for devices associated with serial communication, such as USB-to-serial converters or devices that present themselves as serial ports (e.g., /dev/ttyUSBx or /dev/ttyACMx). These symbolic links often include details like the iManufacturer, iProduct, and iSerial to help identify devices.
  > `ls /dev/serial/by-id/*15HMMKAG010321*`
