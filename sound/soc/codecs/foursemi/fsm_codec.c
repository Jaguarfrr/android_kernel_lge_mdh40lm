/**
 * Copyright (C) 2018 Fourier Semiconductor Inc. All rights reserved.
 * 2018-10-22 File created.
 */

#include "fsm_public.h"
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <linux/miscdevice.h>
#include <sound/soc.h>
#include <linux/version.h>

// 190906 add LGE rcv mode register set S
#include "fs1603.h"
int rcv_boost;
int rcv_volume;
// 190906 add LGE rcv mode register set E

// 1901021 add LGE channel swap S
int channel_swap;
// 1901021 add LGE channel swap E

/* Supported rates and data formats */
#define FSM_RATES SNDRV_PCM_RATE_8000_96000
#define FSM_FORMATS    (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static const unsigned int fsm_rates[] = { 8000, 16000, 32000, 44100, 48000, 88200, 96000 };
static const struct snd_pcm_hw_constraint_list fsm_constraints = {
    .list = fsm_rates,
    .count = ARRAY_SIZE(fsm_rates),
};


int fsm_get_scene(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    int scene = 0;

    scene = fsm_i2c_event(FSM_EVENT_GET_SCENE, 0);
    ucontrol->value.integer.value[0] = scene;

    return 0;
}
#if 0
static int fsm_set_scene(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0)
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
#endif
	fsm_pdata_t *pdata = snd_soc_codec_get_drvdata(codec);
	fsm_dev_t *fsm_dev = pdata->fsm_dev;
	int next_scene = ucontrol->value.integer.value[0];

    pr_info("%s next scene: %d\n", __func__, next_scene);
	pr_info("%s current scene: %d\n", __func__, fsm_dev->cur_scene);

	if(next_scene != fsm_dev->cur_scene){
		pr_info("force to set preset %d\n", next_scene);
	fsm_i2c_event(FSM_EVENT_SET_SCENE, next_scene);
	}
    return 0;
}
#endif
int fsm_get_volume(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    int volume = 0;

    ucontrol->value.integer.value[0] = volume;
    pr_info("%s volume: %x\n", __func__, volume);

    return 0;
}

int fsm_set_volume(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    int ret = 0, volume = 0x8f;//TODO

    //volume = ucontrol->value.integer.value[0];
    pr_info("%s volume: %d\n", __func__, volume);
    fsm_i2c_event(FSM_EVENT_SET_VOLUME, volume);

    return ret;
}

int fsm_get_stop(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    int stop = 0;

    ucontrol->value.integer.value[0] = stop;
    pr_info("%s stop: %d\n", __func__, stop);

    return 0;
}

int fsm_set_stop(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    int ret = 0, stop = 0;

    stop = ucontrol->value.integer.value[0];
    pr_info("%s stop: %x\n", __func__, stop);

    return ret;
}

int fsm_get_bypass(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    pr_info("enter\n");
    return 0;
}


int fsm_dsp_bypass(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    int ret;
    int bypass;

    pr_info("enter\n");
    bypass = ucontrol->value.integer.value[0];

    ret = fsm_i2c_cmd(FSM_CMD_SET_BYPASS, bypass);

    if(ret == 0) {
        pr_info("%s %s tinymix DSP Success, ret: %d\n", __func__, ((bypass == 1) ? "Bypass":"Unbypass"), ret);
    } else {
        pr_info("%s %s tinymix DSP Failed, ret: %d\n", __func__, ((bypass == 1) ? "Bypass":"Unbypass"), ret);
    }

    pr_info("exit\n");
    return ret;
}

static int fsm_get_preset_mode(struct snd_kcontrol *kcontrol,
        struct snd_ctl_elem_value *ucontrol)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0)
    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
#else
#ifdef KERNEL_VER_OVER_4_19_1
    struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
#else
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
#endif
#endif

#ifdef KERNEL_VER_OVER_4_19_1
    fsm_pdata_t *pdata = snd_soc_component_get_drvdata(component);
#else
    fsm_pdata_t *pdata = snd_soc_codec_get_drvdata(codec);
