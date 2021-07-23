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
#include <fcntl.h>
 

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens * tokens);
int cmd_wait(struct tokens* tokens);
//int backProceNum = 0;
/*Runs a new command as a program*/

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun; // function
  char *cmd; // fucntion name
  char *doc; // function description
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_pwd, "pwd", "get the full path of the current directory"},
  {cmd_cd,"cd","go the the give directory"},
  {cmd_wait,"wait","waits until all backgroud proccesses are finished"}
};
/*When give a path , enters the directory defined in the give path*/
int  cmd_cd(unused struct tokens* tokens){
  if(tokens_get_length(tokens) >2){
    perror("cd Can`t take more than one argument");
    return -1;
  }
  char * newDirectory = tokens_get_token(tokens,1);

  if(newDirectory == NULL || strcmp(newDirectory,"~") == 0){
    newDirectory = getenv("HOME");
  }
  int stat = chdir(newDirectory);
  if(stat == -1){
    perror("Given path doesn`t exist");
    return -1;
  }
  return 0;
}
int cmd_wait(unused struct tokens* tokens){
  //int status;
  pid_t procId = wait(NULL);
  // for(int i=0; i< backProceNum; i++){
  //   printf("proc with id:%d terminated\n",wait(&status));
  // }
  while(procId>0){
    //printf("proc with id:%d terminated\n",procId);
    procId = wait(NULL);
  }
  return 0 ;
}


/*Get the full path of the current directory*/
int cmd_pwd(unused struct tokens *tokens){
  if(getcwd(NULL,0)!=NULL){
    printf("%s\n",getcwd(NULL,0));
  }else{
    perror("for some reason Can`t get the director path");
  }
  return 0;
}

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
  exit(0);
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
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
int runCmd(struct tokens * tokens){
   size_t tokLen = tokens_get_length(tokens);
      char *args[tokLen+1];
      args[tokLen] = NULL;
      for(int i =0; i<tokLen; i++){

        args[i]=tokens_get_token(tokens,i);
        if(strcmp(args[i],">") == 0 || strcmp(args[i],"<") ==0 || strcmp(args[i],"&")==0){
          args[i]= NULL;
          break;
        }
      }
      
      if(access(tokens_get_token(tokens,0),F_OK)== 0)
      execv(tokens_get_token(tokens,0),args);

      // if(closeOut == 1){
      //   fclose(stdout);
      // }
      // if(closeIn ==1){
      //   fclose(stdin);
      // }
      return -1;
}

int redirect(struct tokens* tokens){
  size_t tokLen = tokens_get_length(tokens);
  
  for(int i=0 ;i< tokLen-1; i++){
    char * currtok = tokens_get_token(tokens,i);
    if(strcmp(currtok,">") == 0){
      char * filename = tokens_get_token(tokens,i+1);
      FILE *fout ;
      fout =freopen(filename,"w+",stdout);
      if(fout == NULL)return -1;
      
    }else if(strcmp(currtok,"<") == 0){
      char * filename = tokens_get_token(tokens,i+1);
      FILE *fin;
      fin = freopen(filename,"r",stdin);
      if(fin == NULL)return -1;
      
    }
  }
  return 0;
}
void signalDefHandle(){
  signal(SIGINT,SIG_DFL);
  signal(SIGTSTP,SIG_DFL);
  // signal(SIGQUIT,SIG_DFL);
  // signal(SIGKILL,SIG_DFL);
  // signal(SIGTERM,SIG_DFL);
  // signal(SIGTSTP,SIG_DFL);
  // signal(SIGCONT,SIG_DFL);
  // signal(SIGTTIN,SIG_DFL);
  signal(SIGTTOU,SIG_DFL);
}
void signalHandleIgnore(){
  signal(SIGINT,SIG_IGN);
  signal(SIGTSTP,SIG_IGN);
  //   signal(SIGQUIT,SIG_IGN);
  //  signal(SIGKILL,SIG_IGN);
  //  signal(SIGTERM,SIG_IGN);
  //  signal(SIGTSTP,SIG_IGN);
  //  signal(SIGCONT,SIG_IGN);
  //  signal(SIGTTIN,SIG_IGN);
   signal(SIGTTOU,SIG_IGN);
}
int execCommand(unused struct tokens * tokens){
  if(tokens_get_length(tokens) == 0) return -1;
  int amper =-1;
  if(strcmp(tokens_get_token(tokens,tokens_get_length(tokens)-1),"&") == 0){
    amper=1;
  }
  pid_t cpid;
  cpid = fork();
  //int success;
  if(cpid==0){
    signalDefHandle();
    setpgid(0,getpid());
    
    if(redirect(tokens)!= 0){
      perror("Cannot execute program");
      exit(EXIT_FAILURE);
    }

    if(tokens_get_token(tokens,0)[0]=='/'){
      runCmd(tokens);
      perror("Command doesn`t exist");
      exit(EXIT_FAILURE);
    }else if(access(tokens_get_token(tokens,0),F_OK) ==0){
        runCmd(tokens);
        perror("Can`t execute given executable");
        exit(EXIT_FAILURE);
    }
    else{
      char * Path = getenv("PATH");
      char * myPath = strdup(Path);
      char * currTok;
      currTok = strtok(myPath,":");
      while(currTok != NULL){
        char * tmp = malloc(4096);
        
        strcpy(tmp,currTok);

        strcat(tmp,"/");
        for(int i =0 ;i < tokens_get_length(tokens); i++){
          strcat(tmp,tokens_get_token(tokens,i));
          strcat(tmp," ");
        }
        struct tokens * tempTok = tokenize(tmp);
        runCmd(tempTok);
        currTok=strtok(NULL,":");
        free(tmp);
      }
      free(myPath);
      perror("Command doesn`t exist");
      exit(EXIT_FAILURE);
    }
  } else if(cpid >0){
    if(amper != 1){
        signalHandleIgnore();
        setpgid(cpid,cpid);
        tcsetpgrp(0,cpid);
        
        waitpid(cpid,NULL,WUNTRACED);
        tcsetpgrp(0,getpgrp());
    }
  }else {
    perror("can`t start a new proccess"); // can`t fork
  } 
  return -1;
}


int main(unused int argc, unused char *argv[]) {
  init_shell();
  signalHandleIgnore();
  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      /* REPLACE this to run commands as programs. */
      //struct tokens * pathToken = tokenize(getenv("PATH"))
      execCommand(tokens);
    }
    
    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    //cmd_wait(tokens);
    /* Clean up memory */
    tokens_destroy(tokens);
  }


  return 0;
}
