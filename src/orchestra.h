#ifndef ORCHESTRA_H
#define ORCHESTRA_H

int initialize_orchestra(const char *filename);
int initialize_musicians(const char *notes[MAX_MUSICIANS][MAX_NOTES]);
void assign_byzantine_musicians();
void cleanup_resources();

#endif
