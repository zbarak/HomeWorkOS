#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#define TEXTSIZE 1024
#define WORDSIZE 64
#define MAX_BG 4

//Structure to hold info about PIDs and background proccesses
typedef struct Job {
    pid_t pid;                  //0 means free slot aka new child, else its the parent
    char command[TEXTSIZE];     //store the command line for child use
    int in_use;                 //flag: 1 if slot is used, 0 if slot is free
}   Job;

// Structure to hold information about a tokenized command
typedef struct Command{
    char *args[WORDSIZE]; // Command and its arguments (argv-style array)
    int count;              // Number of arguments
    int is_background;      // Flag: 1 if ends with '&', 0 otherwise
} Command;

int split2words(char *words[], char *word_pointer);
int parse_command(char *line, Command *cmd);
void handle_cd(char **args) ;
void handle_exit();
void execute_external_command(Command *user_cmd, char *command_string_for_jobs,Job jobs[]);
void report_syscall_error(const char *syscall_name);
void handle_jobs(Job jobs[]);
void reap_background_jobs(Job jobs[]);