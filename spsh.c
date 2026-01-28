/** libs and config **/

#include <dirent.h>
#include <fcntl.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>

#define PATH_MAX 4096

const char *ENV_VARIABLE = "PATH";

/** prototypes **/

int shlRun(char** args);

/** builtin declarations **/

int shlExit(char **args);
int shlEcho(char **args);
int shlHelp(char **args);
int shlType(char** args);
int shlPwd(char** args);
int shlCd(char** args);
int shlHistory(char** args);

/** builtin commands **/

static int appendidx = 0;

char *builtinStr[] = {
    "exit",
    "echo",
    "help",
    "type",
    "pwd",
    "cd",
    "history"
};

char *autocompleteStr[] = {
    "exit",
    "echo",
    "help",
    "type",
    "pwd",
    "cd",
    "history"
};

int (*builtinFunc[]) (char **) = {
    &shlExit,
    &shlEcho,
    &shlHelp,
    &shlType,
    &shlPwd,
    &shlCd,
    &shlHistory
};

int shlNumBuiltin(){
    return sizeof(builtinStr) / sizeof(char *);
}

/** builtin function implementation **/

int shlExit(char **args){
    char *history_file = getenv("HISTFILE");
    append_history(history_length - appendidx, history_file);
    return 0;
}

int shlEcho(char **args){
   
  for (int i = 1; args[i]; i++) {
        if (i > 1)
            putchar(' ');
        fputs(args[i], stdout);
    }
    putchar('\n');
    return 1;

}

int shlHelp(char **args){
    int i;
    printf("SP Shell\n");
    printf("Enter program names and arguments, and press enter\n");
    printf("The following functions are built in:\n");

    for(i = 0; i < shlNumBuiltin(); i++){
        printf("  %s\n", builtinStr[i]);
    }

    printf("Use the 'man' command for information on other programs\n");
    return 1;
}


int shlType(char** args){

  // builtin function //

  for(int i = 0; i < shlNumBuiltin(); i++){
        if(strcmp(args[1], builtinStr[i]) == 0){
            printf("%s is a shell builtin\n", args[1]);
            return 1;
        }
    }

  // executable file //

  char *env_p = getenv(ENV_VARIABLE);

  if (getenv != NULL) {
    char *copy = strdup(env_p);
    char *token = strtok(copy, ":");
    while (token) {
      DIR *dir = opendir(token);
      if (dir) {
        struct dirent *entry;
        while (entry = readdir(dir)) {
          if (entry->d_name[0] == '.')
            continue;
          if (strcmp(entry->d_name, args[1]) == 0) {
            char *full_path = malloc(strlen(args[1]) + strlen(token) + 2);
            strcpy(full_path, token);
            strcat(full_path, "/");
            strcat(full_path, entry->d_name);

            if (access(full_path, X_OK) == 0) {
              printf("%s is %s\n", args[1], full_path);
              free(copy);
              return 1;
            }
          }
        }
        closedir(dir);
      }
      token = strtok(NULL, ":");
    }
    free(copy);
  }

    printf("%s: not found\n", args[1]);
    return 1;
}

int shlPwd(char** args){
    char cwd[1024];
    if(getcwd(cwd, sizeof(cwd)) != NULL){
        printf("%s\n", cwd);
    }else{
        perror("getcwd() error");
    }
    return 1;
}

int shlCd(char** args){
    char* dir;

    if(args[2] != NULL){
        printf("cd: too many arguments\n");
        return 1;
    }else if(args[1] == NULL || strcmp(args[1], "~") == 0){
        dir = getenv("HOME");
        if(dir == NULL){
            printf("cd: error setting to home directory\n");
            return 1;
        }
        
    }
    else dir = args[1];

    if(chdir(dir) != 0) printf("cd: %s: No such file or directory\n", dir);

    return 1;
}

int shlHistory(char **args) {
    int argc = 0;

    HIST_ENTRY **list = history_list();
    if (!list) return 1;

    int numEntries = 0;
    while (list[numEntries]) numEntries++;
    int newEntries = numEntries - appendidx;

    while(args[argc]) argc++;
    if (args[1] != NULL && strcmp(args[1], "-r") == 0) read_history(args[2]);
    if (args[1] != NULL && strcmp(args[1], "-w") == 0) write_history(args[2]);
    if (args[1] != NULL && strcmp(args[1], "-a") == 0){
        append_history(newEntries, args[2]);
        appendidx = history_length;
    }

    int n = numEntries;
    if (args[1] != NULL) {
        n = atoi(args[1]);
        if (n < 0) return 1;
        if (n > numEntries) n = numEntries;
    }

    int start = numEntries - n;

    for (int i = start; i < numEntries; i++) {
        printf("    %d  %s\n", i + 1, list[i]->line);
    }
    return 1;
}