#endif
    fsm_dev_t *fsm_dev = &pdata->fsm_dev;

    pr_info("device address = 0x%02x.", fsm_dev->i2c_addr);
    pr_info("current preset = %d.", fsm_dev->cur_scene);

    ucontrol->value.integer.value[0] = fsm_dev->cur_scene;
    snd_soc_put_volsw(kcontrol, ucontrol);

    pr_info("exit .");
    return 0;
}
static int fsm_put_preset_mode(struct snd_kcontrol *kcontrol,
        struct snd_ctl_elem_value *ucontrol)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0)
    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
#else
#ifdef KERNEL_VER_OVER_4_19_1
    struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
#else
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
#endif
#endif

#ifdef KERNEL_VER_OVER_4_19_1
    fsm_pdata_t *pdata = snd_soc_component_get_drvdata(component);
#else
    fsm_pdata_t *pdata = snd_soc_codec_get_drvdata(codec);
#endif
    fsm_dev_t *fsm_dev = &pdata->fsm_dev;
    fsm_config_t *config = fsm_get_config();
    int preset = ucontrol->value.integer.value[0];

    if(pdata == NULL) {
        pr_info("Bad parameter.\n");
        return 0;
    }

    pr_info("0x%02x: cur: %d, next: 0x%08x\n", fsm_dev->i2c_addr, \
                fsm_dev->cur_scene, (1 << preset));

    if(preset < PRESET_MUSIC || preset > PRESET_RINGTONE){
        pr_info("Bad preset parameter. do nothing.\n");
        goto Exit;
    }

    config->next_scene = (1 << preset);

Exit:
    return 0;
}

static int fsm_get_mute_state (struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
	pr_info("enter.\n");
	return 0;
}

static int fsm_mute_device (struct snd_kcontrol *kcontrol,
                struct snd_ctl_elem_value *ucontrol)
{
/*#if LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0)
    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
#else
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
#endif
    int mute, preset;
    //int ret, id = -1,;
    fsm_pdata_t *pdata = snd_soc_codec_get_drvdata(codec);
    fsm_dev_t *fsm_dev = &pdata->fsm_dev;

    //mutex_lock(&lr_lock);
    mute = ucontrol->value.integer.value[0];

    if(mute)
        fsm_dev->force_mute = true;
    else
        fsm_dev->force_mute = false;

    pr_info("control %s %s device.\n" ,((fsm_dev->force_mute == true)?"MUTE":"UNMUTE"), \
                                        (fsm_dev->i2c_addr = 0x34)?"left":"right");

    if (!g_mute_status) {
        if (!strncmp("left", fsm_dev->fs16xx_dev, 4)) {
            id = FS16XX_DEV_INDEX_L;
        } else if(!strncmp("right", fsm_dev->fs16xx_dev, 5)) {
            id = FS16XX_DEV_INDEX_R;
        } else {
            PRINT_ERROR("%s: invalid device name!", __func__);
            mutex_unlock(&lr_lock);
        return -1;
        }
    if (!fsm_dev->force_mute) {
            fs16xx_power(codec, 1);
        Sleep(15);
    }
        if(preset == PRESET_VOICE_EARPIECE){
            fs16xx_set_preset(id, 1);
            fs16xx_set_mute(codec, FS_UMUTE_EARPIECE);
            fsm_dev->info.preset_done = PRESET_VOICE_EARPIECE;
        }else if(preset == PRESET_VOICE){
            fs16xx_set_preset(id, 1);
            fs16xx_set_mute(codec, FS_UMUTE);
            fsm_dev->info.preset_done = PRESET_VOICE;
        }else if(preset == PRESET_MUSIC){
            fs16xx_set_preset(id, 0);
            fs16xx_set_mute(codec, FS_UMUTE);
            fsm_dev->info.preset_done = PRESET_MUSIC;
        }else{
            DEBUGPRINT("%s: error invalid preset mode =%d.", __func__, preset);
            mutex_unlock(&lr_lock);
        return 0;
        }
    }
    pr_info("force mute = %d\n", fsm_dev->force_mute);
    snd_soc_put_volsw(kcontrol, ucontrol);*/
    //mutex_unlock(&lr_lock);

    pr_info("exit.\n");

    return 0;
}

