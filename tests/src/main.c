#include "../header/func.h"


int main()
{
    char text[TEXTSIZE];
    Command user_cmd;
    char command_string_for_jobs[TEXTSIZE];
    
    while(1)
    {   
        printf("hw1shell$ ");
        fgets(text, sizeof(text), stdin);
   
           text[strcspn(text, "\n")]= '\0';

            // Parse the input line
            if (parse_command(text, &user_cmd) != 0) 
            {
                // Error was printed inside parse_command (e.g., too many arguments)
                continue;
            }

            // Check internal commands (exit, cd, jobs)

            if (strcmp(user_cmd.args[0], "exit") == 0) 
            {
                break;
            // Guideline 2: exit command
                handle_exit();
            }

            else if (strcmp(user_cmd.args[0], "cd") == 0) 
            {
                // Guideline 3: cd command
                handle_cd(user_cmd.args);
            }

            else if (strcmp(user_cmd.args[0], "jobs") == 0) 
            {
            // Guideline 4: jobs command
                handle_jobs();
            }

            else 
            {
            //Execute external commands
                //execute_external_command(&user_cmd, command_string_for_jobs);
            }
    }
    void shrag();
    return 0;
}
