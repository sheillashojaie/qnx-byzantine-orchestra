#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <errno.h>
#include <sys/neutrino.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include <termios.h>

#define MIN_MUSICIANS 4
#define MAX_MUSICIANS 7
#define DEFAULT_BPM 60
#define MAX_NOTES 100
#define PRIORITY_CONDUCTOR 50
#define BPM_TOLERANCE 0.05
#define BYZANTINE_MAX_DEVIATION 0.20
#define FIRST_CHAIR_MAX_DEVIATION 0.02
#define MAX_PULSES 16
#define REPORT_TIMEOUT_SECONDS 5
#define MICROSECONDS_PER_MINUTE 60000000.0
#define BYZANTINE_BEHAVIOR_CHANCE 0.5
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 40
#define MAX_HISTORY 500
#define REFRESH_INTERVAL_MS 50

typedef enum {
	DEVIATION_NORMAL, DEVIATION_BYZANTINE, DEVIATION_FIRST_CHAIR
} deviation_type_t;

typedef struct {
	int type;        // 1 = pulse, 2 = report
	int musician_id;
	double reported_bpm;
} pulse_msg_t;

typedef struct {
	int id;
	pthread_t thread;
	double perceived_bpm;
	int chid;
	int coid_to_conductor;
	const char **notes;
	int note_index;
	const char *name;
	bool is_byzantine;
	bool is_first_chair;
} musician_t;

extern musician_t musicians[MAX_MUSICIANS];
extern int num_musicians;
extern double conductor_bpm;
extern double target_bpm;
extern int conductor_chid;
extern int coids_to_musicians[MAX_MUSICIANS];
extern volatile bool program_running;
extern volatile bool viz_running;
extern const char *musician_names[MAX_MUSICIANS];
extern int byzantine_count;

#endif
