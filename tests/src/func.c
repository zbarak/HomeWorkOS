#include "../header/func.h"

// A helper function to print errors according to Guideline 13.
void report_syscall_error(const char *syscall_name) 
{
    // The format required: hw1shell: %s failed, errno is %d
    fprintf(stderr, "hw1shell: %s failed, errno is %d\n", syscall_name, errno);
}


int split2words(char *words[], char *word_pointer)
{
    int i = 0;
    //word_pointer = line; // marks new line
    while((*word_pointer == ' ') || (*word_pointer == '\t')){word_pointer++;} //skips space abd \t
    while((*word_pointer != '\0') && (i < WORDSIZE)){
        words[i] = word_pointer; //place word into array
        while((*word_pointer != ' ') && (*word_pointer != '\t') && (*word_pointer != '\0')){word_pointer++;}
        
        if(*word_pointer == '\0') {i++; break;}
        *word_pointer = '\0';
        word_pointer++;
        while((*word_pointer == ' ') || (*word_pointer == '\t')) word_pointer++;
        i++;
    }
    //if (words[i] == NULL) i--;
    //printf("%s\n", line);
    return i;
}

// Parses a command line into a structured Command object.
int parse_command(char *line, Command *cmd) 
{
    // We use a temporary array to hold the pointers from split2words, 
    // ensuring we leave space for the final NULL for exec() if needed.
    char *temp_args[WORDSIZE + 1]; 
    int word_count;
    
    // 1. Initialize the structure
    memset(cmd, 0, sizeof(Command));

    // 2. Tokenize the line using the user-provided function
    word_count = split2words(temp_args, line);

    // 3. Handle tokenization errors (too many parameters, Guideline 14)
    if (word_count > WORDSIZE) {
        fprintf(stderr, "hw1shell: command has too many parameters (max %d)\n", WORDSIZE);
        return -1;
    }

    // 4. Set the command count
    cmd->count = word_count;

    // 5. Check if the line was just empty or contained only whitespace
    if (cmd->count == 0) {
        return 0;
    }
    
    // 6. Copy tokens to the final Command structure args array and handle background flag
    int i;
    for (i = 0; i < cmd->count; i++) {
        cmd->args[i] = temp_args[i];
    }
    
    // 7. Handle the background flag ('&') (Guideline 5)
    char *last_token = cmd->args[cmd->count - 1];

    if (strcmp(last_token, "&") == 0) {
        cmd->is_background = 1;

        // The '&' token is removed from the argument list for exec()
        // We set the pointer at that position to NULL.
        cmd->args[cmd->count - 1] = NULL; 

        // Decrease the count of actual command arguments passed to exec/cd logic
        cmd->count--;
    }


    // 8. Finalize the argument list for exec() (Crucial requirement)
    // The exec family of functions requires the last element of the argument list to be NULL.
    // This handles both cases: 
    // a) If '&' was present, the slot cmd->count-1 is already NULL from step 7.
    // b) If '&' was NOT present, the slot cmd->count needs to be NULL-terminated.
    cmd->args[cmd->count] = NULL;

    return 0; // Success
}

// Implements the 'cd' (change directory) built-in command.
void handle_cd(char **args) 
{
    // 1. Check for the valid number of arguments (Guideline 3)
    // A valid cd command must have exactly two tokens: "cd" and "<directory>".
    // So, args[0] is "cd", args[1] is the target directory, and args[2] must be NULL.

    // Check if the target directory (args[1]) exists AND if there are no extra arguments (args[2] is NULL).
    if (args[1] == NULL || args[2] != NULL) {
        // This covers cases like:
        // - "cd" (args[1] is NULL)
        // - "cd dir1 dir2" (args[2] is not NULL)

        // Error message required by Guideline 3
        fprintf(stderr, "hw1shell: invalid command\n");
        return;
    }

    // 2. Execute the directory change
    const char *target_dir = args[1];

    if (chdir(target_dir) == -1) {
        // chdir failed. This happens if the directory doesn't exist,
        // or the user doesn't have permission, etc.

        // Per Guideline 13: Report the system call failure
        report_syscall_error("chdir");

        // A failed chdir() due to a non-existent directory is essentially an invalid/unexecutable command.
        fprintf(stderr, "hw1shell: invalid command\n");
    }

    // If chdir is successful, it changes the working directory, and the function simply returns.
    // No output is required for a successful 'cd' operation.
}