static int fsm_dummy_fun(struct snd_kcontrol *kcontrol,
                struct snd_ctl_elem_value *ucontrol)
{
    pr_info("enter.\n");
    return 0;
}

// 190906 add LGE rcv mode register set S
int fsm_get_rcv_volume(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    pr_info("%x\n", rcv_volume);
    return rcv_volume;
}

int fsm_set_rcv_volume(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    rcv_volume = ucontrol->value.integer.value[0];
    pr_info("%x\n", rcv_volume);

    return 0;
}

int fsm_get_rcv_boost(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    pr_info("%x\n", rcv_boost);
    return rcv_boost;
}

int fsm_set_rcv_boost(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    rcv_boost = ucontrol->value.integer.value[0];
    pr_info("%x\n", rcv_boost);

    return 0;
}

int fsm_set_rcv_boost_force(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    int ret = 0;
    int value = ucontrol->value.integer.value[0];

    if (rcv_boost != value) {
        ret = fsm_set_registers(FSM_DEV_RIGHT, FS1603_BSTCTRL, (uint16_t)value);
        pr_info("%x, ret = %d\n", value, ret);
        rcv_boost = value;
    } else {
        pr_info("same value, skip.\n");
    }

    return ret;
}
// 190906 add LGE rcv mode register set E

// 191021 add LGE channel swap S
int fsm_get_channel_swap(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    pr_info("%d\n", channel_swap);
    return 0;
}

int fsm_set_channel_swap(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    int ret = 0;

    if (channel_swap != ucontrol->value.integer.value[0]) {
        channel_swap = ucontrol->value.integer.value[0];
        pr_info("set channel swap %d\n", channel_swap);
        ret = fsm_channel_swap(channel_swap);
    } else {
        pr_info("skip set channel swap, same direction\n");
    }

    return ret;
}
// 191021 add LGE channel swap E
// 191107 add LGE force calibration related func S
int fsm_get_calib_result(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    int impedance = 0;
    int status = 0;
    int ret1 = 0;
    int ret2 = 0;

    impedance = fsm_get_calib_impedance(FSM_DEV_LEFT);
    status = fsm_get_calib_status(FSM_DEV_LEFT);

    if (status && (6144 <= impedance) && (impedance <= 8704))
        ret1 = 1;
    else
        ret1 = 0;
    pr_info("%x, impedance : %d, status : %d, ret1(bottom) : %d\n", FSM_DEV_LEFT, impedance, status, ret1);

    impedance = fsm_get_calib_impedance(FSM_DEV_RIGHT);
    status = fsm_get_calib_status(FSM_DEV_RIGHT);

    if (status && (6144 <= impedance) && (impedance <= 7680))
        ret2 = 1;
    else
        ret2 = 0;
    pr_info("%x, impedance : %d, status : %d, ret2(top) : %d\n", FSM_DEV_RIGHT, impedance, status, ret2);

    ucontrol->value.integer.value[0] = ret1&ret2;
    return 0;
}

int fsm_set_calib_result(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    pr_info("enter\n");
    return 0;
}

int fsm_get_force_spk_calib(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    pr_info("enter\n");
    return 0;
}

int fsm_set_force_spk_calib(struct snd_kcontrol *kcontrol,
            struct snd_ctl_elem_value *ucontrol)
{
    int impedance = 0;
    int status = 0;

    pr_info("enter\n");
    fsm_delay_ms(500);
    fsm_calibrate(1);

    impedance = fsm_get_calib_impedance(FSM_DEV_LEFT);
    status = fsm_get_calib_status(FSM_DEV_LEFT);
    pr_info("%x, impedance : %d, status : %d\n", FSM_DEV_LEFT, impedance, status);

    impedance = fsm_get_calib_impedance(FSM_DEV_RIGHT);
    status = fsm_get_calib_status(FSM_DEV_RIGHT);
    pr_info("%x, impedance : %d, status : %d\n", FSM_DEV_RIGHT, impedance, status);

    pr_info("exit\n");
    return 0;
}
// 191107 add LGE force calibration related func E

