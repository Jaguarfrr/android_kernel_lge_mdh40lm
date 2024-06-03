#
# Copyright (C) 2019 Wingtech Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

subdir-ccflags-y += -DCONFIG_FSM_I2C -DCONFIG_FSM_FS1603
ifneq ($(TARGET_BUILD_VARIANT),user)
subdir-ccflags-y += -DFSM_DEBUG
endif
obj-$(CONFIG_SND_SOC_FS16XX) += fsm_i2c.o fs1603.o fsm_core.o fsm_firmware.o fsm_sysfs.o fsm_codec.o