void handle_exit(Job jobs[])
{
    int status;

    // Reap all background jobs before exiting
    for (int i = 0; i < MAX_BG; i++) {
        if (jobs[i].in_use) {
            if (waitpid(jobs[i].pid, &status, 0) < 0) 
                report_syscall_error("waitpid");
            else
                printf("hw1shell: pid %d finished\n", jobs[i].pid);
        }
    }
}

void handle_jobs(Job jobs[]) 
{
    for(int i = 0; i < MAX_BG; i++){
        if(jobs[i].in_use){
            printf("%d\t%s\n", jobs[i].pid, jobs[i].command);
        }
    }
}

void execute_external_command(Command *user_cmd, char *command_string_for_jobs, Job jobs[])
{
    if((user_cmd->count == 0) || (user_cmd->args[0] == NULL)) //Make sure there is actual proper command to execute
        return;

    int is_bg = user_cmd->is_background;
    if(is_bg){                              //Check if the command is background, and then check if we have free space for it (MAX = 4)
        int running_bg = 0;
        for(int i = 0; i < MAX_BG; i++){
            if(jobs[i].in_use){
                running_bg++;
            }
        }
    

    if(running_bg >= MAX_BG){ //Too many background jobs
        fprintf(stderr, "hw1shell: too many background commands running\n");
        return;
        }
    }
    pid_t pid = fork();  //create a new process to actually perform the external command on the child process.

    if(pid < 0){
        report_syscall_error("fork"); //fork() failed: no child was created, report the issue(?)
        fprintf(stderr, "hw1shell: invalid command\n"); //print the errno code?
        return;
    }

    if(pid == 0){
        //=============================
        //  This is the child process
        //=============================
        execvp(user_cmd->args[0], user_cmd->args); //if all went good we never return
        report_syscall_error("execvp");             //if we did continue, that means execvp failed
        fprintf(stderr, "hw1shell: invalid command\n");//So we report it and exit <----------------------------- FIX REPORTING LATER
        exit(1);
    }

    else {
        //=============================
        //  This is the parent process
        //=============================
        
        if(is_bg){                  //Find a free spot in jobs[], as this is a background command
            int slot = -1;
            for(int i = 0; i< MAX_BG; i++){
                if(!jobs[i].in_use){
                    slot = i;
                    break;
                }
            }
        
            if(slot == -1){ //We already checked but just to make sure
                fprintf(stderr, "hw1shell: too many background commands running\n");
                return;
            }

            jobs[slot].pid = pid;
            jobs[slot].in_use = 1;
            //Store the original command line (with &)
            strncpy(jobs[slot].command, command_string_for_jobs, TEXTSIZE - 1);
            jobs[slot].command[TEXTSIZE - 1] = '\0';

            printf("hw1shell: pid %d started\n", pid);
        }
        else {
        int status;
        if(waitpid(pid, &status, 0) < 0) { //If all worked we return with value > 0
            // we returned, waitpid failed
            report_syscall_error("waitpid");
            fprintf(stderr, "hw1shell: invalid command\n");
            }
        }
    }
}

void reap_background_jobs(Job jobs[])
{
    int status;
    //Go over every background job slot:
    for(int i = 0; i < MAX_BG; i++){
        if(jobs[i].in_use){
            pid_t ret = waitpid(jobs[i].pid, &status, WNOHANG); //Check if this child process is finished without blocking it
            if (ret > 0){
                printf("hw1shell: pid %d finished\n", jobs[i].pid); //Print which child with which pid has finished.
                //free up its slot
                jobs[i].in_use = 0;
                jobs[i].pid = 0;
                jobs[i].command[0] = '\0';
            }
            else if (ret < 0) {
                //Error while checking this child
                report_syscall_error("waitpid");
                fprintf(stderr, "hw1shell: invalid command\n");
                //Also free the slots
                jobs[i].in_use = 0;
                jobs[i].pid = 0;
                jobs[i].command[0] = '\0';
            }
            //ret == 0 -> child is still running, do nothing
        }
    }
}