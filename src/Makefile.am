
plugindir = $(libdir)/mediastreamer/plugins

plugin_LTLIBRARIES = libmsg7231.la

libmsg7231_la_CFLAGS = $(MEDIASTREAMER_CFLAGS)

libmsg7231_la_LIBADD = $(MEDIASTREAMER_LIBS) -lmsacm32

libmsg7231_la_LDFLAGS = -module -no-undefined

libmsg7231_la_SOURCES = msg7231.c
