#include "../header/func.h"

int main()
{
    char text[TEXTSIZE];
    Command user_cmd;
    char command_string_for_jobs[TEXTSIZE];
    Job jobs[MAX_BG] = {0};
    char cwd[1024]; // Buffer to store the current working directory
    while(1)
    {
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            printf("Could not get directory");
        }   
        printf("hw1shell-%s$ ", cwd);
        fgets(text, sizeof(text), stdin);
   
        text[strcspn(text, "\n")]= '\0';

        strncpy(command_string_for_jobs, text, TEXTSIZE - 1);
        command_string_for_jobs[TEXTSIZE - 1] = '\0';

        // Parse the input line
        if (parse_command(text, &user_cmd) != 0) 
        {
            // Error was printed inside parse_command (e.g., too many arguments)
            continue;
        }

        if (user_cmd.count == 0) { // If the command was empty (just Enter), do nothing and show prompt again
            continue;
        }

        // Check internal commands (exit, cd, jobs)

        if (strcmp(user_cmd.args[0], "exit") == 0) 
        {
        // Guideline 2: exit command
            handle_exit(jobs);
            break;
        }

        else if (strcmp(user_cmd.args[0], "cd") == 0) 
        {
            // Guideline 3: cd command
            handle_cd(user_cmd.args);

        }

        else if (strcmp(user_cmd.args[0], "jobs") == 0) 
        {
        // Guideline 4: jobs command
            handle_jobs(jobs);
        }

        else 
        {
            //Execute external commands
            execute_external_command(&user_cmd, command_string_for_jobs, jobs);
            if (strcmp(user_cmd.args[0], "cat") == 0)
            {
                printf("\n");
            }
        }
        reap_background_jobs(jobs);
    }
     return 0;
}
