#include "byzantine_orchestra.h"

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
