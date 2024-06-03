#define BAT_NTC_100 1
#define RBAT_PULL_UP_R             100000
#define RBAT_PULL_UP_VOLT          1800
#define BIF_NTC_R 16000

/*
 * NTC data sheet value has low accuracy in low voltage
 * This value is tuning value
 */
struct fuelgauge_temperature Fg_Temperature_Table[21] = {
        {-20, 1041751},
        {-15, 841598},
        {-10, 557962},
        {-5, 470422},
        {0, 328651},
        {5, 272290},
        {10, 199137},
        {15, 162617},
        {20, 127071},
        {25, 100000},
        {30, 79232},
        {35, 63181},
        {40, 50687},
        {45, 40908},
        {50, 31226},
        {55, 27094},
        {60, 22221},
        {65, 18320},
        {70, 15175},
        {75, 12627},
        {80, 10557},
};
