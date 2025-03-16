#include "byzantine_orchestra.h"

musician_t musicians[MAX_MUSICIANS];
int num_musicians = 0;
double conductor_bpm = DEFAULT_BPM;
int conductor_chid;
int coids_to_musicians[MAX_MUSICIANS];
volatile bool program_running = true;
int byzantine_count = 0;

const char *musician_names[MAX_MUSICIANS] = { "Melody", "Harmony", "Bass",
		"Counter-Melody", "Rhythm" };

int parse_arguments(int argc, char *argv[]);
const char* select_piece();
int initialize_musicians(const char *notes[MAX_MUSICIANS][MAX_NOTES]);
void cleanup_resources();

int main(int argc, char *argv[]) {
	srand(time(NULL)); // Seed random number

	if (parse_arguments(argc, argv) != 0) {
		return 1;
	}

	const char *filename = select_piece();
	if (!filename) {
		return 1;
	}

	const char *notes[MAX_MUSICIANS][MAX_NOTES];
	if (read_notes_from_file(filename, musician_names, notes) == -1) {
		printf("Could not read notes from file\n");
		return 1;
	}

	assign_byzantine_musicians();

	printf("Byzantine Orchestra starting with %d musicians at %.1f BPM\n\n",
			num_musicians, conductor_bpm);

	conductor_chid = ChannelCreate(0);
	if (conductor_chid == -1) {
		perror("Could not create conductor channel");
		return 1;
	}

	if (initialize_musicians(notes) != 0) {
		cleanup_resources();
		return 1;
	}

	sleep(1);

	pthread_t conductor;
	pthread_create(&conductor, NULL, conductor_thread, NULL);
	pthread_join(conductor, NULL);

	program_running = false;
	sleep(1);

	cleanup_resources();
	return 0;
}

int parse_arguments(int argc, char *argv[]) {
	if (argc < 3) {
		printf("Usage: %s <num_musicians> <bpm>\n", argv[0]);
		return 1;
	}

	num_musicians = atoi(argv[1]);
	if (num_musicians < 3 || num_musicians > MAX_MUSICIANS) {
		printf("Number of musicians must be between 3 and %d\n", MAX_MUSICIANS);
		return 1;
	}

	conductor_bpm = atof(argv[2]);
	if (conductor_bpm <= 0) {
		printf("BPM must be a positive number\n");
		return 1;
	}

	return 0;
}

const char* select_piece() {
	printf("Select a piece:\n");
	printf("1. Night on Bald Mountain\n");
	int piece_selection;
	scanf("%d", &piece_selection);

	switch (piece_selection) {
	case 1:
		return "../src/night_on_bald_mountain.txt";
	default:
		printf("Invalid choice\n");
		return NULL;
	}
}

int initialize_musicians(const char *notes[MAX_MUSICIANS][MAX_NOTES]) {
	for (int i = 0; i < num_musicians; i++) {
		musicians[i].id = i;
		musicians[i].perceived_bpm = conductor_bpm;
		musicians[i].notes = notes[i];
		musicians[i].note_index = 0;
		musicians[i].name = musician_names[i];

		musicians[i].chid = ChannelCreate(0);
		if (musicians[i].chid == -1) {
			perror("Could not create musician channel");
			return -1;
		}

		musicians[i].coid_to_conductor = ConnectAttach(0, 0, conductor_chid, 0,
				0);
		if (musicians[i].coid_to_conductor == -1) {
			perror("Could not create connection to conductor");
			return -1;
		}

		coids_to_musicians[i] = ConnectAttach(0, 0, musicians[i].chid, 0, 0);
		if (coids_to_musicians[i] == -1) {
			perror("Conductor could not create connection to musician");
			return -1;
		}

		pthread_create(&musicians[i].thread, NULL, musician_thread,
				&musicians[i]);
	}
	return 0;
}

void cleanup_resources() {
	for (int i = 0; i < num_musicians; i++) {
		pthread_cancel(musicians[i].thread);
		ConnectDetach(musicians[i].coid_to_conductor);
		ConnectDetach(coids_to_musicians[i]);
		ChannelDestroy(musicians[i].chid);
	}
	ChannelDestroy(conductor_chid);
}

void assign_byzantine_musicians() {
	// 1 to fewer than half of musicians
	byzantine_count = 1 + (rand() % (num_musicians / 2));

    for (int i = 0; i < num_musicians; i++) {
        musicians[i].is_byzantine = false;
    }

    for (int i = 0; i < byzantine_count; i++) {
        int index;
        do {
            index = rand() % num_musicians;
        } while (musicians[index].is_byzantine);

        musicians[index].is_byzantine = true;
        printf("%s will be a byzantine musician\n", musician_names[index]);
    }
}
