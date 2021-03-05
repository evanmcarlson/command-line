#include <stdio.h>
#include <readline/readline.h>
#include <locale.h>
#include <pwd.h>
#include <unistd.h>

#include "history.h"
#include "logger.h"
#include "ui.h"
#include "shell.h"
#include "util.h"

/* -- Private function forward declarations -- */
static int readline_init(void);
static int key_up(int count, int key);
static int key_down(int count, int key);
static char **command_completion(const char *text, int start, int end);
static char *command_generator(const char *text, int state);


static char prompt_str1[80] = "╭-[%s]-[%d]-[%s@%s:%s]";
static char prompt_str2[80] = "╰─• ";

// the count of commands
static int count = 1;

void init_ui(void)
{
    LOGP("Initializing UI...\n");

    char *locale = setlocale(LC_ALL, "en_US.UTF-8");
    LOG("Setting locale: %s\n",
            (locale != NULL) ? locale : "could not set locale!");

    rl_startup_hook = readline_init;
}

int getUsername(char *username, int max)
{
  uid_t uid = geteuid();
  struct passwd *pw = getpwuid(uid);
  if (pw)
  {
    strcpy(username, pw->pw_name);
  }

  return 0;
}

char *prompt_line1(void) {
    // set status code of last command
    char emoji[5];
    bool statusCode = getStatus();

    if(statusCode == true) {
        // the last command was successful! set happy emoji
        emoji[0] = 0xF0;
        emoji[1] = 0x9F;
        emoji[2] = 0x98;
        emoji[3] = 0x84;
        emoji[4] = '\0';
    }
    else {
        // otherwise, set bad emoji
        emoji[0] = 0xF0;
        emoji[1] = 0x9F;
        emoji[2] = 0xA4;
        emoji[3] = 0xAE;
        emoji[4] = '\0';
    }

    // finally, get username and cwd
    char username[80];
    getUsername(username, 79);

    char hostname[80];
    gethostname(hostname, 79); // check status code !!

    char cwd[256];
    getcwd(cwd, 255);

    // if cwd starts with the home directory, give path alias ~
    char *checker = NULL;
    char home[128];
    sprintf(home, "/home/%s", username);

    checker = strstr(cwd, home);
    if(checker == cwd) {
        // make a copy of the cwd
        char cpy[128];
        strcpy(cpy, cwd);
        // overwrite cwd with tilda!
        strncpy(cwd, "~", sizeof(cwd));
        // append the remaining location if there is one
        if(strlen(cpy) > strlen(home)) {
            strcat(cwd, cpy + strlen(home));
        }
    }

    // where are we freeing this?
    char *buf = malloc(sizeof(char) * 256);
    sprintf(buf, prompt_str1, emoji, count++, username, hostname, cwd);
    return buf;
}

char *prompt_line2(void) {
    return prompt_str2;
}

char *read_command(void)
{
    if(isatty(STDIN_FILENO)) {
        // interactive mode; print to terminal
        char *prompt = prompt_line1();
        puts(prompt);
        return readline(prompt_line2());
    }
    else {
        // scripting mode
        char *line = malloc(sizeof(char) * 256);
        size_t line_sz;
        ssize_t read = getline(&line, &line_sz, stdin);
        if(read == -1) {
            // end of input stream
            return NULL;
        }
        size_t newline = strcspn(line, "\n");
        line[newline] = '\0';
        return line;
    }
}

int readline_init(void)
{
    rl_bind_keyseq("\\e[A", key_up);
    rl_bind_keyseq("\\e[B", key_down);
    rl_variable_bind("show-all-if-ambiguous", "on");
    rl_variable_bind("colored-completion-prefix", "on");
    rl_attempted_completion_function = command_completion;
    return 0;
}

int key_up(int count, int key)
{
    /* Modify the command entry text: */
    rl_replace_line("User pressed 'up' key", 1);

    /* Move the cursor to the end of the line: */
    rl_point = rl_end;

    // TODO: reverse history search

    return 0;
}

int key_down(int count, int key)
{
    /* Modify the command entry text: */
    rl_replace_line("User pressed 'down' key", 1);

    /* Move the cursor to the end of the line: */
    rl_point = rl_end;
    // TODO: forward history search

    return 0;
}

char **command_completion(const char *text, int start, int end)
{
    /* Tell readline that if we don't find a suitable completion, it should fall
     * back on its built-in filename completion. */
    rl_attempted_completion_over = 0;

    return rl_completion_matches(text, command_generator);
}

/**
 * This function is called repeatedly by the readline library to build a list of
 * possible completions. It returns one match per function call. Once there are
 * no more completions available, it returns NULL.
 */
char *command_generator(const char *text, int state)
{
    // TODO: find potential matching completions for 'text.' If you need to
    // initialize any data structures, state will be set to '0' the first time
    // this function is called. You will likely need to maintain static/global
    // variables to track where you are in the search so that you don't start
    // over from the beginning.

    return NULL;
}