static const struct snd_kcontrol_new fsm_snd_controls[] =
{
    //SOC_SINGLE_EXT("FSM Left Scene", 0, 0, 2, 0, fsm_get_scene, fsm_set_scene),
    SOC_SINGLE_EXT("FSM_Volume", 0, 0, 2, 0, fsm_get_volume, fsm_set_volume),
    SOC_SINGLE_EXT("FSM_Stop", 0, 0, 2, 0, fsm_get_stop, fsm_set_stop),
    SOC_SINGLE_EXT("FSM_Dsp_Bypass", 0, 0, 2, 0, fsm_get_bypass, fsm_dsp_bypass),
    SOC_SINGLE_EXT("FSM_Preset_Mode", 0, 0, 7, 0, fsm_get_preset_mode, fsm_put_preset_mode),
    SOC_SINGLE_EXT("FSM_Device_Mute", 0xAE, 8, 1, 0, fsm_get_mute_state, fsm_mute_device),
    SOC_SINGLE_EXT("FSM_AEC_Output", 0x04, 11, 1, 0, snd_soc_get_volsw, fsm_dummy_fun),
// 190906 add LGE rcv mode register set S
    SOC_SINGLE_EXT("FSM_RCV_Volume", 0, 0, 2, 0, fsm_get_rcv_volume, fsm_set_rcv_volume),
    SOC_SINGLE_EXT("FSM_RCV_Boost", 0, 0, 2, 0, fsm_get_rcv_boost, fsm_set_rcv_boost),
    SOC_SINGLE_EXT("FSM_RCV_Boost_SET", 0, 0, 2, 0, fsm_get_rcv_boost, fsm_set_rcv_boost_force),
// 190906 add LGE rcv mode register set E
// 191021 add LGE channel swap S
    SOC_SINGLE_EXT("FSM_CHANNEL_SWAP", 0, 0, 2, 0, fsm_get_channel_swap, fsm_set_channel_swap),
// 191021 add LGE channel swap E
// 191107 add LGE force calibration related func S
    SOC_SINGLE_EXT("FSM_GET_CALIB_STATUS", 0, 0, 2, 0, fsm_get_calib_result, fsm_set_calib_result),
    SOC_SINGLE_EXT("FSM_FORCE_SPK_CALIB", 0, 0, 2, 0, fsm_get_force_spk_calib, fsm_set_force_spk_calib),
// 191107 add LGE force calibration related func E
};

static int fsm_add_widgets(fsm_pdata_t *pdata)
{
#ifdef KERNEL_VER_OVER_4_19_1
    snd_soc_add_component_controls(pdata->component,
            fsm_snd_controls,
            ARRAY_SIZE(fsm_snd_controls));
#else
    snd_soc_add_codec_controls(pdata->codec, fsm_snd_controls,
                ARRAY_SIZE(fsm_snd_controls));
#endif
    return 0;
}

#if 0    //LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0)
static struct snd_soc_codec *snd_soc_kcontrol_codec(struct snd_kcontrol *kcontrol)
{
    return snd_kcontrol_chip(kcontrol);
}
#endif

static int fsm_startup(struct snd_pcm_substream *substream,
            struct snd_soc_dai *dai)
{
    int ret;
#ifdef KERNEL_VER_OVER_4_19_1
    fsm_pdata_t *pdata = snd_soc_component_get_drvdata(dai->component);
#else
    fsm_pdata_t *pdata = snd_soc_codec_get_drvdata(dai->codec);
#endif
    fsm_dev_t *fsm_dev = &pdata->fsm_dev;
    fsm_config_t *config = fsm_get_config();

    pr_info("enter.\n");

    if (!substream->runtime){
        pr_info("substream->runtime == NULL.\n");
        return 0;
    }

    if(!(fsm_dev->dev_state & STATE(FW_INITED))) {
        pr_info("fw not load yet, reload\n");
        ret = fsm_i2c_event(FSM_EVENT_LOAD_FW, 0);
    }

    if(config->next_scene != fsm_dev->cur_scene) {
        pr_info("force to set preset %d\n", config->next_scene);
        fsm_i2c_event(FSM_EVENT_SET_SCENE, 1);
    }

    ret = snd_pcm_hw_constraint_mask64(substream->runtime, \
            SNDRV_PCM_HW_PARAM_FORMAT, FSM_FORMATS);
    if (ret < 0)
        return ret;

