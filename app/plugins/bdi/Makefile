# The name of the plugin module (shared object) is built up by concatenating:
#  * the prefix: 'libopcua_'
#  * the directory/plugin name under 'plugins'
#  * the suffix: '.so'
TARGET_LIB = libopcua_$(notdir $(CURDIR)).so
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
DEPS = $(patsubst %.c,%.d,$(SRCS))

PKGS = gio-2.0 glib-2.0 gmodule-2.0 libcurl jansson
CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))

LIB_OPEN62541 = $(SDKTARGETSYSROOT)/usr/lib/libopen62541.a
LIBS += $(LIB_OPEN62541)

# app/include
INCLUDE = ../../include

# All the plugin modules are to be placed in 'app/lib'. 'eap-create.sh' will
# automatically pick up the 'lib' folder and add it to the final .eap file.
TARGET_LIB_DIR = ../../lib

CFLAGS += -I.
CFLAGS += $(addprefix -I, $(INCLUDE))

CFLAGS += -Wall \
	-Wextra \
	-Wformat=2 \
	-Wpointer-arith \
	-Wbad-function-cast \
	-Wstrict-prototypes \
	-Wmissing-prototypes \
	-Winline \
	-Wdisabled-optimization \
	-Wfloat-equal \
	-W \
	-Werror \
	-Wno-maybe-uninitialized

# preprocessor flags to generate Makefile dependencies
CPPFLAGS = -MMD -MP

CFLAGS_MODULES = $(CFLAGS) -fPIC

all: $(TARGET_LIB_DIR)/$(TARGET_LIB)

$(TARGET_LIB_DIR)/$(TARGET_LIB): $(TARGET_LIB)
	mkdir -p $(TARGET_LIB_DIR)
	cp $(TARGET_LIB) $(TARGET_LIB_DIR)

$(TARGET_LIB): $(OBJS)
	$(CC) $(CFLAGS_MODULES) $(CPPFLAGS) $(LDFLAGS) -shared $^ $(LIBS) $(LDLIBS) \
		-o $@

-include $(DEPS)

clean:
	rm -f $(DEPS) *.o core *.so $(TARGET_LIB_DIR)/$(TARGET_LIB)
