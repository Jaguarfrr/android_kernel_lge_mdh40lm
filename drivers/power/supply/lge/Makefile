# SPDX-License-Identifier: GPL-2.0
#
# Makefile for LG Electronics Power Management devices.
#
subdir-ccflags-y += -I$(srctree)/drivers/power/supply

charger_controller-y := charger_controller_core.o \
			charger_controller_vote.o \
			charger_controller_algo.o \
			charger_controller_cmd.o  \
			charger_controller_info.o
charger_controller-y += charger_controller_atcmd.o
charger_controller-y += charger_controller_ccd.o
charger_controller-y += charger_controller_game.o
charger_controller-y += charger_controller_misc.o
charger_controller-y += charger_controller_otp.o
charger_controller-y += charger_controller_spec.o
charger_controller-y += charger_controller_thermal.o

pseudo_power_supply-y := lge_pseudo_power_supply.o
pseudo_power_supply-$(CONFIG_LGE_PM_PSEUDO_BATTERY) += lge_pseudo_batt.o
pseudo_power_supply-$(CONFIG_LGE_PM_PSEUDO_HVDCP) += lge_pseudo_hvdcp.o
obj-$(CONFIG_LGE_PM_BATTERY_CYCLE) += battery_cycle.o
obj-$(CONFIG_LGE_PM_BATTERY_ID)		+= lge_battery_id.o
obj-$(CONFIG_LGE_PM_CHARGER_CONTROLLER)	+= charger_controller.o
obj-$(CONFIG_LGE_PM_POWER_SYSFS)	+= lge_power_sysfs.o
obj-$(CONFIG_LGE_PM_PSEUDO_POWER_SUPPLY) += pseudo_power_supply.o
obj-$(CONFIG_LGE_PM_QNOVO_QNS) += qns_system.o
