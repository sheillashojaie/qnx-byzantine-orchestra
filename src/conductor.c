#include "byzantine_orchestra.h"

void* conductor_thread(void *unused_arg) {
	(void) unused_arg;

	for (int pulse_count = 0; pulse_count < 16 && program_running;
			pulse_count++) {
		printf("\n--- Pulse %d ---\n", pulse_count + 1);

		pulse_msg_t msg = { .type = 1 }; // Pulse
		for (int i = 0; i < num_musicians; i++) {
			if (MsgSend(coids_to_musicians[i], &msg, sizeof(msg), NULL, 0)
					== -1) {
				printf("Conductor: Failed to send message to %s: %s\n",
						musicians[i].name, strerror(errno));
			}
		}
		// Sleep for one beat
		usleep((60.0 / conductor_bpm) * 1000000);

		// Collect reports from musicians
		double total_reported_bpm = 0;
		int reporting_musicians = 0;
		time_t start_report_time = time(NULL);

		while (reporting_musicians < num_musicians
				&& time(NULL) - start_report_time < 5) {
			pulse_msg_t msg;
			int rcvid = MsgReceive(conductor_chid, &msg, sizeof(msg), NULL);

			if (rcvid == -1 && errno != EINTR) {
				printf("Conductor: Receive error: %s\n", strerror(errno));
				break;
			}

			if (msg.type == 2) { // Report
				total_reported_bpm += msg.reported_bpm;
				reporting_musicians++;
			}

			MsgReply(rcvid, EOK, NULL, 0);
		}

		// Update conductor's BPM based on reports
		if (reporting_musicians > 0) {
			conductor_bpm = total_reported_bpm / reporting_musicians;
			printf("Conductor: Average reported BPM: %.1f\n", conductor_bpm);
		} else {
			printf("Conductor: No reports received, keeping current tempo\n");
		}
	}

	printf("\n--- Concert ended after 16 pulses ---\n");
	return NULL;
}
