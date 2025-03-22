#ifndef IO_H
#define IO_H

int parse_arguments(int argc, char *argv[]);
const char* select_piece();
int read_notes_from_file(const char *filename,
                         const char *musician_names[MAX_MUSICIANS],
                         const char *notes[MAX_MUSICIANS][MAX_NOTES]);
void free_notes_memory(const char *notes[MAX_MUSICIANS][MAX_NOTES]);

#endif
