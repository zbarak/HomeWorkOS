#include "../header/func.h"
#include <stdlib.h>
#include <string.h>
#define TEXTSIZE 200
#define WORDSIZE 50

int main()
{
    char text[TEXTSIZE];
    char *words[WORDSIZE];
    int num_of_words=0;
    char* token;
    int i;
    int a=1;
    int b=3;
    printf("a+b= %d",a+b);
    
    while(1)
    {
        printf("hw1shell$ ");
        fgets(text, sizeof(text), stdin);
        if (strcmp(text, "exit\n") == 0)
            break;
        else
        {
           num_of_words=0;
           text[strcspn(text, "\n")]= '\0';
           token = strtok(text, " ");
           while (token != NULL)
           {
            words[num_of_words] = token;
            num_of_words++;
            token = strtok(NULL, " ");
           }

           for (i=0; i<num_of_words; i++)
           {
            printf("hw1shell$ %s\n", words[i]);
           }
            printf("hw1shell$ Number of words: %d\n", num_of_words);
        } 
    }
    return 0;
}
