#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/neutrino.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <time.h>

#define MAX_MUSICIANS 5
#define DEFAULT_BPM 60
#define MAX_NOTES 100

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
} musician_t;

musician_t musicians[MAX_MUSICIANS];
int num_musicians = 0;
double conductor_bpm = DEFAULT_BPM;
int conductor_chid;
int coids_to_musicians[MAX_MUSICIANS];
volatile bool program_running = true;

void* conductor_thread(void *arg);
void* musician_thread(void *arg);
double add_normal_variance(double bpm);
int read_notes_from_file(const char *filename,
        const char *musician_names[MAX_MUSICIANS],
        const char *notes[MAX_MUSICIANS][MAX_NOTES]);

const char *musician_names[MAX_MUSICIANS] = { "Melody", "Harmony", "Bass",
        "Counter-Melody", "Rhythm" };

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <num_musicians> <bpm>\n", argv[0]);
        return 1;
    }

    num_musicians = atoi(argv[1]);
    if (num_musicians < 1 || num_musicians > MAX_MUSICIANS) {
        printf("Number of musicians must be between 1 and %d\n", MAX_MUSICIANS);
        return 1;
    }

    // Set BPM from command-line argument
    conductor_bpm = atof(argv[2]);
    if (conductor_bpm <= 0) {
        printf("BPM must be a positive number\n");
        return 1;
    }

    // Prompt user to select a piece
    printf("Select a piece:\n");
    printf("1. Night on Bald Mountain\n");
    int piece_selection;
    scanf("%d", &piece_selection);

    const char *filename;
    switch (piece_selection) {
    case 1:
        filename = "../src/night_on_bald_mountain.txt";
        break;
    default:
        printf("Invalid choice\n");
        return 1;
    }

    // Read notes from the selected file
    const char *notes[MAX_MUSICIANS][MAX_NOTES];
    int num_notes = read_notes_from_file(filename, musician_names, notes);
    if (num_notes == -1) {
        printf("Failed to read notes from file\n");
        return 1;
    }

    printf("Byzantine Orchestra starting with %d musicians at %.1f BPM\n\n", num_musicians, conductor_bpm);

    // Create conductor channel
    conductor_chid = ChannelCreate(0);
    if (conductor_chid == -1) {
        perror("Could not create conductor channel");
        return 1;
    }

    // Initialize musicians and assign note sequences and names
    for (int i = 0; i < num_musicians; i++) {
        musicians[i].id = i;
        musicians[i].perceived_bpm = conductor_bpm;
        musicians[i].notes = notes[i];
        musicians[i].note_index = 0;
        musicians[i].name = musician_names[i];

        // Create individual channel for each musician
        musicians[i].chid = ChannelCreate(0);
        if (musicians[i].chid == -1) {
            perror("Could not create musician channel");
            // Clean up previously created channels
            for (int j = 0; j < i; j++) {
                ChannelDestroy(musicians[j].chid);
            }
            ChannelDestroy(conductor_chid);
            return 1;
        }

        // Pre-establish connection to conductor
        musicians[i].coid_to_conductor = ConnectAttach(0, 0, conductor_chid, 0,
                0);
        if (musicians[i].coid_to_conductor == -1) {
            perror("Could not create connection to conductor");
            for (int j = 0; j <= i; j++) {
                ChannelDestroy(musicians[j].chid);
            }
            ChannelDestroy(conductor_chid);
            return 1;
        }
    }

    // Pre-establish conductor's connections to all musicians
    for (int i = 0; i < num_musicians; i++) {
        coids_to_musicians[i] = ConnectAttach(0, 0, musicians[i].chid, 0, 0);
        if (coids_to_musicians[i] == -1) {
            perror("Conductor could not create connection to musician");
            // Clean up
            for (int j = 0; j < i; j++) {
                ConnectDetach(coids_to_musicians[j]);
            }
            for (int j = 0; j < num_musicians; j++) {
                ConnectDetach(musicians[j].coid_to_conductor);
                ChannelDestroy(musicians[j].chid);
            }
            ChannelDestroy(conductor_chid);
            return 1;
        }
    }

    // Create threads for musicians
    for (int i = 0; i < num_musicians; i++) {
        pthread_create(&musicians[i].thread, NULL, musician_thread,
                &musicians[i]);
    }

    sleep(1);

    // Create conductor thread
    pthread_t conductor;
    pthread_create(&conductor, NULL, conductor_thread, NULL);

    // Wait for conductor thread to finish
    pthread_join(conductor, NULL);

    // Signal threads to terminate
    program_running = false;

    sleep(1);

    // Clean up
    for (int i = 0; i < num_musicians; i++) {
        pthread_cancel(musicians[i].thread);
        ConnectDetach(musicians[i].coid_to_conductor);
        ConnectDetach(coids_to_musicians[i]);
        ChannelDestroy(musicians[i].chid);
    }
    ChannelDestroy(conductor_chid);

    return 0;
}