    ret |= snd_pcm_hw_constraint_list(substream->runtime, 0,
            SNDRV_PCM_HW_PARAM_RATE, &fsm_constraints);

    pr_info("exit. err = %d\n", ret);
    return ret;
}


static void fsm_shutdown(struct snd_pcm_substream *substream,
            struct snd_soc_dai *dai)
{
    pr_info("%s enter.\n", __func__);
}

static int fsm_set_dai_sysclk(struct snd_soc_dai *codec_dai,
            int clk_id, unsigned int freq, int dir)
{
    pr_info("%s freq: %d.\n", __func__, freq);
    return 0;
}

static int fsm_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
    int format = 0, ret = 0;

    pr_info("%s enter.\n", __func__);

    switch (fmt & SND_SOC_DAIFMT_MASTER_MASK)
    {
    case SND_SOC_DAIFMT_CBS_CFS:
        /* default value */
        break;
    case SND_SOC_DAIFMT_CBM_CFM:
    default:
        /* only supports Slave mode */
        pr_err("%s invalid DAI master/slave interface\n", __func__);
        return -EINVAL;
    }

    format = fmt & SND_SOC_DAIFMT_FORMAT_MASK;

    switch(format)
    {
    case SND_SOC_DAIFMT_I2S:
        format = 3;
        break;
    case SND_SOC_DAIFMT_LEFT_J:
        format = 2;
        break;
    default:
        pr_err("%s invalid dai format: %x\n", __func__, format);
        return -EINVAL;
    }
    pr_info("%s exit ret = %d.\n", __func__, ret);
    return ret;
}

static int fsm_hw_params(struct snd_pcm_substream *substream,
            struct snd_pcm_hw_params *params,
            struct snd_soc_dai *dai)
{
    int ret = 0, bclk = 0, srate= 0, format = 0;
    int sample_size = 0, phy_size = 0;

    format = params_format(params);
    /*switch(format)
    {
    case SNDRV_PCM_FORMAT_S16_LE:
        break;
    case SNDRV_PCM_FORMAT_S24_LE:
    case SNDRV_PCM_FORMAT_S24_3LE:
    case SNDRV_PCM_FORMAT_S32_LE:
        break;
    }*/

    srate = params_rate(params);
    sample_size = snd_pcm_format_width(format);
    phy_size = snd_pcm_format_physical_width(format);
    pr_info("%s Requested srate: %d, sample size: %d, physical size: %d\n", __func__,
            srate, sample_size, phy_size);
    bclk = srate * phy_size * 2;
    if(srate == 32000)
        bclk += 32;
    fsm_i2c_event(FSM_EVENT_SET_SRATE, srate);
    fsm_i2c_event(FSM_EVENT_SET_BCLK, bclk);
    pr_info("%s exit.\n", __func__);

    return ret;
}

static int fsm_mute(struct snd_soc_dai *dai, int mute, int stream)
{
    int ret = 0;

    pr_info("%s enter.\n", __func__);
    if(stream == SNDRV_PCM_STREAM_PLAYBACK)
    {
        if(!mute) {
            pr_info("%s --- unmute ---\n", __func__);
            //ret = fsm_i2c_event(FSM_EVENT_LOAD_FW, 0);
            //ret |= fsm_i2c_event(FSM_EVENT_INIT, 0);
            ret = fsm_i2c_event(FSM_EVENT_SPEAKER_ON, 0);
            ret |= fsm_i2c_event(FSM_EVENT_CALIBRATE, 0);
        } else {
            pr_info("%s --- mute ---\n", __func__);
            ret = fsm_i2c_event(FSM_EVENT_SPEAKER_OFF, mute);
        }
    }
    pr_info("%s exit ret = %d.\n", __func__, ret);
    return 0;
}

static int fsm_digital_mute(struct snd_soc_dai *dai, int mute)
{
    return fsm_mute(dai, mute, SNDRV_PCM_STREAM_PLAYBACK);
}

static const struct snd_soc_dai_ops fsm_dai_ops = {
    .startup =    fsm_startup,
    .set_fmt =    fsm_set_fmt,
    .set_sysclk =    fsm_set_dai_sysclk,
    .hw_params =    fsm_hw_params,
    //.mute_stream =    fsm_mute,
    .digital_mute = fsm_digital_mute,
    .shutdown =    fsm_shutdown,
};

