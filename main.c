#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

const int MAX_LEN = 256;

void sh_loop();
char* sh_readline();

int main(int argc, char* argv[]) {

    // shell body loop
    sh_loop();

    return EXIT_SUCCESS;
}

void sh_loop() {
    int status = 1;
    char* line;
    do {
        printf(": ");

        line = sh_readline();
        //checking for blank lines and comments
        if(strlen(line) > 0 && line[0] != '#') {
            printf("%s\n", line);
        }        

        if(strcmp(line, "exit") == 0) {
            status = 0;
        }
        free(line);
    } while (status);
}

//reading line from std_in
char* sh_readline() {
    //dynamic buffer we can read stdin into, if we need more space we'll realloc later
    int buffsize = MAX_LEN;
    char* buffer = malloc(sizeof(char) * MAX_LEN);
    int len = 0;
    int c;

    //reading char from stdin into c, if \n or EOF then stop reading
    while((c = getchar()) != '\n' && c != EOF) {
        buffer[len++] = (char) c;

        //uh oh input exceeded buffer size, time to realloc
        if(len >= buffsize) {
            buffsize += MAX_LEN;
            buffer = realloc(buffer, buffsize);
        }
    }
    //null terminate string
    buffer[len] = '\0';

    return buffer;
}