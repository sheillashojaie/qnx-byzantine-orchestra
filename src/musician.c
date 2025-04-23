#include <byzantine_orchestra.h>

void* musician_thread(void* arg) {
    musician_t *musician = (musician_t*) arg;
    double reported_bpm;
    struct timespec start_time, end_time;
    double elapsed_time;
    double wait_time;

    while (program_running) {
        // Wait for message from conductor
        pulse_msg_t msg;
        int rcvid = MsgReceive(musician->chid, &msg, sizeof(msg), NULL);

        if (rcvid == -1 && errno != EINTR) {
            printf("%s: Receive error: %s\n", musician->name, strerror(errno));
            continue;
        }

        // Record start time for next note
        clock_gettime(CLOCK_MONOTONIC, &start_time);

        // Process the message
        if (msg.type == 1) { // Pulse
            bool byzantine_timing = false;
            bool report_honestly = true;

            if (musician->is_byzantine && rand() % 100 < (BYZANTINE_BEHAVIOR_CHANCE * 100)) {
                byzantine_timing = true;
                report_honestly = false;
            }

            update_musician_bpm(musician, byzantine_timing);
            reported_bpm = get_reported_bpm(musician, report_honestly);

            MsgReply(rcvid, EOK, NULL, 0);

            // Calculate how long to wait based on perceived BPM
            double target_time = MICROSECONDS_PER_MINUTE / musician->perceived_bpm;

            // Calculate elapsed time since receiving the message
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000000.0 +
                             (end_time.tv_nsec - start_time.tv_nsec) / 1000.0;

            // Wait for the remaining time
            if (elapsed_time < target_time) {
                wait_time = target_time - elapsed_time;
                usleep((useconds_t)wait_time);
            }

            play_note_with_viz(musician, reported_bpm);

            // Loop notes
            musician->note_index = (musician->note_index + 1) % MAX_NOTES;

            // Report back to conductor
            pulse_msg_t report = {
                .type = 2,
                .musician_id = musician->id,
                .reported_bpm = reported_bpm
            };

            if (MsgSend(musician->coid_to_conductor, &report, sizeof(report), NULL, 0) == -1) {
                printf("%s: Could not send report: %s\n", musician->name, strerror(errno));
            }
        } else {
            MsgReply(rcvid, EOK, NULL, 0);
        }
    }

    return NULL;
}

void update_musician_bpm(musician_t *musician, bool byzantine_timing) {
	deviation_type_t deviation_type;

	if (musician->is_byzantine && byzantine_timing) {
		deviation_type = DEVIATION_BYZANTINE;
	} else if (musician->is_first_chair) {
		deviation_type = DEVIATION_FIRST_CHAIR;
	} else {
		deviation_type = DEVIATION_NORMAL;
	}

	musician->perceived_bpm = add_variance(conductor_bpm, deviation_type);
}

double get_reported_bpm(musician_t *musician, bool report_honestly) {
	if (musician->is_first_chair) {
		if (musician->is_byzantine) {
			// First chair, if Byzantine, uses first chair deviation for reporting
			return add_variance(conductor_bpm, DEVIATION_FIRST_CHAIR);
		} else {
			// First chair, if not Byzantine, reports its actual perceived BPM
			return musician->perceived_bpm;
		}
	} else if (report_honestly) {
		return musician->perceived_bpm;
	} else {
		return add_variance(conductor_bpm, DEVIATION_NORMAL);
	}
}

void play_note(musician_t *musician, double reported_bpm) {
    const char *status = musician->is_byzantine ? "[BYZANTINE]" : "";

    if (musician->is_first_chair && musician->is_byzantine) {
        status = "[FIRST CHAIR, BYZANTINE]";
    } else if (musician->is_first_chair) {
        status = "[FIRST CHAIR]";
    } else if (musician->is_byzantine) {
        status = "[BYZANTINE]";
    }

    if (musician->is_byzantine || musician->is_first_chair) {
        printf("%s %s: Playing %s at %.1f BPM (reporting: %.1f BPM)\n",
                musician->name, status, musician->notes[musician->note_index],
                musician->perceived_bpm, reported_bpm);
    } else {
        printf("%s: Playing %s at %.1f BPM\n", musician->name,
                musician->notes[musician->note_index], musician->perceived_bpm);
    }
    add_note_event(musician->notes[musician->note_index], musician->perceived_bpm, musician->id);
}


double add_variance(double bpm, deviation_type_t deviation_type) {
	double min_deviation, max_deviation;

	switch (deviation_type) {
	case DEVIATION_NORMAL:
		min_deviation = -BPM_TOLERANCE;
		max_deviation =  BPM_TOLERANCE;
		break;

	case DEVIATION_BYZANTINE:
		if (rand() % 2 == 0) {
			// Too fast
			min_deviation =  BPM_TOLERANCE;
			max_deviation =  BYZANTINE_MAX_DEVIATION;
		} else {
			// Too slow
			min_deviation = -BYZANTINE_MAX_DEVIATION;
			max_deviation = -BPM_TOLERANCE;
		}
		break;

	case DEVIATION_FIRST_CHAIR:
		min_deviation = -FIRST_CHAIR_MAX_DEVIATION;
		max_deviation =  FIRST_CHAIR_MAX_DEVIATION;
		break;

	default:
		min_deviation = 0.0;
		max_deviation = 0.0;
		break;
	}

	// Uniform random value between min_deviation and max_deviation
	double random_factor = ((rand() / (double)RAND_MAX) * (max_deviation - min_deviation)) + min_deviation;
	return bpm * (1.0 + random_factor);
}

