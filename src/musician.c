#include "byzantine_orchestra.h"

void* musician_thread(void *arg) {
    musician_t *musician = (musician_t*)arg;

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
            musician->perceived_bpm = add_normal_variance(conductor_bpm); // Add variance to BPM
            const char *note = musician->notes[musician->note_index]; // Play the next note
            printf("%s: Playing %s at %.1f BPM\n", musician->name, note, musician->perceived_bpm);
            musician->note_index = (musician->note_index + 1) % MAX_NOTES; // Loop notes

            MsgReply(rcvid, EOK, NULL, 0);

            // Report back to conductor
            pulse_msg_t report = { .type = 2, .musician_id = musician->id, .reported_bpm = musician->perceived_bpm };
            if (MsgSend(musician->coid_to_conductor, &report, sizeof(report), NULL, 0) == -1) {
                printf("%s: Failed to send report: %s\n", musician->name, strerror(errno));
            }
        } else {
            MsgReply(rcvid, EOK, NULL, 0);
        }
    }

    printf("%s: Thread ending\n", musician->name);
    return NULL;
}
