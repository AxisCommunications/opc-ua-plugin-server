PROG = opcpluginserver
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
DEPS = $(patsubst %.c,%.d,$(SRCS))

PKGS = gio-2.0 glib-2.0 gio-unix-2.0 gmodule-2.0 libcurl axparameter
CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))

LIB_OPEN62541 = $(SDKTARGETSYSROOT)/usr/lib/libopen62541.a
LIBS += $(LIB_OPEN62541)

# 'include' directory
INCLUDE = include
# 'plugins' directory: opcua modules organized in subdirs under 'plugins'
PLUGINS = plugins

CFLAGS += -DAPPNAME=\"$(PROG)\"
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

.PHONY: plugins

all:	$(PROG) .PHONY
	$(STRIP) $(PROG)

-include $(DEPS)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $^ $(LIBS) $(LDLIBS) -o $@

plugins:
	for plugin in $(PLUGINS)/*; do \
		$(MAKE) -C $$plugin; \
	done

clean:
	for plugin in $(PLUGINS)/*; do \
		$(MAKE) -C $$plugin clean; \
	done
	rm -f $(DEPS) $(PROG) *.o core *.eap *LICENSE.txt *.conf *.orig *.old tmp*
	rm -rf lib
