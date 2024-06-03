// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "imgsensor_sensor.h"

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/string.h>

#include "kd_camera_typedef.h"
#include "kd_camera_feature.h"


#include "imgsensor_hw.h"
//+Bug 525349, wanglei6.wt,MODIFY, 2019.12.28,S96769BA1 camera bringup
char *imgsensor_sensor_idx_name[IMGSENSOR_SENSOR_IDX_MAX_NUM] = {
	IMGSENSOR_SENSOR_IDX_NAME_MAIN,
	IMGSENSOR_SENSOR_IDX_NAME_SUB,
	IMGSENSOR_SENSOR_IDX_NAME_MAIN2,
	IMGSENSOR_SENSOR_IDX_NAME_SUB2,
	IMGSENSOR_SENSOR_IDX_NAME_MAIN3,
};

enum IMGSENSOR_RETURN imgsensor_hw_release_all(struct IMGSENSOR_HW *phw)
{
	int i;

	for (i = 0; i < IMGSENSOR_HW_ID_MAX_NUM; i++) {
		if (phw->pdev[i]->release != NULL)
			(phw->pdev[i]->release)(phw->pdev[i]->pinstance);
	}
	return IMGSENSOR_RETURN_SUCCESS;
}
enum IMGSENSOR_RETURN imgsensor_hw_init(struct IMGSENSOR_HW *phw)
{
	struct IMGSENSOR_HW_SENSOR_POWER      *psensor_pwr;
	struct IMGSENSOR_HW_CFG               *pcust_pwr_cfg;
	struct IMGSENSOR_HW_CUSTOM_POWER_INFO *ppwr_info;
	int i, j;

	for (i = 0; i < IMGSENSOR_HW_ID_MAX_NUM; i++) {
		if (hw_open[i] != NULL)
			(hw_open[i])(&phw->pdev[i]);

		if (phw->pdev[i]->init != NULL)
			(phw->pdev[i]->init)(phw->pdev[i]->pinstance);
	}

	for (i = 0; i < IMGSENSOR_SENSOR_IDX_MAX_NUM; i++) {
		psensor_pwr = &phw->sensor_pwr[i];

		pcust_pwr_cfg = imgsensor_custom_config;
		while (pcust_pwr_cfg->sensor_idx != i &&
		       pcust_pwr_cfg->sensor_idx != IMGSENSOR_SENSOR_IDX_NONE)
			pcust_pwr_cfg++;

		if (pcust_pwr_cfg->sensor_idx == IMGSENSOR_SENSOR_IDX_NONE)
			continue;

		ppwr_info = pcust_pwr_cfg->pwr_info;
		while (ppwr_info->pin != IMGSENSOR_HW_PIN_NONE) {
			for (j = 0; j < IMGSENSOR_HW_ID_MAX_NUM; j++)
				if (ppwr_info->id == phw->pdev[j]->id)
					break;

			psensor_pwr->id[ppwr_info->pin] = j;
			ppwr_info++;
		}
	}

	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN imgsensor_hw_power_sequence(
	struct IMGSENSOR_HW             *phw,
	enum   IMGSENSOR_SENSOR_IDX      sensor_idx,
	enum   IMGSENSOR_HW_POWER_STATUS pwr_status,
	struct IMGSENSOR_HW_POWER_SEQ   *ppower_sequence,
	char *pcurr_idx)
{
	struct IMGSENSOR_HW_SENSOR_POWER *psensor_pwr =
	    &phw->sensor_pwr[sensor_idx];

	struct IMGSENSOR_HW_POWER_SEQ    *ppwr_seq = ppower_sequence;
	struct IMGSENSOR_HW_POWER_INFO   *ppwr_info;
	struct IMGSENSOR_HW_DEVICE       *pdev;
	int                               pin_cnt = 0;

	while (ppwr_seq < ppower_sequence + IMGSENSOR_HW_SENSOR_MAX_NUM &&
		ppwr_seq->name != NULL) {
		if (!strcmp(ppwr_seq->name, PLATFORM_POWER_SEQ_NAME)) {
			if (sensor_idx == ppwr_seq->_idx)
				break;
		} else {
			if (!strcmp(ppwr_seq->name, pcurr_idx))
				break;
		}
		ppwr_seq++;
	}

	if (ppwr_seq->name == NULL)
		return IMGSENSOR_RETURN_ERROR;

	ppwr_info = ppwr_seq->pwr_info;

	while (ppwr_info->pin != IMGSENSOR_HW_PIN_NONE &&
		ppwr_info < ppwr_seq->pwr_info + IMGSENSOR_HW_POWER_INFO_MAX) {

		if (pwr_status == IMGSENSOR_HW_POWER_STATUS_ON &&
		   ppwr_info->pin != IMGSENSOR_HW_PIN_UNDEF) {
			pdev = phw->pdev[psensor_pwr->id[ppwr_info->pin]];
		/*pr_debug(
		 *  "sensor_idx = %d, pin=%d, pin_state_on=%d, hw_id =%d\n",
		 *  sensor_idx,
		 *  ppwr_info->pin,
		 *  ppwr_info->pin_state_on,
		 * psensor_pwr->id[ppwr_info->pin]);
		 */

			if (pdev->set != NULL)
				pdev->set(
				    pdev->pinstance,
				    sensor_idx,
				    ppwr_info->pin,
				    ppwr_info->pin_state_on);

			mdelay(ppwr_info->pin_on_delay);
		}

		ppwr_info++;
		pin_cnt++;
	}

	if (pwr_status == IMGSENSOR_HW_POWER_STATUS_OFF) {
		if(strcmp("gc2375_mipi_raw", pcurr_idx) == 0 || strcmp("gc2375h_mipi_raw", pcurr_idx) == 0)
			{
				pdev = phw->pdev[psensor_pwr->id[IMGSENSOR_HW_PIN_PDN]];
				if (pdev->set != NULL)
						pdev->set(
							pdev->pinstance,
							sensor_idx,
							IMGSENSOR_HW_PIN_PDN,
							Vol_High);
				mdelay(5);
				while (pin_cnt) {
					ppwr_info--;
					pin_cnt--;

					if (ppwr_info->pin != IMGSENSOR_HW_PIN_UNDEF) {
						pdev =
						    phw->pdev[psensor_pwr->id[ppwr_info->pin]];
						mdelay(ppwr_info->pin_on_delay);

						if (pdev->set != NULL)
						{
							if(ppwr_info->pin == IMGSENSOR_HW_PIN_PDN)
							{
								continue;
							}
							else
							{
								pdev->set(
									pdev->pinstance,
									sensor_idx,
									ppwr_info->pin,
									ppwr_info->pin_state_off);
							}
						}
					}
				}
				mdelay(5);
				pdev = phw->pdev[psensor_pwr->id[IMGSENSOR_HW_PIN_PDN]];
				if (pdev->set != NULL)
						pdev->set(
							pdev->pinstance,
							sensor_idx,
							IMGSENSOR_HW_PIN_PDN,
							Vol_Low);
			}
		else
			{
				while (pin_cnt) {
					ppwr_info--;
					pin_cnt--;

					if (ppwr_info->pin != IMGSENSOR_HW_PIN_UNDEF) {
						pdev =
						    phw->pdev[psensor_pwr->id[ppwr_info->pin]];
						mdelay(ppwr_info->pin_on_delay);

						if (pdev->set != NULL)
								pdev->set(
									pdev->pinstance,
									sensor_idx,
									ppwr_info->pin,
									ppwr_info->pin_state_off);
						}
					}
			}
	}

	/* wait for power stable */
	if (pwr_status == IMGSENSOR_HW_POWER_STATUS_ON)
		mdelay(5);
	return IMGSENSOR_RETURN_SUCCESS;
}

enum IMGSENSOR_RETURN imgsensor_hw_power(
	struct IMGSENSOR_HW     *phw,
	struct IMGSENSOR_SENSOR *psensor,
	char *curr_sensor_name,
	enum IMGSENSOR_HW_POWER_STATUS pwr_status)
{
	enum IMGSENSOR_SENSOR_IDX sensor_idx = psensor->inst.sensor_idx;


#if defined(CONFIG_IMGSENSOR_MAIN)  || \
		defined(CONFIG_IMGSENSOR_SUB)   || \
		defined(CONFIG_IMGSENSOR_MAIN2) || \
		defined(CONFIG_IMGSENSOR_SUB2) || \
		defined(CONFIG_IMGSENSOR_MAIN3)
	char *pcustomize_sensor = NULL;
	if (curr_sensor_name == NULL)
		return IMGSENSOR_RETURN_ERROR;

