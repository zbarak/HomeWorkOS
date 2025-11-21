#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define TEXTSIZE 200
#define WORDSIZE 50

// Structure to hold information about a tokenized command
typedef struct {
    char *args[WORDSIZE]; // Command and its arguments (argv-style array)
    int count;              // Number of arguments
    int is_background;      // Flag: 1 if ends with '&', 0 otherwise
} Command;

int split2words(char *words[], char *word_pointer);
int parse_command(char *line, Command *cmd);
void handle_cd(char **args) ;
void handle_exit();
void execute_external_command(Command user_cmd, char command_string_for_jobs);
void report_syscall_error(const char *syscall_name);
void handle_jobs();

void shrag();