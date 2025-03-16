#include "byzantine_orchestra.h"

void* musician_thread(void *arg) {
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
            if (musician->is_byzantine) {
                if (rand() % 2) { // 50% chance of Byzantine timing
                    musician->perceived_bpm = add_byzantine_variance(conductor_bpm);
                    // Report deceptively
                    reported_bpm = add_normal_variance(conductor_bpm);
                    printf("%s [BYZANTINE]: Playing %s at %.1f BPM (reporting: %.1f BPM)\n",
                           musician->name, musician->notes[musician->note_index],
                           musician->perceived_bpm, reported_bpm);
                } else {
                	// Play normally and report truthfully
                    musician->perceived_bpm = add_normal_variance(conductor_bpm);
                    reported_bpm = musician->perceived_bpm;
                    printf("%s [BYZANTINE]: Playing %s at %.1f BPM (reporting: %.1f BPM)\n",
                           musician->name, musician->notes[musician->note_index],
                           musician->perceived_bpm, reported_bpm);
                }
            } else {
                musician->perceived_bpm = add_normal_variance(conductor_bpm);
                reported_bpm = musician->perceived_bpm;

                printf("%s: Playing %s at %.1f BPM\n", musician->name,
                       musician->notes[musician->note_index],
                       musician->perceived_bpm);
            }

            // Loop notes
            musician->note_index = (musician->note_index + 1) % MAX_NOTES;
            MsgReply(rcvid, EOK, NULL, 0);
            // Report back to conductor
            pulse_msg_t report = { .type = 2, .musician_id = musician->id,
                                 .reported_bpm = reported_bpm };

            if (MsgSend(musician->coid_to_conductor, &report, sizeof(report),
                        NULL, 0) == -1) {
                printf("%s: Failed to send report: %s\n", musician->name,
                       strerror(errno));
            }
        } else {
            MsgReply(rcvid, EOK, NULL, 0);
        }
    }

    printf("%s: Thread ending\n", musician->name);
    return NULL;
}