void* conductor_thread(void *unused_arg) {
	(void) unused_arg;

	int pulse_count = 0;
	int max_pulses = 16;

	while (pulse_count < max_pulses && program_running) {
		pulse_count++;

		printf("\n--- Pulse %d ---\n", pulse_count);

		pulse_msg_t msg;
		msg.type = 1; // Pulse

		sleep(1);

		struct timespec start_time;
		clock_gettime(CLOCK_MONOTONIC, &start_time);

		for (int i = 0; i < num_musicians; i++) {
			if (MsgSend(coids_to_musicians[i], &msg, sizeof(msg), NULL, 0)
					== -1) {
				printf("Conductor: Failed to send message to %s: %s\n",
						musicians[i].name, strerror(errno));
			}
		}

		struct timespec end_time;
		clock_gettime(CLOCK_MONOTONIC, &end_time);

		// Sleep for one beat
		usleep((60.0 / conductor_bpm) * 1000000);

		// Collect reports from musicians
		double total_reported_bpm = 0;
		int reporting_musicians = 0;

		time_t start_report_time = time(NULL);
		const int timeout_seconds = 5;

		while (reporting_musicians < num_musicians
				&& time(NULL) - start_report_time < timeout_seconds) {

			pulse_msg_t msg;
			int rcvid = MsgReceive(conductor_chid, &msg, sizeof(msg), NULL);

			if (rcvid == -1) {
				if (errno == EINTR) {
					continue;
				}
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
			double new_bpm = total_reported_bpm / reporting_musicians;
			printf("Conductor: Average reported BPM: %.1f\n", new_bpm);
			conductor_bpm = new_bpm;
		} else {
			printf("Conductor: No reports received, keeping current tempo\n");
		}
	}

	printf("\n--- Concert ended after %d pulses ---\n", pulse_count);
	return NULL;
}

void* musician_thread(void *arg) {
	musician_t *musician = (musician_t*) arg;
	int my_id = musician->id;
	int my_chid = musician->chid;
	int my_coid_to_conductor = musician->coid_to_conductor;

	while (program_running) {
		// Wait for message from conductor on dedicated channel
		pulse_msg_t msg;

		int rcvid = MsgReceive(my_chid, &msg, sizeof(msg), NULL);

		if (rcvid == -1) {
			if (errno == EINTR) {
				continue;
			}
			printf("%s: Receive error: %s\n", musician->name, strerror(errno));
			continue;
		}

		// Process the message
		if (msg.type == 1) { // 1 = pulse
			struct timespec pulse_time;
			clock_gettime(CLOCK_MONOTONIC, &pulse_time);

			// Add a small variance to the BPM
			double perceived_bpm = add_normal_variance(conductor_bpm);
			musician->perceived_bpm = perceived_bpm;

			// Play the note from the sequence
			const char *note = musician->notes[musician->note_index];
			printf("%s: Playing %s at %.1f BPM\n", musician->name, note,
					perceived_bpm);
			 // Loop after MAX_NOTES
			musician->note_index = (musician->note_index + 1) % MAX_NOTES;

			MsgReply(rcvid, EOK, NULL, 0);

			// Report back to conductor
			pulse_msg_t report;
			report.type = 2; // Report
			report.musician_id = my_id;
			report.reported_bpm = perceived_bpm;

			if (MsgSend(my_coid_to_conductor, &report, sizeof(report), NULL, 0)
					== -1) {
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

double add_normal_variance(double bpm) {
	// Add a small variance (+/- 5%)
	double variance = ((rand() % 1000) / 10000.0) - 0.05;
	return bpm * (1.0 + variance);
}

int read_notes_from_file(const char *filename,
		const char *musician_names[MAX_MUSICIANS],
		const char *notes[MAX_MUSICIANS][MAX_NOTES]) {
	FILE *file = fopen(filename, "r");
	if (!file) {
		perror("Failed to open file");
		return -1;
	}

	char line[256];
	int musician_index = 0;

	while (fgets(line, sizeof(line), file)) {
		if (musician_index >= MAX_MUSICIANS) {
			break;
		}

		// Tokenize the line into notes
		char *token = strtok(line, " \n");
		int note_index = 0;

		while (token && note_index < MAX_NOTES) {
			notes[musician_index][note_index] = strdup(token);
			token = strtok(NULL, " \n");
			note_index++;
		}

		musician_index++;
	}

	fclose(file);
	return musician_index;
}
