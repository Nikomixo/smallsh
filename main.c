#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

const int MAX_LEN = 256;

//debug flag for printing stuff
#ifndef DEBUG
#define DEBUG 0
#endif

//from https://brennan.io/2015/01/16/write-a-shell-in-c/
#define CMD_DELIM " \t\r\n\a" 

void sh_loop();
char* sh_readline();
struct cmd* sh_processline(char* line);
int sh_execcmd(struct cmd* currCmd, int* status);
int launch(struct cmd* currCmd);

struct cmd {
    char** sh_argv;
    int sh_argc;
    char* input;
    char* output;
    int bg_task;
};

int main(int argc, char* argv[]) {

    // shell body loop
    sh_loop();

    return EXIT_SUCCESS;
}

void sh_loop() {
    int status = 0;
    int running = 1;
    char* line;
    struct cmd* currCmd;

    do {
        printf(": ");

        line = sh_readline();
        //checking for blank lines and comments
        if(strlen(line) > 0 && line[0] != '#') {
            currCmd = sh_processline(line);

            //debug print cmd
            if(DEBUG) {
                printf("argc: %d\nargv: ", currCmd->sh_argc);
                for(int i = 0; i < currCmd->sh_argc; i++) {
                    printf("%s ", currCmd->sh_argv[i]);
                }
                printf("\n");
                if(currCmd->input != NULL) printf("input: %s\n", currCmd->input);
                if(currCmd->output != NULL) printf("output: %s\n", currCmd->output);
                printf("bg_task: %d\n", currCmd->bg_task);
            }

            running = sh_execcmd(currCmd, &status);

            free(currCmd->sh_argv);
            free(currCmd);
        }        
        free(line);
    } while (running);
}

//executing non built in cmd
int launch(struct cmd* currCmd) {
    pid_t pid, wid;
    int s = 0;
    int fd;

    pid = fork();
    switch(pid) {
    case 0: //child
        //input output redirections
        if(currCmd->input != NULL) { 
            if((fd = open(currCmd->input, O_RDONLY)) == -1) {
                printf("cannot open %s for input\n", currCmd->input); //file opening failed
                s = 1;
                exit(EXIT_FAILURE);
            } else {
                if(dup2(fd, 0) == -1) {
                    perror("input"); //file redirection failed
                    s = 1;
                    exit(EXIT_FAILURE);
                }
            }
        }

        if(currCmd->output != NULL) { 
            if((fd = open(currCmd->output, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1) {
                printf("cannot open %s for input\n", currCmd->output); //file opening failed
                s = 1;
                exit(EXIT_FAILURE);
            } else {
                if(dup2(fd, 1) == -1) {
                    perror("output"); //file redirection failed
                    s = 1;
                    exit(EXIT_FAILURE);
                }
            }
        }

        if(execvp(currCmd->sh_argv[0], currCmd->sh_argv) == -1) {
            perror("Error");
            fflush(stdout);
            exit(EXIT_FAILURE);
        }
        break;
    case -1: //stinky while forking
        perror("Error");
        fflush(stdout);
        break;
    default: //parent, wait for child to be done
        wid = waitpid(pid, &s, 0);
        fflush(stdout);
        break;
    }
    if(s != 0) s = 1;
    return s;
}

//executing built in commands or calling launch
int sh_execcmd(struct cmd* currCmd, int* status) {

    //look for built in cmds
    if(strcmp(currCmd->sh_argv[0], "status") == 0) {
        printf("exit value %d\n", *status);
        return 1;
    } else if(strcmp(currCmd->sh_argv[0], "exit") == 0) {
        return 0;
    } else if(strcmp(currCmd->sh_argv[0], "cd") == 0) {

        if(currCmd->sh_argc == 1) {
            chdir(getenv("HOME"));
        } else {
            if(chdir(currCmd->sh_argv[1]) != 0) {
                perror("Error");
                fflush(stdout);
            }
        }

        return 1;
    } else {
        // else execute command w/ exec
        *status = launch(currCmd);
        return 1;
    }
}

//processing command
struct cmd* sh_processline(char* line) {
    struct cmd* tempCmd = malloc(sizeof(struct cmd));

    tempCmd->input = NULL;
    tempCmd->output = NULL;
    int buffsize = MAX_LEN;

    tempCmd->sh_argv = malloc(sizeof(char*) * buffsize);

    char* saveptr; //for strtok_r
    char* token;
    tempCmd->sh_argc = 0;

    //processing line
    token = strtok_r(line, CMD_DELIM, &saveptr);
    while(token != NULL) {
        if(strcmp(token, "<") == 0) {                           //input redirection
            tempCmd->input = strtok_r(NULL, CMD_DELIM, &saveptr);
        } else if(strcmp(token, ">") == 0) {                    //output redirection
            tempCmd->output = strtok_r(NULL, CMD_DELIM, &saveptr);
        } else {
            tempCmd->sh_argv[tempCmd->sh_argc++] = token;

            // num args exceeded buffer size, realloc
            if(tempCmd->sh_argc > buffsize) {
                buffsize += MAX_LEN;
                tempCmd->sh_argv = realloc(tempCmd->sh_argv, buffsize);
            }
        }
        token = strtok_r(NULL, CMD_DELIM, &saveptr);
    }

    //checking for &
    if(strcmp(tempCmd->sh_argv[tempCmd->sh_argc-1], "&") == 0) {
        tempCmd->bg_task = 1;
        tempCmd->sh_argc--;
    } else {
        tempCmd->bg_task = 0;
    }

    tempCmd->sh_argv[tempCmd->sh_argc] = NULL;

    return tempCmd;
}

//reading line from std_in
char* sh_readline() {
    //dynamic buffer we can read stdin into, if we need more space we'll realloc later
    int buffsize = MAX_LEN;
    char* buffer = malloc(sizeof(char) * MAX_LEN);
    int len = 0;
    int c;

    char* pid = malloc(sizeof(char) * floor(log10(getpid()) + 1) + 1); //malloc strlen of pid, floor(log10(getpid()) + 1) gets strlen of int, another +1 for null terminator.
    sprintf(pid, "%d", getpid());

    //reading char from stdin into c, if \n or EOF then stop reading
    c = getchar();
    while(c != '\n' && c != EOF) {
        //variable expansion is done here
        if(c == '$') {
            if((c = getchar()) == '$'){
                //$$ found, replacing it with process id.
                for(int i = 0; i < strlen(pid); i++) {
                    buffer[len++] = pid[i];
                    if(len >= buffsize) { //pid str exceeded buffer size, need to realloc
                        buffsize += MAX_LEN;
                        buffer = realloc(buffer, buffsize);
                    }
                }
                c = getchar();
            } else {
                buffer[len++] = '$';
            }
        } else {
            buffer[len++] = (char) c;
            c = getchar();
        }
        

        //uh oh input exceeded buffer size, time to realloc
        if(len >= buffsize) {
            buffsize += MAX_LEN;
            buffer = realloc(buffer, buffsize);
        }
    }
    //null terminate string
    buffer[len] = '\0';
    free(pid);

    return buffer;
}