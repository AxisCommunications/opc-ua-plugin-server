# Basic Device Information Plugin

## Description

This plugin provides basic device information through OPC-UA nodes. It exposes
various properties of the Axis device such as model name, firmware version,
serial number, and other device-specific information.

## Features

- **OPC-UA Nodes:**
    - The plugin creates an object node called **BasicDeviceInfo** under the
    Objects folder.
    - This object contains multiple variable nodes representing different
    device properties.
    - Example properties include: ModelName, FirmwareVersion, SerialNumber,
    HardwareID.

- **Interacting with the Plugin:**
    - Connect to the OPC-UA server using an OPC-UA client like UaExpert
    (as described in the main ACAP README).
    - Navigate to the **BasicDeviceInfo** object node to view all available
    device properties.
    - Read operations can be performed on individual property nodes to
    retrieve their values.

## License

**[MIT License](../../../LICENSE)**