	switch (sensor_idx) {
#ifdef CONFIG_IMGSENSOR_MAIN
	case IMGSENSOR_SENSOR_IDX_MAIN:
		pcustomize_sensor = IMGSENSOR_STRINGIZE(CONFIG_IMGSENSOR_MAIN);
		break;
#endif
#ifdef CONFIG_IMGSENSOR_SUB
	case IMGSENSOR_SENSOR_IDX_SUB:
		pcustomize_sensor = IMGSENSOR_STRINGIZE(CONFIG_IMGSENSOR_SUB);
		break;
#endif
#ifdef CONFIG_IMGSENSOR_MAIN2
	case IMGSENSOR_SENSOR_IDX_MAIN2:
		pcustomize_sensor = IMGSENSOR_STRINGIZE(CONFIG_IMGSENSOR_MAIN2);
		break;
#endif
#ifdef CONFIG_IMGSENSOR_SUB2
	case IMGSENSOR_SENSOR_IDX_SUB2:
		pcustomize_sensor = IMGSENSOR_STRINGIZE(CONFIG_IMGSENSOR_SUB2);
		break;
#endif
#ifdef CONFIG_IMGSENSOR_MAIN3
	case IMGSENSOR_SENSOR_IDX_MAIN3:
		pcustomize_sensor = IMGSENSOR_STRINGIZE(CONFIG_IMGSENSOR_MAIN3);
		break;
#endif
	default:
		break;
	}


	if (pcustomize_sensor != NULL &&
		strlen(pcustomize_sensor) > 2 &&
		!strstr(pcustomize_sensor, curr_sensor_name))
		return IMGSENSOR_RETURN_ERROR;
#else
	if (curr_sensor_name == NULL)
		return IMGSENSOR_RETURN_ERROR;
#endif

	pr_info(
	    "sensor_idx %d, power %d curr_sensor_name %s\n",
	    sensor_idx,
	    pwr_status,
	    curr_sensor_name);

	imgsensor_hw_power_sequence(
	    phw,
	    sensor_idx,
	    pwr_status,
	    platform_power_sequence,
	    imgsensor_sensor_idx_name[sensor_idx]);
        //+Bug 525349, wanglei6.wt,MODIFY, 2019.12.28,S96769BA1 camera bringup

	imgsensor_hw_power_sequence(
	    phw,
	    sensor_idx,
	    pwr_status,
	    sensor_power_sequence,
	    curr_sensor_name);

	return IMGSENSOR_RETURN_SUCCESS;
}

