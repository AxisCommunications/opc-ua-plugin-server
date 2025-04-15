*©2025 Axis Communications AB. AXIS COMMUNICATIONS, AXIS, ARTPEC and VAPIX are registered trademarks of Axis AB in various jurisdictions. All other trademarks are the property of their respective owners.*

<!-- omit in toc -->
# OPC-UA Plugin Server ACAP

This repository contains the source code for building an OPC-UA Server ACAP,
along with a guide on developing and building custom modules/plugins.
Two example plugins are provided.

The OPC-UA Server is based on the [**open62541**](https://www.open62541.org/)
library. For an introduction to OPC-UA, see
[**OPC Foundation**](https://opcfoundation.org/).

<!-- omit in toc -->
## Table of contents

- [Build the ACAP](#build-the-acap)
    - [Make and Docker](#make-and-docker)
    - [Docker](#docker)
    - [Docker (development containers)](#docker-development-containers)
- [Install and setup](#install-and-setup)
    - [Install](#install)
    - [Setup](#setup)
    - [Usage](#usage)
- [Products and firmware versions](#products-and-firmware-version)
- [Create your own plugin](#create-your-own-plugin)
- [Limitations](#limitations)
    - [Security](#security)
    - [Certification](#certification)
- [License](#license)

## Build the ACAP

### Make and Docker

```sh
make dockerbuild
```

This will build the ACAP (server and all plugins) for both supported
architectures: armv7hf and aarch64.

### Docker

Alternatively, you can build the ACAP for each architecture using the following
commands without invoking `make`:

```sh
# 32-bit ARM, e.g. ARTPEC-6- and ARTPEC-7-based devices
docker build --build-arg ARCH=armv7hf --output type=local,dest=. .

# 64-bit ARM, e.g. ARTPEC-8-based devices
docker build --build-arg ARCH=aarch64 --output type=local,dest=. .
```

### Docker (development containers)

If you want to start developing your own version of the ACAP or to customize the
provided example, it is recommend to do it inside a Docker container to speed up
the development cycle by avoiding the recompilation of all the source code.

Start by building a Docker image with the needed libraries. The command will
differ depending on the target architecture (armv7hf or aarch64).

```sh
make dev-image-arm7hf
```

or

```sh
make dev-image-aarch64
```

The next step is to start a development container:

```sh
make devel-armv7hf
```

or

```sh
make devel-aarch64
```

The above commands will drop you in a shell in the container. To build the ACAP
within the container, use:

```sh
cd app
acap-build .
```

## Install and Setup

### Install

The ACAP (`.eap` file) can be uploaded to the Axis camera either via the "Apps"
section of the web GUI or, by using the command-line. The ACAP SDK includes a
shell script called `eap-install.sh` which can be used to perform various
actions on the `.eap` file like uploading, starting, stopping or removing the
application. For a brief usage description, run:

```sh
eap-install.sh --help
```

inside the running developer container. So, for example, to upload and start the
newly built OPC-UA server ACAP you can run:

```sh
eap-install.sh <camera hostname/ip> <username> <password> install
```

### Setup

The ACAP has two configurable parameters shown under the **Settings** section
(see the picture below):

- the logging level (default being info)

- the listening TCP port (default being 4840)

![OPC-UA Server Settings](assets/Parameters.png "OPC-UA Server ACAP Settings")

The parameters can also be listed and set by running these commands:

- to list the ACAP parameters with their current values:

```sh
curl --insecure --anyauth --user <username>:<password> \
    'https://<camera hostname/ip>/axis-cgi/param.cgi?action=list&group=axisopcua'
```

- to set a parameter value, for example to set the server port to **4841**:

```sh
curl --insecure --anyauth --user <username>:<password> \
    'https://<camera hostname/ip>/axis-cgi/param.cgi?action=update&axisopcua.port=4841'
```

- to set the log level to **Info**:

```sh
curl --insecure --anyauth --user <username>:<password> \
    'https://<camera hostname/ip>/axis-cgi/param.cgi?action=update&axisopcua.loglevel=1'
```

The ACAP supports five different log levels. Each level has a corresponding
number, as following:

- 0 - Debug
- 1 - Info
- 2 - Warning
- 3 - Error
- 4 - Fatal

> [!NOTE]
> If any parameter is modified, the ACAP requires a restart.

### Usage

To interact with the OPC-UA server an OPC-UA client is needed. A very useful
such tool is
[**UaExpert**](https://www.unified-automation.com/products/development-tools/uaexpert.html)
which can be used to explore OPC-UA servers. UaExpert enables users to perform
read/write operations on nodes, execute method calls, view OPC-UA events, alarms
and more.
See the pictures below on how to connect to the OPC-UA server, using UAExpert.

Click on the **Add Server** button shown in the picture below.

![UAExpertAdd](assets/UAExpert_AddServer.png)

Use custom discovery to find the OPC-UA Server.

![UAExpertCustomDiscovery](assets/UAExpert_Discovery.png)

After clicking on **Add Server**, under **Custom Discovery**, there will be a
text input to enter the URL/Endpoint for the server.

If the camera's IP is 192.168.0.90 and the OPC-UA server port is 4840 (the
default value), the server endpoint URL will be:

>opc.tcp://192.168.0.90:4840

UAExpert will list all the available security options for the server.

![UAExpertEndpoints](assets/UAExpert_Endpoints.png)

In this case there will only be one option: **None**. Select the security policy
and press **OK**. It will now appear in the menu on the left side. The server
will only allow anonymous authentication and therefore the other authentication
options are not available.

![UAExpertConnect](assets/UAExpert_Connect.png)

To connect to the server right click on the name and click **Connect** or click
on the **Connect Server** icon in the tool bar. If the connection is
established, the server will present its OPC-UA information as shown in the
picture below.

![UAExpert](assets/UAExpert_InfoModel.png)

There is one *object node* called **BasicDeviceInfo** with *property nodes*
presenting different information about the active camera. This is implemented in
the **plugins/bdi** module. The **plugins/hello_world** module is responsible
for creating a *variable node*, called **HelloWorldNode** with the string value
"Hello World!".

## Products and Firmware Version

The ACAP has been tested on the following products (with firmware version):

- AXIS Q1645 - 11.7.61
- AXIS F9111 - 12.2.58
- AXIS P3265-LV - 12.0.4
- AXIS Q1961-TE - 12.0.10

>[!NOTE]
> The ACAP may work on other Axis cameras and firmware versions beyond those
> listed, but compatibility is not guaranteed. Users are advised to test the
> ACAP on their specific device and firmware version before deploying it in
> production. For more information on compatibility, please visit the
> [**Axis Developer Documentation**](https://developer.axis.com/acap/axis-devices-and-compatibility/).

## Create your own plugin

The ACAP was designed to support easy addition of new plugins. The *plugins* are
essentially dynamically loaded libraries. They are implemented using GLib's
GModule APIs.

Create a new directory under *app/plugins/* and copy a Makefile from the example
plugin. Each plugin will need its own Makefile.

```text
axis-opcua
├── app
│   ├── include
│   │   ├── error.h
│   │   ├── log.h
│   │   └── plugin.h
│   ├── plugins
│   │   ├── bdi
│   │   │   ├── bdi_plugin.c
│   │   │   ├── bdi_plugin.h
│   │   │   └── Makefile
│   │   ├── hello_world
│   │   │   ├── hello_world_plugin.c
│   │   │   ├── hello_world_plugin.h
│   │   │   └── Makefile
│   │   └── your_plugin
│   │       ├── your_plugin.c
│   │       ├── your_plugin.h
│   │       └── Makefile
│   ├── opcua_parameter.c
│   ├── opcua_parameter.h
│   ├── opcua_open62541.c
│   ├── opcua_open62541.h
│   ├── opcua_server.c
│   ├── opcua_server.h
│   ├── plugin.c
│   ├── LICENSE
│   ├── Makefile
│   └── manifest.json
├── Dockerfile
└── README.md
```

To use ACAP SDK APIs add the required package(s) by editing the `PKGS` variable
in the Makefile. See
[**ACAP developer pages**](https://developer.axis.com/acap/) for available APIs.

```make
# Add packages to this variable in the Makefile
PKGS = gio-2.0 glib-2.0 gmodule-2.0
```

The name of the dynamically loadable library (plugin) will be derived from the
directory name under *plugins/*. For example, if the chosen directory name was
**`gas_sensor`**, the resulting name of the file will be
`libopcua_gas_sensor.so` and will be added to the `app/lib` folder which is
going to be part of the ACAP package file (`.eap`).

When the ACAP is started on the Axis camera, it will automatically attempt to
load all the modules present under its `lib` directory.

>[!NOTE]
>To configure which plugins to load, simply add or remove subdirectories within
>the `plugins` directory.

**Requirements:**
The plugins are implemented using GModule APIs. All plugins must implement the
following set of functions which are called from the main server application:

```c
/*******************************************************************************
 * @server - The opcua server
 * @logger - The logger for open62541 log API (see log.h)
 * @params - User parameters (can be NULL)
 * @err - Error handling, use SET_ERROR() or g_prefix_error() (see error.h)
 ******************************************************************************/
gboolean
opc_ua_create(UA_Server *server, UA_Logger *logger, gpointer params, GError **err)

void
opc_ua_destroy(void)

gchar *
opc_ua_get_name(void)
```

These functions need to be defined for the plugin to be loaded successfully by
the server application. During start up the server ACAP will try to load all the
existing plugins and will call *`opc_ua_create()`* for each one of them.

Similarly, when the ACAP is stopped, *`opc_ua_destroy()`* will be called for
every plugin that was loaded.

Some features in the Open62541 library are optional and can be enabled using
specific build options. A list of available build flags can be found
[**here**](https://www.open62541.org/doc/master/building.html#build-options).

If a feature is missing when developing a plugin it can be added by enabling the
required flag. This is done in the `Dockerfile`, section `Build open65421`.
In the *cmake* command the flags can be enabled or disabled as needed:

```docker
#------------------------------
# Build open65421
#------------------------------
...
cmake -DCMAKE_INSTALL_PREFIX=${SDKTARGETSYSROOT}/usr \
    -DBUILD_SHARED_LIBS=OFF \
    -DUA_LOGLEVEL=200 \
    -DUA_BUILD_EXAMPLES=OFF ..
```

By default, only basic functionality is available. Additional flags can be added
to enable more features, but this may increase the ACAP's memory consumption.

>[!NOTE]
>There are two Dockerfiles, one for building the whole ACAP and one for starting
>the development container. Make sure to modify the one that you are using.

## Limitations

In this version there are some limitations. Some of the known ones will be
explained here.

### Security

Note that this version does not support encryption policies or certificate
exchange. Any OPC-UA client on the same network can connect to the server,
as authentication is not supported. Neither is encryption of data and messages.
If this is required please contact us for further discussions.

### Certification

The ACAP is not certified by the OPC Foundation. Although it uses the
[**open62541**](https://www.open62541.org/) library, which is certified for
version 1.4.0-re, this certification does not extend to the ACAP itself. In
other words, the ACAP has not undergone the OPC Foundation's certification
process, and its compliance with OPC-UA standards is not guaranteed. Read more
about the open62541 certification
[**here**](https://opcfoundation.org/products/view/open62541/).

## License

**[Apache License 2.0](LICENSE)**
