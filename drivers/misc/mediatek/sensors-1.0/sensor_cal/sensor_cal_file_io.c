#include "sensor_cal_file_io.h"
#include <hwmsensor.h>
#include <linux/fs.h>
#include <linux/syscalls.h>

#define SENSOR_CAL_TAG	"SENSOR_CAL"
#define SENSOR_CAL_ERR(fmt, args...)	pr_err(SENSOR_CAL_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define SENSOR_CAL_LOG(fmt, args...)	pr_debug(SENSOR_CAL_TAG fmt, ##args)

#define PROFILE ID_BASE -1
#define DATALEN 10
#define MSGLEN  128

#define MAX_FILESIZE  0x200000

static char msg[MSGLEN]; //only for cal file
static char hist[MSGLEN]; //for sensor logging
static char* dmsg = "not initailized\n"; //msg when failed to be initialized

int sensor_calibration_read(int sensor, int* cal);
int sensor_calibration_save(int sensor, int* cal);
int sensor_history_save(int sensor, const char* __event, const char* __msg );

static int check_filesize(char* name)
{
	struct kstat stat;
	int res, ret = 1;

	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);

	res = vfs_stat(name, &stat);

	if (res) {
		ret = -1;
	} else if (stat.size > MAX_FILESIZE) {
		ret = 0;
	} else {
		ret = 1;
	}

	set_fs(old_fs);

	return ret;
}

static char* hist_msg(int sensor, const char* __event, const char* __msg)
{
	struct tm tm;
	struct timespec tp = __current_kernel_time();
	time_to_tm(tp.tv_sec, sys_tz.tz_minuteswest * 60 * (-1), &tm);
	if (__event && __msg) {
		memset(hist, 0x0, MSGLEN);
		sprintf(hist, "[%02d-%02d]%02d:%02d:%02d.%.03lu : sensor_type(%d) event(%s) : ",
				tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, (unsigned long)tp.tv_nsec/1000000, sensor, __event);
		strncat(hist, __msg, (MSGLEN -1) - strlen(hist));
		return hist;
	}
	return dmsg;
}

static int proxy_baro_msg(int sensor, int* cal)
{
	int pCal;
	int ret;
	int res = sensor_calibration_read(sensor, &pCal);
	if (res)
		ret = sprintf(msg, "%d 0", *cal);
	else
		ret = sprintf(msg, "%d %d", *cal, pCal);
	return ret;
}

static int accel_gyro_msg(int sensor, int* cal)
{
	int pCal[3];
	int ret;
	int res = sensor_calibration_read(sensor, pCal);
	if (res)
		ret = sprintf(msg, "%d %d %d 0 0 0", cal[0], cal[1], cal[2]);
	else
		ret = sprintf(msg, "%d %d %d %d %d %d", cal[0], cal[1], cal[2], pCal[0], pCal[1], pCal[2]);
	return ret;
}

static char* cur_msg(void)
{
	return msg;
}

static char* sensor_msg(int sensor, int* cal)
{
	int ret = 0, res = 0;

	memset(msg, 0x0, MSGLEN);

	switch (sensor) {
	case ID_ACCELEROMETER:
	case ID_GYROSCOPE:
		ret = accel_gyro_msg(sensor, cal);
		break;

	case ID_PRESSURE:
	case ID_PROXIMITY:
		ret = proxy_baro_msg(sensor, cal);
		break;

	default:
		res = 1;
		break;
	}

	msg[ret++] = '\n';
	msg[ret] = '\0';

	return (res) ? dmsg : msg;
}

static char* get_path(int sensor)
{
	char* fname = NULL;

	switch (sensor) {
	case PROFILE:
		fname = SENSOR_HISTORY_FILE;
		break;

	case ID_ACCELEROMETER:
		fname = SENSOR_ACCEL_CAL_FILE;
		break;

	case ID_GYROSCOPE:
		fname = SENSOR_GYRO_CAL_FILE;
		break;

	case ID_PRESSURE:
		fname = SENSOR_BAROMETER_CAL_FILE;
		break;

	case ID_PROXIMITY:
		fname = SENSOR_PROXIMITY_CAL_FILE;
		break;

	default:
		SENSOR_CAL_LOG("Undefined sensor type\n");
		fname = SENSOR_HISTORY_FILE;
		break;
	}

	return fname;
}

int __sensor_read(const char* fname, const int sensor, int* cal)
{
	int fd, res = -EINVAL;
	char buf[MSGLEN];
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	fd = ksys_open(fname, O_RDONLY, S_IRUGO | S_IWUSR);
	if (fd < 0)
		goto cal_read_error;

	res = ksys_read(fd, buf , MSGLEN);
	if (res < 0)
		goto cal_read_error;

	switch (sensor) {
	case ID_ACCELEROMETER:
	case ID_GYROSCOPE:
		sscanf(buf,"%d %d %d",&cal[0], &cal[1], &cal[2]);
		break;

	case ID_PRESSURE:
	case ID_PROXIMITY:
		sscanf(buf,"%d",cal);
		break;

	default:
		break;
	}

cal_read_error:
	ksys_close(fd);
	ksys_chmod(fname, S_IRUGO | S_IWUSR);
	set_fs(old_fs);
	return (res < 0) ? -EINVAL : 0;
}

int __sensor_write(const char* fname, const char* msg, int flag)
{
	int fd, res = -EINVAL;
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	fd = ksys_open(fname, flag, S_IRUGO | S_IWUSR);
	if (fd < 0)
		goto cal_write_error;

	res = ksys_write(fd, msg, strlen(msg));
	if (res < 0)
		goto cal_write_error;

	//ksys_sync(fd);

cal_write_error:
	ksys_close(fd);
	ksys_chmod(fname, S_IRUGO | S_IWUSR);
	set_fs(old_fs);
	return (res < 0) ? -EINVAL : 0;
}

static int sensor_read(const int sensor, int* cal)
{
	return __sensor_read(get_path(sensor), sensor, cal);
}

static int sensor_write(const int sensor, const char* msg)
{
	return __sensor_write(get_path(sensor), msg, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC);
}

int sensor_history_save(int sensor, const char* __event, const char* __msg )
{
	int flag = O_WRONLY | O_CREAT | S_IROTH | O_APPEND | O_SYNC;

	if (!check_filesize(get_path(PROFILE)))
		flag |= O_TRUNC;

	return __sensor_write(get_path(PROFILE), hist_msg(sensor, __event, __msg), flag);
}
EXPORT_SYMBOL(sensor_history_save);

int sensor_calibration_save(int sensor, int* cal)
{
	return sensor_write(sensor, sensor_msg(sensor, cal)) || sensor_history_save(sensor, "calibration", cur_msg());
}
EXPORT_SYMBOL(sensor_calibration_save);

int sensor_calibration_read(int sensor, int *cal_read)
{
	return sensor_read(sensor, cal_read);
}
EXPORT_SYMBOL(sensor_calibration_read);
