#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <linux/limits.h>

#include <fcntl.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

int numBack = 0;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_wait(struct tokens *tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc
{
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_pwd, "pwd", "prints the current working directory"},
    {cmd_cd, "cd", "changes the current working directory"},
    {cmd_wait, "wait", "waits for all background processes"},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens)
{
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens)
{
  exit(0);
}

/* Prints the current working directory */
int cmd_pwd(struct tokens *tokens)
{
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != NULL)
  {
    printf("%s\n", cwd);
  }
  else
  {
    perror("error");
  }
  return 1;
}

/* Changes the current working directory */
int cmd_cd(struct tokens *tokens)
{
  if (tokens_get_length(tokens) > 1)
  {
    chdir(tokens_get_token(tokens, 1));
  }
  return 1;
}

int cmd_wait(struct tokens *tokens)
{
  fprintf(stdout, "waiting for background processes\n");
  for (int i = numBack - 1; i >= 0; i--)
  {
    int status;
    wait(&status);
    numBack = i;
  }
  fprintf(stdout, "all background processes finished\n");

  return 1;
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[])
{
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell()
{
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive)
  {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

char *getDirectory(char *name)
{
  if (access(name, F_OK) != -1)
  {
    //directory given :)
    return name;
  }
  char *path = getenv("PATH");
  char *delim = ":";
  char *saveptr;
  char *slash = "/";
  for (;; path = NULL)
  {
    char *cur = malloc(PATH_MAX);
    char *token = strtok_r(path, delim, &saveptr);
    if (token == NULL)
      return NULL;
    strcat(cur, token);
    strcat(cur, slash);
    strcat(cur, name);
    if (access(cur, F_OK) != -1)
    {
      // file exists
      return cur;
    }
  }
  return NULL;
}

int main(unused int argc, unused char *argv[])
{
  init_shell();
  static char line[4096];
  int line_num = 0;
  signal(SIGTTOU, SIG_IGN);
  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin))
  {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0)
    {
      cmd_table[fundex].fun(tokens);
    }
    else
    {
      size_t num_params = tokens_get_length(tokens);
      char *last = tokens_get_token(tokens, num_params - 1);
      bool inBack = false;
      if (last[0] == '&')
      {
        inBack = true;
        num_params--;
      }
      pid_t pid = fork();

      if (pid == 0)
      { //new process
        setpgid(getpid(), getpid());
        char *params[num_params];
        int curI = 0;
        for (int i = 0; i < num_params; i++)
        {
          char *curStr = tokens_get_token(tokens, i);
          if (curStr[0] == '>')
          {

            int openW = open(tokens_get_token(tokens, ++i), O_WRONLY | O_CREAT | O_APPEND, 0666);
            if (openW != -1)
            {
              dup2(openW, STDOUT_FILENO);
              close(openW);
            }
          }
          else if (curStr[0] == '<')
          {

            int openR = open(tokens_get_token(tokens, ++i), O_RDONLY | O_APPEND);
            if (openR != -1)
            {
              dup2(openR, STDIN_FILENO);
              close(openR);
            }
          }
          else
          {
            params[curI] = curStr;
            curI++;
          }
        }
        params[curI] = NULL;
        char *name = getDirectory(tokens_get_token(tokens, 0));
        if (!name)
        {
          printf("\ncan't find program\n");
          exit(0);
        }
        execv(name, params);
        exit(0);
      }
      else
      {
        // old process
        if (!inBack)
        {
          int status;
          tcsetpgrp(STDIN_FILENO, pid);
          waitpid(pid, &status, WUNTRACED);
          tcsetpgrp(STDIN_FILENO, getpid());
        }
        else
        {
          numBack++;
        }
      }
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
