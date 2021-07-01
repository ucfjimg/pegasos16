#ifndef TIME_H_
#define TIME_H_

#ifndef TIME_T_DEFINED_
#define TIME_T_DEFINED_
typedef unsigned long time_t;
#endif

struct tm {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

extern struct tm *localtime(const time_t *pt);
extern time_t mktime(struct tm *tm);
extern time_t time(time_t *tloc);

#endif

