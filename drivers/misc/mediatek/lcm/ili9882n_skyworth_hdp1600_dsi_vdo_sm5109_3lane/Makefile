#
# Makefile for misc devices that really don't fit anywhere else.
#
#include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-y += ili9882n_skyworth_hdp1600_dsi_vdo_sm5109_3lane.o

ccflags-$(CONFIG_MTK_LCM) += -I$(srctree)/drivers/misc/mediatek/lcm/inc

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(CMDQ_PLATFORM)/
