#define BAT_NTC_68 1
#define RBAT_PULL_UP_R             62000
#define RBAT_PULL_UP_VOLT          1800
#define BIF_NTC_R 16000

/*
 * NTC data sheet value has low accuracy in low voltage
 * This value is tuning value
 */
struct fuelgauge_temperature Fg_Temperature_Table[21] = {
		{-20, 601129},
		{-15, 515896},
		{-10, 362915},
		{-5, 280636},
		{0, 213810},
		{5, 171815},
		{10, 140146},
		{15, 109346},
		{20, 81520},
		{25, 65681},
		{30, 52685},
		{35, 41576},
		{40, 33569},
		{45, 26173},
		{50, 21394},
		{55, 17581},
		{60, 15423},
		{65, 12967},
		{70, 10791},
		{75, 9021},
		{80, 7574},
};
