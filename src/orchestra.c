#include <byzantine_orchestra.h>

int initialize_orchestra(const char *filename) {
    const char *notes[MAX_MUSICIANS][MAX_NOTES];

    if (read_notes_from_file(filename, musician_names, notes) == -1) {
        printf("Could not read notes from file\n");
        return -1;
    }

    assign_byzantine_musicians();

    printf("Byzantine Orchestra starting with %d musicians at %.1f BPM\n\n",
           num_musicians, conductor_bpm);

    conductor_chid = ChannelCreate(0);
    if (conductor_chid == -1) {
        perror("Could not create conductor channel");
        return -1;
    }

    if (initialize_musicians(notes) != 0) {
        free_notes_memory(notes);
        return -1;
    }

    return 0;
}

int initialize_musicians(const char *notes[MAX_MUSICIANS][MAX_NOTES]) {
	for (int i = 0; i < num_musicians; i++) {
		musicians[i].id = i;
		musicians[i].perceived_bpm = conductor_bpm;
		musicians[i].notes = notes[i];
		musicians[i].note_index = 0;
		musicians[i].name = musician_names[i];
		musicians[i].is_first_chair = (i == 0);

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

void cleanup_resources() {
	for (int i = 0; i < num_musicians; i++) {
		pthread_cancel(musicians[i].thread);
		ConnectDetach(musicians[i].coid_to_conductor);
		ConnectDetach(coids_to_musicians[i]);
		ChannelDestroy(musicians[i].chid);
	}
	ChannelDestroy(conductor_chid);
}