static struct snd_soc_dai_driver fsm_dai[] =
{
    {
        .name = "foursemi-aif",
        .id = 1,
        .playback =
        {
            .stream_name = "Playback",
            .channels_min = 1,
            .channels_max = 4,
            .rates = FSM_RATES,
            .formats = FSM_FORMATS,
        },
        .ops = &fsm_dai_ops,
        .symmetric_rates = 1,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
        .symmetric_channels = 1,
        .symmetric_samplebits = 1,
#endif
    },
    {
        .name = "foursemi-aif-right",
        .id = 1,
        .playback =
        {
            .stream_name = "Playback",
            .channels_min = 1,
            .channels_max = 4,
            .rates = FSM_RATES,
            .formats = FSM_FORMATS,
        },
        .ops = &fsm_dai_ops,
        .symmetric_rates = 1,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
        .symmetric_channels = 1,
        .symmetric_samplebits = 1,
#endif
    },

};

#define NAME_SIZE    32
static char g_codec_name[NAME_SIZE] = "";
static char g_codec_dai_name[NAME_SIZE] = "";

int fsm_get_codec_info(char *codec_name, char *dai_name)
{
    if(fsm_check_device() <= 0)
        return -EINVAL;
    if(codec_name == NULL || dai_name == NULL)
        return -EINVAL;

    memcpy(codec_name, g_codec_name, strlen(g_codec_name));
    memcpy(dai_name, g_codec_dai_name, strlen(g_codec_dai_name));
    pr_info("%s Codec: %s\n", __func__, codec_name);
    pr_info("%s Codec DAI: %s\n", __func__, dai_name);

    return 0;
}
EXPORT_SYMBOL(fsm_get_codec_info);

int fsm_set_codec_info(struct device *dev, struct snd_soc_dai_driver *dai)
{
    char name[NAME_SIZE];
    char tmp[NAME_SIZE];
    int id1, id2;

    if (dev_name(dev) == NULL)
        return -EINVAL;

    strlcpy(name, dev_name(dev), NAME_SIZE);
    if (sscanf(name, "%x-%x", &id1, &id2) != 2)
        return -EINVAL;

    /* sanitize component name for DAI link creation */
    snprintf(tmp, NAME_SIZE, "%s.%s", dev->driver->name, name);
    strlcpy(name, tmp, NAME_SIZE);
    memcpy(g_codec_name, name, strlen(name));
    memcpy(g_codec_dai_name, dai->name, strlen(dai->name));
    pr_info("%s Codec: %s\n", __func__, name);
    pr_info("%s Codec DAI: %s\n", __func__, dai->name);

    return 0;
}
#ifdef FIRMWARE_CHECKER
#include <linux/timer.h>
#include <linux/workqueue.h>
static void firmware_checker_timer_handler(struct timer_list *t);

