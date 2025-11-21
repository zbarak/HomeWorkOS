#include "../header/func.h"

void shrag()
{
    printf("Shrags.\n");
}

int split2words(char *words[], char *word_pointer){
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