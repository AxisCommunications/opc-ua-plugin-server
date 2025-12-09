# Virtual Input Plugin

## Description

This plugin exposes the [Virtual Input API](https://developer.axis.com/vapix/network-video/input-and-outputs/#virtual-input-api) over OPC-UA.

## Features

The Virtual Inputs are presented in the form of an OPC-UA object with
`Activate`/`Deactivate` methods and boolean variable nodes modelling the virtual
input ports. These can also be accessed via OPC-UA read/write operations.

- **VirtualInputs** (object)
    - **Activate** (method)
    - **Deactivate** (method)
    - **VirtualInput-1** (variable)
    - **VirtualInput-2** (variable)
    - **...**

## Usage

The `Activate` method maps to the `/axis-cgi/virtualinput/activate.cgi` CGI.
It takes two input parameters: `<port number>` and `<duration>`. Use `"-1"` as
duration value to leave the port in active state indefinitely.

The `Deactivate` method maps to the `/axis-cgi/virtualinput/deactivate.cgi` CGI.
It takes only one input parameter: the `<port number>`. It deactivates the
virtual input port.

The state of the virtual input ports can also be read or written via direct
OPC-UA read/write operations, they are exposed as boolean variable nodes.

## License

**[MIT License](../../../LICENSE)**
