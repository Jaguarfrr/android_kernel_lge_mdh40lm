#
# Makefile for the touchscreen drivers.
#

# Each configuration option enables a list of files.

# Touch Core Driver Common Header Code..!!
ccflags-y += -I$(srctree)/drivers/input/touchscreen/lge/

obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_TD4320)		+= touch_td4320.o
obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_TD4320)		+= touch_td4320_reflash.o
obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_TD4320)		+= touch_td4320_testing.o
obj-$(CONFIG_LGE_TOUCH_SYNAPTICS_TD4320)		+= touch_td4320_device.o