/** utilities **/

int shlRedir(char **args) {
    for(int i = 0; args[i] != NULL; i++) {
        int target = -1;
        int append = 0;

        if(strcmp(args[i], ">") == 0) {
            target = STDOUT_FILENO;
        } else if(strcmp(args[i], ">>") == 0) {
            target = STDOUT_FILENO;
            append = 1;
        } else if(strcmp(args[i], "2>") == 0) {
            target = STDERR_FILENO;
        } else if(args[i][0] >= '0' && args[i][0] <= '9' && args[i][1] == '>' && args[i][2] == '\0') {
            target = args[i][0] - '0';
        } else if(args[i][0] >= '0' && args[i][0] <= '9' && args[i][1] == '>' && args[i][2] == '>' && args[i][3] == '\0') {
            target = args[i][0] - '0';
            append = 1;
        }

        if(target != -1) {
            if(args[i + 1] == NULL) {
                fprintf(stderr, "syntax error: expected filename after redirection\n");
                return -1;
            }

            int fd;
            if(append)
                fd = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
            else
                fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if(fd < 0) {
                perror(args[i + 1]);
                return -1;
            }

            if(dup2(fd, target) < 0) {
                perror("dup2");
                close(fd);
                return -1;
            }
            close(fd);

            int j;
            for(j = i; args[j + 2] != NULL; j++) {
                args[j] = args[j + 2];
            }
            args[j] = NULL;
            i--;
        }
    }
    return 0;
}

int shlExecutePipeline(char ***cmds, int ncmds) {
    int prev_fd = -1;
    pid_t pids[ncmds];

    for (int i = 0; i < ncmds; i++) {
        int pipefd[2];

        if (i < ncmds - 1) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                return 1;
            }
        }

        pid_t pid = fork();
        if (pid == 0) {
            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }

            if (i < ncmds - 1) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            shlRun(cmds[i]);
            exit(0);
        }

        pids[i] = pid;

        if (prev_fd != -1) close(prev_fd);

        if (i < ncmds - 1) {
            close(pipefd[1]);
            prev_fd = pipefd[0];
        }
    }

    for (int i = 0; i < ncmds; i++) waitpid(pids[i], NULL, 0);

    return 1;
}

/** autocompletion **/

char *autocompGen(const char *text, int state){
    static int index;
    static int len;
    const char *name;

    static DIR *dir;
    static char *path;
    static char *env_p;
    static char path_copy[4096];
    static char *saveptr;

    if (state == 0) {
        index = 0;
        len = strlen(text);
    

        if(dir){
            closedir(dir);
            dir = NULL;
        }

        env_p = getenv(ENV_VARIABLE);
        saveptr = NULL;

        if(env_p){
            strncpy(path_copy, env_p, sizeof(path_copy) - 1);
            path_copy[sizeof(path_copy) - 1] = '\0';
            path = strtok_r(path_copy, ":", &saveptr);
        }else{
            path = NULL;
        }
    }

    // shell builtins //

    while ((name = builtinStr[index++])) {
        if (strncmp(name, text, len) == 0)  return strdup(name);
    }

    // other excutables//

    while (path) {
        if (!dir) {
            dir = opendir(path);
            if (!dir) {
                path = strtok_r(NULL, ":", &saveptr);
                continue;
            }
        }

        struct dirent *entry;
        while ((entry = readdir(dir))) {
            if (strncmp(entry->d_name, text, strlen(text)) != 0) continue;

            char fullpath[PATH_MAX];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

            if (access(fullpath, X_OK) == 0) return strdup(entry->d_name);
        
        }

        closedir(dir);
        dir = NULL;
        path = strtok_r(NULL, ":", &saveptr);
    }    
    return NULL;
}

char** shlAutocomplete(const char* input, int start, int end){
    (void)start;
    (void)end;

    if (start == 0)return rl_completion_matches(input, autocompGen);

    return rl_completion_matches(input, rl_filename_completion_function);

}

/** input processing **/

char *shlReadLine(){
    rl_attempted_completion_function = shlAutocomplete;
    char* line = readline("$ ");
    if(!line){
        printf("\n");
        exit(EXIT_SUCCESS);
    }

    if (*line) { 
        add_history(line);
    }

    return line;
}

#define SHL_TOK_BUFSIZE 64
#define SHL_TOK_DELIM " \t\r\n\a"

