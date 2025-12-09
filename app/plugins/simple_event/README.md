# Simple Event Plugin

## Description

This example plugin demonstrates how to integrate Axis events with OPC-UA, it:

- Subscribes to Axis "LiveStreamAccessed" events.
- Generates OPC-UA events when the Axis `LiveStreamAccessed` event is triggered.
- Shows how to map Axis event data to standard OPC-UA event properties.

## Features

- Creates an OPC-UA object node called **LiveStreamAccessed**, which acts as
an event notifier for these events.
- Exposes an `Accessed` boolean property showing the current state of the
live stream access.
- Triggers OPC-UA events with the following standard properties:
    - `Timestamp`
    - `Severity` (fixed to 500 in this example).
    - `Message`
    - `SourceName`

## Usage

- Subscribe to the `Accessed` variable to see the current state of the Axis
event
- To read the OPC-UA Events, subscribe to the `LiveStreamAccessed` object using
the event view in UAExpert

## Development

To adapt this plugin for other Axis events (functions mentioned below are found
in `simple_event_plugin.c`):

1. **Change the event subscription**:
    - Modify the topics / key-value pairs in `setup_ax_event()` to subscribe to
    different Axis events.
    - Update the event topic names in the key-value set to match the desired
    Axis events.

2. **Customize the event object**:
    - Change the object name and description as presented in the OPC-UA address
    space.
    - Add or remove properties in `create_event_object()` to map additional Axis
    event data.

3. **Handle different event data**:
    - Update the callback function `simple_opc_event_cb()` to process the
    specific data fields from the new Axis event.
    - Extract different fields from the Axis event and map them to the
    properties of the OPC-UA event object.

## License

**[MIT License](../../../LICENSE)**
