#include <byzantine_orchestra.h>

void* musician_thread(void* arg) {
    musician_t *musician = (musician_t*) arg;
    double reported_bpm;

    while (program_running) {
        // Wait for message from conductor
        pulse_msg_t msg;
        int rcvid = MsgReceive(musician->chid, &msg, sizeof(msg), NULL);

        if (rcvid == -1 && errno != EINTR) {
            printf("%s: Receive error: %s\n", musician->name, strerror(errno));
            continue;
        }

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
            play_note(musician, reported_bpm);

            // Loop notes
            musician->note_index = (musician->note_index + 1) % MAX_NOTES;
            MsgReply(rcvid, EOK, NULL, 0);

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
}

double add_variance(double bpm, deviation_type_t deviation_type) {
	double variance = 0.0;

	switch (deviation_type) {
	case DEVIATION_NORMAL:
		// Normal variance: +/- BPM_TOLERANCE
		variance = ((rand() % 1000) / 10000.0) - BPM_TOLERANCE;
		break;

	case DEVIATION_BYZANTINE:
		// Byzantine variance: at least BPM_TOLERANCE, at most BYZANTINE_MAX_DEVIATION
		if (rand() % 2) {
			// Too fast
			variance = BPM_TOLERANCE
					+ (BYZANTINE_MAX_DEVIATION - BPM_TOLERANCE)
							* (rand() % 1000) / 1000.0;
		} else {
			// Too slow
			variance = -BPM_TOLERANCE
					- (BYZANTINE_MAX_DEVIATION - BPM_TOLERANCE)
							* (rand() % 1000) / 1000.0;
		}
		break;

	case DEVIATION_FIRST_CHAIR:
		// First chair variance: +/- FIRST_CHAIR_MAX_DEVIATION
		variance = ((rand() % 400) / 10000.0) - FIRST_CHAIR_MAX_DEVIATION;
		break;

	default:
		variance = 0.0;
		break;
	}

	return bpm * (1.0 + variance);
}
