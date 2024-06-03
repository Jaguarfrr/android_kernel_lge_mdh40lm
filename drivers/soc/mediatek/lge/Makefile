MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc

obj-$(CONFIG_MACH_LGE)				+= devices_lge.o
obj-$(CONFIG_LGE_BOOTLOADER_LOG)	+= lge_bootloader_log.o
obj-$(CONFIG_LGE_BOOT_MODE) += lge_boot_mode.o
obj-$(CONFIG_LGE_HANDLE_PANIC)		+= lge_handle_panic.o
obj-$(CONFIG_PRE_SELF_DIAGNOSIS) += lge_pre_selfd.o
obj-$(CONFIG_LGE_EARJACK_DEBUGGER)	+= lge_earjack_debugger.o
obj-$(CONFIG_LGE_QFPROM_INTERFACE) += lge_efuse_access.o
obj-$(CONFIG_LGE_PM_AT_POWEROFF) += lge_atd_mid.o
obj-$(CONFIG_LGE_MME_TEST) += lge_mme_test.o
obj-$(CONFIG_LGE_MME_DRV) += lge_mme_drv.o
obj-$(CONFIG_LGE_USB_DIAG_LOCK)		+= lge_diag_lock.o
obj-$(CONFIG_LGE_USB_ID) += lge_usb_id.o
obj-$(CONFIG_LGE_QSDL_SUPPORT) += lge_qsdl.o
obj-$(CONFIG_LGE_PCB_INDICATOR)		+= lge_pcb_indicator.o

ifeq ($(TARGET_BUILD_VARIANT),user)
ccflags-y += -DUSER_BUILD
endif