char** shlSplitLine(char* line) {
    int bufSize = SHL_TOK_BUFSIZE, pos = 0;
    char** tokens = malloc(bufSize * sizeof(char*));

    if (!tokens) {
        fprintf(stderr, "Allocation Error\n");
        exit(EXIT_FAILURE);
    }

    char* p = line;

    while (*p) {
        while (*p && strchr(SHL_TOK_DELIM, *p)) p++;

        if (!*p) break;

        int cap = 64;
        int len = 0;
        char* token = malloc(cap);

        if (!token) {
            fprintf(stderr, "Allocation Error\n");
            exit(EXIT_FAILURE);
        }

        char quote = 0;
        
        while (*p) {

            if (!quote && strchr(SHL_TOK_DELIM, *p)) break;

            if (*p == '\\') {
                if (quote == '\'') {
                    token[len++] = *p++;
                }
                else if (quote == '"') {
                    p++;
                    if (*p == '"' || *p == '\\') {
                        token[len++] = *p++;
                    } else {
                        token[len++] = '\\';
                        if (*p) token[len++] = *p++;
                    }
                }
                else {
                    p++;
                    if (*p) token[len++] = *p++;
                }
            }else if (*p == '\'' || *p == '"') {
                if (!quote) {
                    quote = *p++;
                } else if (*p == quote) {
                    quote = 0;
                    p++;
                } else {
                    token[len++] = *p++;
                }
            }else {
                token[len++] = *p++;
            }

            if (len + 1 >= cap) {
                cap *= 2;
                token = realloc(token, cap);
                if (!token) {
                    fprintf(stderr, "Allocation Error\n");
                    exit(EXIT_FAILURE);
                }
            }
        }

        token[len] = '\0';
        tokens[pos++] = token;

        if (pos >= bufSize) {
            bufSize += SHL_TOK_BUFSIZE;
            tokens = realloc(tokens, bufSize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "Allocation Error\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    tokens[pos] = NULL;
    return tokens;
}

/** process execution **/

// non-system //

int shlLaunch(char **args){
    pid_t pid, wpid;
    int status;

    pid = fork();
    if(pid == 0){
        if(shlRedir(args) < 0){
            exit(EXIT_FAILURE);
        }
        execvp(args[0], args);
        printf("%s: command not found\n", args[0]);
        exit(EXIT_FAILURE);
    }else if(pid < 0){
        perror("shl");
    }else{
        do{
            wpid = waitpid(pid, &status, WUNTRACED);
        }while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;
}

// system //

int shlExecute(char **args) {
    if (!args[0]) return 1;

    char **cmds[64];
    int ncmds = 0;

    cmds[ncmds++] = args;

    for (int i = 0; args[i]; i++) {
        if (strcmp(args[i], "|") == 0) {
            args[i] = NULL;
            cmds[ncmds++] = &args[i + 1];
        }
    }

    if (ncmds == 1) {

        int saved_stdout = dup(STDOUT_FILENO);
        int saved_stderr = dup(STDERR_FILENO);

        if (saved_stdout < 0 || saved_stderr < 0) {
            perror("dup");
            return 1;
        }

        if (shlRedir(cmds[0]) < 0) {
            dup2(saved_stdout, STDOUT_FILENO);
            dup2(saved_stderr, STDERR_FILENO);
            close(saved_stdout);
            close(saved_stderr);
            return 1;
        }

        for (int i = 0; i < shlNumBuiltin(); i++) {
            if (strcmp(args[0], builtinStr[i]) == 0) {
                int status = (*builtinFunc[i])(cmds[0]);

                dup2(saved_stdout, STDOUT_FILENO);
                dup2(saved_stderr, STDERR_FILENO);
                close(saved_stdout);
                close(saved_stderr);

                return status;
            }
        }
         int status = shlLaunch(cmds[0]);

        dup2(saved_stdout, STDOUT_FILENO);
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stdout);
        close(saved_stderr);

        return status;
    }

    return shlExecutePipeline(cmds, ncmds);
}

int shlRun(char** args){
    for (int i = 0; i < shlNumBuiltin(); i++) {
        if (strcmp(args[0], builtinStr[i]) == 0) {
            return (*builtinFunc[i])(args);
            fflush(stdout);
        }
    }

    execvp(args[0], args);
    perror("execvp");
    return 1;
}

/** main **/

void shlLoop(){
    char *line;
    char **args;
    int status;

    do{
        line = shlReadLine();
        args = shlSplitLine(line);
        status = shlExecute(args);

        free(line);
        free(args);
    }while (status);
    
}


int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  read_history(getenv("HISTFILE"));
  appendidx = history_length;

  shlLoop();

  return 0;
}
