 #include <fcntl.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util.h"
#include "history.h"
#include "logger.h"
#include "ui.h"

int command_loop();

// boolean representing last command's status code
bool statusCode = true;
bool interactive = true;
int numCommands = 0;
bool executing = false;

int MAX_ARGS = 4096; /* POSIX compliant max arg size */

struct command_line {
    char **tokens;
    bool stdout_pipe;
    char *stdout_file;
};

void sigint_handler(int signo) {
    printf("\n");
    if(!executing) read_command();
    fflush(stdout);
}

bool getStatus() {
    return statusCode;
}

// allocate initial memory for a list of 128 command_line structs. 
// TODO: allocate dynamically; malloc one ptr, then realloc as needed
struct command_line* create_commands() {
    struct command_line *commands = malloc(sizeof(struct command_line*) * 128);
    // allocate initial memory for a list of
    for(int i = 0; i < 128; i++) {
        commands[i].tokens = malloc(sizeof(char) * 129); // inital command line capacity of 128 ch
        commands[i].stdout_pipe = false;
        commands[i].stdout_file = NULL;
    }
    return commands; 
}

int free_commands(struct command_line *commands) {
    for(int i = 0; i < 128; i++) {
       // free(commands[i].tokens);
       // free(commands[i].tokens);
    }
    free(commands);
    return 0;
}

int tokenize(char* command, struct command_line* commands) {
    char *next_tok = command;
    char *curr_tok;
    int index = 0;
    while((curr_tok = next_token(&next_tok, " \t")) != NULL) {
        // check for special characters: | and >
        if(strcmp(curr_tok, "|") == 0) {
            // skip this token, update pipe info, increment command number
            commands[numCommands].stdout_pipe = true;
            commands[numCommands].tokens[index] = NULL; // null terminate
            numCommands++;
            index = 0;
        }
        else if(strcmp(curr_tok, ">") == 0) {
            // set the next argument to the output file
            curr_tok = next_token(&next_tok, " \t");
            commands[numCommands].stdout_file = curr_tok;
        }
        else {
            // add token to command
            LOGP("adding token to command!\n");
            commands[numCommands].tokens[index++] = curr_tok;
        }
    }
    commands[numCommands].tokens[index] = NULL; // null terminate
    if(numCommands == 0 && index != 0) numCommands = 1;
    return 0;
}

int change_directory(struct command_line* commands) {
    if(commands[0].tokens[1] == NULL) {
        // cd with no args will cd to home
        char username[80];
        getUsername(username, 79);
        char home[128];
        strcpy(home, "/home/");
        strcat(home, username);
        chdir(home);
    }
    else {
        if(chdir(commands[0].tokens[1]) != 0) {
            if(interactive) {
                // printf("chdir: no such file or directory: %s\n", args[1]);
            }
            statusCode = false;
            return -1;
        }
    }
    return 0;
}

void redirect(int in, int out, struct command_line *command)
{
     pid_t pid = fork();
     if(pid == 0) {
          // i am the child!
          if(in != 0) {
               dup2(in, 0);
               close(in);
          }
          if(out != 1) {
               dup2(out, 1);
               close(out);
          }
          int success = execvp(command->tokens[0], command->tokens);
          if(success == -1) close(STDIN_FILENO);
     }
     else {
          // i am the parent!
          wait(&pid);
     }
}

int execute_pipeline(struct command_line *cmds)
{
     executing = true;
     int input = 0; // variable to track input fd; the first process gets input from fd 0.
     struct command_line* command = cmds; // varibale to track current command; starts as ptr to first cmd.

     while(command->stdout_pipe == true) {
          // create a pipe!
          int fd[2];
          if(pipe(fd) == -1) {
               perror("error creating pipe\n");
          }

          redirect(input, fd[1], command);
          close(fd[1]); // the child will write to this
          input = fd[0]; // the next child will read from this
          command++;
     }

     // otherwise, we reached the last command!
     // set stdin as the read end of pipe
     if(input != 0) {
         dup2(input, 0);
     }

     // determine where to output the final results, and exec the command
     if(command->stdout_file == NULL) {
          // write to terminal
          LOGP("executing command!\n");
          int success = execvp(command->tokens[0], command->tokens);
          if(success == -1) {
              return -1;
          }
     }
     else {
          // redirect output and write final results to the file
          freopen(command->stdout_file, "a+", stdout);
          execvp(command->tokens[0], command->tokens);
     }

     return 0;
}

int execute_command(struct command_line* commands) {
    pid_t child = fork();
    if (child == 0) { // i am the child!
        int success = execute_pipeline(commands);
        if(success == -1) {
            if(interactive) {
                // printf("sheesh: no such file or directory: %s\n", command);
            }
            statusCode = false;
        }
        return -1;
    }
    else if(child == -1) {
        perror("fork error");
        return -1;
    }
    else { // i am the parent!
        int status;
        waitpid(child, &status, 0);
        executing = false;
        if(status != 0) {
            statusCode = false;
            return -1;
        }
    }
    return 0;
}

int check_built_ins(struct command_line *commands) {
    char **builtIns = (char *[]) { "exit", "cd", "jobs", "history" };
 
        int commandNum = 0;
        for(int i = 0; i < 4; i++) {
            if(strcmp(commands[0].tokens[0], builtIns[i]) == 0) {
                commandNum = i + 1;
                break;
            }
        }

        switch(commandNum) {
        case 1: // exit the terminal
            {
            return -1;
            }
        case 2: // change directories
            {
            int success = change_directory(commands);
            if(success == -1) {
                statusCode = false;
            }
            break;
            }
        default:
            {
            int success = execute_command(commands);
            if(success == -1) {
                close(STDIN_FILENO);
                statusCode = false;
                return -1;
            }
            }
        }
    return 0;
}

int command_loop() {
    // first things first, get the command line
    bool running = true;
    while (running) {
        executing = false;
        numCommands = 0;
        char *command = read_command(); // command line as a string
        
        if(command == NULL) {
            // reached the last command in scripting mode
            LOGP("reached end of command\n");
            return -1;
        }
        
        // remove comments before parsing command
        char *ptr = strstr(command, "#");
        if(ptr != NULL) {
            while(*ptr != NULL) {
                // remove the current char and increment char
                *(ptr++) = 0;
            }
        }
        
        // reset the status code after reading the previous one!
        statusCode = true;
        
        // list of commands (arg arrays)
        struct command_line *commands = create_commands();
        
        // tokenize the command, and update command list
        tokenize(command, commands);

        // check built-in commands
        if(numCommands != 0) {
            if(check_built_ins(commands) == -1) {
                return -1;
            }
        }
        
        free(command);
        free_commands(commands);
    }
    return 0;
}

int main(void)
{
    init_ui();

    // set up signal handler. SIGINT is sent via Ctrl+C
    signal(SIGINT, sigint_handler);


        // initalize dynamically allocated argument list
        // char **args = malloc(sizeof(char*) * 128); // array of char pointers; init capacity of 128
        // for(int i = 0; i < 128; ++i) {
        //    args[i] = (char*) malloc(256 + 1); // commands can be max  256 chars
        // }

        // int numArgs = 0;

    command_loop(); // printf("exiting shell...\n");

    // free dynamically allocated memory
    // for(int i = 0; i < numCommands; i++) {
    //    free(args[i]);
    // }
    // free(args);
    // free_commands();

    return 0;
}
