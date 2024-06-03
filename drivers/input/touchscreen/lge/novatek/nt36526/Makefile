#
# Makefile for the Novatek NT36xxx touchscreen driver.
#

# Each configuration option enables a list of files.

# Touch Core Driver Common Header Code..!!
ccflags-y += -I$(srctree)/drivers/input/touchscreen/lge/

obj-$(CONFIG_LGE_TOUCH_NOVATEK_NT36526) += touch_nt36xxx.o touch_nt36xxx_fw_update.o touch_nt36xxx_ext_proc.o touch_nt36xxx_mp_ctrlram.o
