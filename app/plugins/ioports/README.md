# Input/Output Plugin

## Description

This plugin exposes the Input/Output Ports of the Axis device over the OPC-UA
protocol. It aims at providing the OPC-UA equivalent of the [I/O port management API](https://developer.axis.com/vapix/network-video/io-port-management).

## Features

The plugin presents the I/O Ports as OPC-UA objects in a tree structure as
follows:

- **I/O Ports** (objects folder)
    - **I/O Port \<#\>** (object with following properties:)
        - `Configurable`: true/false
        - `Direction`: 0 (Input)/1 (Output)
        - `Disabled`: true/false
        - `Index`: integer
        - `Name`: string
        - `NormalState`: 0 (Open)/1 (Closed)
        - `State`: 0 (Open)/1 (Closed)
        - `Usage`: string

The plugin also implements an OPC-UA event type: `IOPStateEventType` (a subtype of
`IOPEventType`). This OPC-UA event is sent out when the current state of an I/O
Port changes.

| Property     | Access    | Type                | Description                 |
|--------------|-----------|---------------------|-----------------------------|
| Configurable | R/O       | Boolean             | Indicates if the direction of the I/O port is user configurable or not |
| Direction    | R/O - R/W | IOPortDirectionType | Determines if the port is an input or an output. R/O if `Configurable` is FALSE. |
| Disabled     | R/O       | Boolean             | If TRUE no port properties can be changed by the user |
| Index        | R/O       | Int32               | The port number/index       |
| Name         | R/W       | String              | User configurable name assigned to the port |
| NormalState  | R/W       | IOPortStateType     | The desired normal state of the port: `OPEN` or `CLOSED` |
| State        | R/O - R/W | IOPortStateType     | The current state of the port: `OPEN` or `CLOSED` |
| Usage        | R/W       | String              | User configurable usage description for the port |

## License

**[MIT License](../../../LICENSE)**