static void firmware_checker_timer_handler(struct timer_list *t)
{
    int ret = 0;
    fsm_pdata_t *pdata = from_timer(pdata, t, firmware_checker_timer);

    pr_info("%s: firmware checking...",__func__);

    ret = queue_work(pdata->firmware_checker_wq, &pdata->firmware_checker);
    if (!ret)
        pr_info("Error: %s (%d)\n", __func__, ret);
}
static void firmware_check_callback(struct work_struct *work)
{
    int ret = 0;
    fsm_pdata_t *pdata = container_of(work, fsm_pdata_t, firmware_checker);
    fsm_dev_t *fsm_dev = &pdata->fsm_dev;

    if (fsm_dev->dev_state & STATE(FW_INITED)) {
        pr_info("%s : firmware was loaded \n", __func__);
        del_timer_sync(&pdata->firmware_checker_timer);

    } else {
        pr_info("try to load the firmware=%s\n", fsm_dev->preset_name);
        ret |= fsm_init_firmware(pdata, 1);
        mod_timer(&pdata->firmware_checker_timer, jiffies + msecs_to_jiffies(1000));
    }

}
#endif
#ifdef KERNEL_VER_OVER_4_19_1
static int fsm_codec_probe(struct snd_soc_component *comp)
#else
static int fsm_codec_probe(struct snd_soc_codec *codec)
#endif
{
#ifdef KERNEL_VER_OVER_4_19_1
    fsm_pdata_t *pdata = snd_soc_component_get_drvdata(comp);
#else
    fsm_pdata_t *pdata = snd_soc_codec_get_drvdata(codec);
#endif
    int ret = 0;

    pr_info("enter.\n");

#ifdef KERNEL_VER_OVER_4_19_1
    pdata->component = comp;
#else
    pdata->codec = codec;
#endif
    pr_info("firmware request\n");

#ifdef FIRMWARE_CHECKER
    pdata->firmware_checker_wq = create_singlethread_workqueue("firmware_checkQ");
    if (!pdata->firmware_checker_wq) {
         pr_info("Error: Create firmware checker workqueue failed\n");
         return 0;
     }
     timer_setup(&pdata->firmware_checker_timer, firmware_checker_timer_handler, 0);
     INIT_WORK(&pdata->firmware_checker, firmware_check_callback);
     mod_timer(&pdata->firmware_checker_timer, jiffies + msecs_to_jiffies(1000));
#else
    ret |= fsm_init_firmware(pdata, 1);
#endif
    ret |= fsm_add_widgets(pdata);

    pr_info("exit, fs16xx codec registered, ret = %d.\n", ret);

    return ret;
}
#ifdef KERNEL_VER_OVER_4_19_1
static void fsm_codec_remove(struct snd_soc_component *comp){}
#else
static int fsm_codec_remove(struct snd_soc_codec *codec)
{
    return 0;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
#ifdef  CONFIG_FSM_REGMAP
static struct regmap *fsm_get_regmap(struct device *dev)
{
    fsm_pdata_t *pdata = dev_get_drvdata(dev);
    fsm_dev_t *fsm_dev = &pdata->fsm_dev;

    if(pdata == NULL)
    {
        pr_err("%s get drvdata failed.\n", __func__);
        return NULL;
    }

    return fsm_dev->regmap;
}
#endif
#endif

#ifdef KERNEL_VER_OVER_4_19_1
static struct snd_soc_component_driver soc_component_dev_fsm =
#else
static struct snd_soc_codec_driver soc_codec_dev_fsm =
#endif
{
    .probe =    fsm_codec_probe,
    .remove =    fsm_codec_remove,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
#ifdef  CONFIG_FSM_REGMAP
    .get_regmap =    fsm_get_regmap,
#endif
#endif
};

int fsm_codec_register(struct i2c_client *i2c)
{
    int ret = 0;

    pr_info("%s enter, i2c addr: 0x%02x\n", __func__, i2c->addr);

    if (fsm_check_device() == 0) {
        pr_info("%s register codec(1)\n", __func__);
#ifdef KERNEL_VER_OVER_4_19_1
        ret = snd_soc_register_component(&i2c->dev,&soc_component_dev_fsm,
                &fsm_dai[0],1);
#else
        ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_fsm,
            &fsm_dai[0], 1);
#endif
        fsm_set_codec_info(&i2c->dev, &fsm_dai[0]);
    } else {
        pr_info("%s register codec(2)\n", __func__);
#ifdef KERNEL_VER_OVER_4_19_1
        ret = snd_soc_register_component(&i2c->dev, &soc_component_dev_fsm,
        &fsm_dai[1], 1);

#else
        ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_fsm,
        &fsm_dai[1], 1);
#endif
        fsm_set_codec_info(&i2c->dev, &fsm_dai[1]);
    }

    if (ret < 0)
    {
        dev_err(&i2c->dev, "%s failed to register CODEC DAI: %d\n", __func__, ret);
        return ret;
    }

    pr_info("%s exit, ret:%d\n", __func__, ret);
    return ret;
}

int fsm_codec_unregister(struct i2c_client *i2c)
{
    pr_info("%s enter.\n", __func__);
#ifdef KERNEL_VER_OVER_4_19_1
    snd_soc_unregister_component(&i2c->dev);
#else
    snd_soc_unregister_codec(&i2c->dev);
#endif
    return 0;
}

