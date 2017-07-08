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
#include <sys/stat.h>
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
int cmd_cd(struct tokens *tokens);
/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_pwd, "pwd", "prints current directory to stdout"},
  {cmd_cd, "cd", "takes one argument, a directory path, and change the current directory to that directory"}
};

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

/*Prints current directory*/
int cmd_pwd(unused struct tokens *tokens){
    char cwd[1020];
    if (getcwd(cwd,sizeof(cwd))!=NULL){
	fprintf(stdout,"%s\n",cwd);
	return 1;
    }else{
	perror("Error pwd");
	return 0;
	}
	
	
}

/*Changes current directory to directory taken by argument */
int cmd_cd(unused struct tokens *tokens){
   char buffer[1020];
   char cwd[1020];
   if (tokens_get_token(tokens,1)==NULL){
        chdir(getenv("HOME"));
        return 0;
   }
   strcpy(buffer,tokens_get_token(tokens,1));
   if(buffer[0]!='/'){
	getcwd(cwd,sizeof(cwd));
	strcat(cwd,"/");
	strcat(cwd,buffer);
        chdir(cwd);
   }else{
	chdir(buffer);
   
   }
	return 0;
	
	 
}


/* Runs line commands */
void line_commands(unused struct tokens *tokens){
  int status;
  int i;
  char *args[10]={NULL};
  char *sup_args[5]={NULL};
  char *temp;
  // Gives all path environments 
  char *paths=getenv("PATH");
  char *ptn;
  int j=0;
  for (i=0;(temp=tokens_get_token(tokens,i))!=NULL;++i){
        if (strcmp(temp,"<")==0 || strcmp(temp,">")==0){
		sup_args[j]=temp;
                if (tokens_get_token(tokens,i+1)!=NULL){
                	sup_args[j+1]=tokens_get_token(tokens,i+1);
                        ++j;
       			++i;
                }else{
   			fprintf(stderr,"No input or output file specified\n");
			exit(0);
                }
                ++j;
        } else{
        	args[i]=temp;
        }
        if (tokens_get_token(tokens,i+1)==NULL){
                args[i+1]=NULL;
        }
  }	
  pid_t pid=fork();
  if (pid==-1){
        perror("fork");
  }
  if (pid!=0){
        wait(&status);
  }else{
  // Separate out intput path string in to strings by delimeter ":"
        if(args[0]==NULL){
	       exit(1);
        }
        pid_t cpid=getpid();
        setpgid(cpid,cpid);
        tcsetpgrp(0,cpid);
        ptn=strtok(paths,":");
        while(ptn!=NULL){
                char buff1[100];
                char buff2[100];
                strcpy(buff1,ptn);
                strcpy(buff2,args[0]);
                strcat(buff1,"/");
                strcat(buff1,buff2);
                // Checks if the path exists or is accesable 
               	if (access(buff1,X_OK)==0){
               		strcpy(args[0],buff1);
                	break;
                }
                ptn=strtok(NULL,":");
         }
         if(sup_args[0]!=NULL){
		int fd;
                if (strcmp(sup_args[0],"<")==0){
                	if((fd=open(sup_args[1],O_RDONLY,0644))<0){
				fprintf(stderr,"%s\n",sup_args[1]);
			}
        
                	dup2(fd,0);
                        close(fd);
                }else{
                	if((fd=open(sup_args[1],O_CREAT | O_WRONLY, 0644))<0){
				perror(sup_args[1]); /*open failed*/
			}
                	dup2(fd,1);
                        close(fd);
        	}

        }
         //execute command line  with path appened to the original command
         if(execv(args[0],args)<0){
		exit(1);
	}
         exit(1);
   }
}

/*Signal Handling Implementation part 6 */

/* Signal handling of SIGTIN */
void signal_handlerint(int sig){
    signal(sig,SIG_IGN);
    pid_t current_pid=getpid();
    	
    if (current_pid !=getppid()){
	kill(( getpgid( current_pid), SIGINT);
    }

} 

/*Signal hadnling of SIGTSTP*/

void signal_handlerstp(int sig){
   pid_t current_pid=getpid();
   signal(sig,SIG_IGN);
   if (current_pid != getppid()){
      printf("Parent coming through: %d",current_pid);
      kill((pid_t) getpgid(current_pid), SIG_DFL);
   }

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

int main(unused int argc, unused char *argv[]) {
  init_shell();
  static char line[4096];
  int line_num = 0;
  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);
  signal(SIGINT,signal_handlerint);
  signal(SIGTSTP,signal_handlerstp);
  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));
      
    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      /* REPLACE this to run commands as programs. */
      line_commands(tokens);
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
