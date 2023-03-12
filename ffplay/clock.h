#ifndef FFPLAY_CLOCK_H_
#define FFPLAY_CLOCK_H_

typedef struct Clock {
    double pts;           /* clock base */
    double pts_drift;     /* clock base minus time at which we updated the clock */
    double last_updated;
    double speed;
    int serial;           /* clock is based on a packet with this serial */
    int paused;
    int *queue_serial;    /* pointer to the current packet queue serial, used for obsolete clock detection */
} Clock;

// 初始化时钟
void init_clock(Clock *c, int *queue_serial);
void set_clock(Clock *c, double pts, int serial);
void sync_clock_to_slave(Clock *c, Clock *slave);
double get_clock(Clock *c);
void set_clock_speed(Clock *c, double speed);
void set_clock_at(Clock *c, double pts, int serial, double time);

#endif