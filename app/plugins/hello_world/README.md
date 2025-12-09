# Hello World Plugin

## Description

This is a simple example plugin that demonstrates how to create a basic OPC-UA
variable node. It creates a single read/write variable node called
"HelloWorldNode" and serves as a template for developing custom plugins.

## Features

- Creates a single OPC-UA variable node called **HelloWorldNode**
under the Objects folder.
- The node supports both read and write operations.
- Its default value is "Hello World!".

## Development

To create your own plugin based on this example:

- **Create a new plugin directory:**

    ```bash
    cd app/plugins
    mkdir my_new_plugin
    cd my_new_plugin
    ```

- **Copy the contents of hello_world plugin:**

    ```bash
    cp ../hello_world/Makefile .
    cp ../hello_world/hello_world_plugin.h my_plugin.h
    cp ../hello_world/hello_world_plugin.c my_plugin.c
    ```

- **Mandatory changes:**
    - Update the namespace in `my_plugin.c` (change `UA_PLUGIN_NAMESPACE`).
    This ensures your plugin has a unique identifier within the OPC-UA server.
    - Update the plugin name in `my_plugin.c` (change `UA_PLUGIN_NAME`).
    This defines the name by which the plugin is identified.

## License

**[MIT License](../../../LICENSE)**
