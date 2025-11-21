#include "../header/func.h"


int main()
{
    char text[TEXTSIZE];
    char *words[WORDSIZE];
    int num_of_words=0;
    int i;

    while(1)
    {   
        printf("hw1shell$ ");
        fgets(text, sizeof(text), stdin);
        if (strcmp(text, "exit\n") == 0)
            break;
        else
        {
           num_of_words = 0;
           text[strcspn(text, "\n")]= '\0';

        num_of_words = split2words(words, text);


                    for (i=0; i<num_of_words; i++)
           {
             printf("[%d] %s\n",i, words[i]);
            }
             printf("hw1shell$ Number of words: %d\n", num_of_words);
        } 
    }
    void shrag();
    return 0;
}
