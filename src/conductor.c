#include <byzantine_orchestra.h>

void* conductor_thread(void *unused_arg) {
    (void) unused_arg;

    double current_bpm = DEFAULT_BPM;
    conductor_bpm = current_bpm;
    target_bpm = current_bpm;

    for (int pulse_count = 0; pulse_count < MAX_PULSES && program_running; pulse_count++) {
        printf("\n--- Pulse %d ---\n", pulse_count + 1);

        bool bpm_changed = false;

        // Possibly change BPM if pulse count is start of new quarter measure
        if ((pulse_count) % 4 == 0) {
            if (rand() % 100 < 50) {
            	// Random BPM between MIN_BPM and MAX_BPM
                double new_bpm = MIN_BPM + ((double) rand() / RAND_MAX) * (MAX_BPM - MIN_BPM);
                target_bpm = new_bpm;
                conductor_bpm = new_bpm;
                bpm_changed = true;
                printf("Conductor: Changing tempo to %.1f BPM\n", new_bpm);
            }
        }

        // Send pulse to all non-blacklisted musicians
        pulse_msg_t msg = { .type = 1 }; // Pulse
        int active_musicians = 0;

        for (int i = 0; i < num_musicians; i++) {
            if (!musicians[i].is_blacklisted) {
                if (MsgSend(coids_to_musicians[i], &msg, sizeof(msg), NULL, 0) == -1) {
                    printf("Conductor: Failed to send message to %s: %s\n",
                           musicians[i].name, strerror(errno));
                } else {
                    active_musicians++;
                }
            }
        }

        if (active_musicians == 0) {
            printf("Conductor: No active musicians remaining, ending concert\n");
            break;
        }

        // Collect reports from active musicians
        double total_reported_bpm = 0;
        int reporting_musicians = 0;
        time_t start_report_time = time(NULL);

        // Wait until all active musician reports received
        while (reporting_musicians < active_musicians &&
               time(NULL) - start_report_time < REPORT_TIMEOUT_SECONDS) {
            pulse_msg_t report;
            int rcvid = MsgReceive(conductor_chid, &report, sizeof(report), NULL);

            if (rcvid == -1 && errno != EINTR) {
                printf("Conductor: Receive error: %s\n", strerror(errno));
                break;
            }

            if (report.type == 2) { // Report
                // Consider reports from non-blacklisted musicians
                if (!musicians[report.musician_id].is_blacklisted) {
                    total_reported_bpm += report.reported_bpm;
                    reporting_musicians++;

                    // Update reputation based on timing accuracy
                    double behaviour_score = calculate_behaviour_score(report.reported_bpm, conductor_bpm);
                    update_reputation(report.musician_id, behaviour_score * 0.5);
                }
            }

            MsgReply(rcvid, EOK, NULL, 0);
        }

        process_reputation_votes();

        // Apply reputation decay
        if (pulse_count % 4 == 0) { // Every measure
            decay_all_reputations();
        }

        // Update conductor's BPM based on trusted musicians only
        if (!bpm_changed && reporting_musicians > 0) {
            double trusted_bpm_sum = 0;
            int trusted_count = 0;

            for (int i = 0; i < num_musicians; i++) {
                if (is_musician_trusted(i)) {
                    trusted_bpm_sum += musicians[i].last_reported_bpm;
                    trusted_count++;
                }
            }

            if (trusted_count > 0) {
                conductor_bpm = trusted_bpm_sum / trusted_count;
                printf("Conductor: Average trusted BPM: %.1f (from %d trusted musicians)\n",
                       conductor_bpm, trusted_count);
            } else {
                printf("Conductor: No trusted musicians available, maintaining tempo\n");
            }
        } else if (!bpm_changed) {
            printf("Conductor: No reports received, keeping current tempo\n");
        }


        print_reputation_status();
    }

    printf("\n--- Concert ended after %d pulses ---\n", MAX_PULSES);
    return NULL;
}
