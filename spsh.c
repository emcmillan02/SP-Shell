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

/** list **/

typedef struct Job{
    int jNum;
    pid_t pid;
    char* cmd;
    char* status;
    struct Job *next;
} Job;

typedef struct JobList{
    Job *head;
    Job *tail;
    size_t size;
} JobList;

static JobList *jobList = NULL;

JobList *initJList(){
    JobList *list = malloc(sizeof(JobList));
    if (list == NULL) {
        fprintf(stderr, "Error creating list\n");
        return NULL;
    }
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    return list;
}

void appendJList(JobList *list, int jNum, pid_t pid, char *cmd, char *status){
    Job *newJob = malloc(sizeof(Job));
    if (newJob == NULL) {
        fprintf(stderr, "Error appending list\n");
        return;
    }

    newJob->jNum = jNum;
    newJob->pid = pid;
    newJob->cmd = strdup(cmd);
    newJob->status = strdup(status);
    newJob->next = NULL;

    if (list->head == NULL) {
        list->head = newJob;
        list->tail = newJob;
        list->head = list->tail;
    } else {
        list->tail->next = newJob;
        list->tail = newJob;
    }

    list->size++;
}

void freeJList(JobList *list){
    Job *next = list->head;
    Job *toFree = NULL;
    while (next != NULL) {
        toFree = next;
        next = next->next;
        free(toFree->cmd);
        free(toFree->status);
        free(toFree);
    }
    free(list);
}

void shlUpdateJobs(JobList *list) {
    Job *iter = list->head;
    while (iter != NULL) {
        int status;
        pid_t result = waitpid(iter->pid, &status, WNOHANG);
        if (result > 0 && (WIFEXITED(status) || WIFSIGNALED(status))) {
            free(iter->status);
            iter->status = strdup("Done");
        }
        iter = iter->next;
    }
}

/** programmable completions **/

typedef struct Completion {
    char* name;
    char* path;
    struct Completion *next;
} Completion;

typedef struct CompDictionary {
    Completion *head;
    Completion *tail;
    size_t size;
} CompDictionary;

static CompDictionary *compDict = NULL;

CompDictionary *initDict(){
    CompDictionary *dict = malloc(sizeof(CompDictionary));
    if (dict == NULL) {
        fprintf(stderr, "Error creating dictionary\n");
        return NULL;
    }
    dict->head = NULL;
    dict->tail = NULL;
    dict->size = 0;
    return dict;
}

void appendDict(CompDictionary *dict, char* name, char* path){
    Completion *newComp = malloc(sizeof(Completion));
    if (newComp == NULL) {
        fprintf(stderr, "Error appending dictionary\n");
        return;
    }

    newComp->name = name;
    newComp->path = path;

    if (dict->head == NULL) {
        dict->head = newComp;
        dict->tail = newComp;
        dict->head = dict->tail;
    } else {
        dict->tail->next = newComp;
        dict->tail = newComp;
    }

    dict->size++;
}

void freeDict(CompDictionary *dict){
    Completion *next = dict->head;
    Completion *toFree = NULL;
    while (next != NULL) {
        toFree = next;
        next = next->next;
        free(toFree->name);
        free(toFree->path);
        free(toFree);
    }
    free(dict);
}

/** shell variables **/

typedef struct Variable{
    char* name;
    char* value;
    struct Variable *next;    
} Variable;

typedef struct VarList{
    Variable* head;
    Variable* tail;
    size_t size;
} VarList;

static VarList *varList = NULL;

VarList *initVList(){
    VarList *dict = malloc(sizeof(VarList));
    if (dict == NULL) {
        fprintf(stderr, "Error creating list\n");
        return NULL;
    }
    dict->head = NULL;
    dict->tail = NULL;
    dict->size = 0;
    return dict;
}

void appendVList(VarList *list, char *name, char *value) {
    Variable *var = list->head;

    while (var != NULL) {
        if (strcmp(var->name, name) == 0) {
            free(var->value);
            var->value = strdup(value);
            return;
        }
        var = var->next;
    }

    Variable *newVar = malloc(sizeof(Variable));
    if (newVar == NULL) {
        fprintf(stderr, "Error appending list\n");
        return;
    }
    newVar->name  = strdup(name);
    newVar->value = strdup(value);
    newVar->next  = NULL;

    if (list->head == NULL) {
        list->head = newVar;
        list->tail = newVar;
    } else {
        list->tail->next = newVar;
        list->tail = newVar;
    }
    list->size++;
}

void freeVList(VarList *list){
    Variable *next = list->head;
    Variable *toFree = NULL;
    while (next != NULL) {
        toFree = next;
        next = next->next;
        free(toFree->name);
        free(toFree->value);
        free(toFree);
    }
    free(list);
}

int isValidIdentifier(const char *name) {
    if (!name || (!isalpha((unsigned char)name[0]) && name[0] != '_'))
        return 0;
    for (int i = 1; name[i]; i++) {
        if (!isalnum((unsigned char)name[i]) && name[i] != '_')
            return 0;
    }
    return 1;
}

char *expandVar(const char *word) {
    size_t cap = 128;
    size_t len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;

    const char *p = word;
    while (*p) {
        if (*p == '$' && p[1] == '{') {
            p += 2;
            char name[256];
            int nlen = 0;
            while (*p && *p != '}' && nlen < 255) name[nlen++] = *p++;
            name[nlen] = '\0';
            if (*p == '}') p++;

            const char *val = "";
            Variable *var = varList->head;
            while (var != NULL) {
                if (strcmp(var->name, name) == 0) { val = var->value; break; }
                var = var->next;
            }

            size_t vlen = strlen(val);
            while (len + vlen + 1 >= cap) { cap *= 2; out = realloc(out, cap); }
            memcpy(out + len, val, vlen);
            len += vlen;

        } else if (*p == '$' && (isalpha((unsigned char)p[1]) || p[1] == '_')) {
            p++;
            char name[256];
            int nlen = 0;
            while (*p && (isalnum((unsigned char)*p) || *p == '_') && nlen < 255) name[nlen++] = *p++;
            name[nlen] = '\0';

            const char *val = "";
            Variable *var = varList->head;
            while (var != NULL) {
                if (strcmp(var->name, name) == 0) { val = var->value; break; }
                var = var->next;
            }

            size_t vlen = strlen(val);
            while (len + vlen + 1 >= cap) { cap *= 2; out = realloc(out, cap); }
            memcpy(out + len, val, vlen);
            len += vlen;

        } else {
            if (len + 2 >= cap) { cap *= 2; out = realloc(out, cap); }
            out[len++] = *p++;
        }
    }
    out[len] = '\0';
    return out;
}

/** prototypes **/

int shlRun(char** args);

/** builtin declarations **/

int shlExit(char **args);
int shlEcho(char **args);
int shlHelp(char **args);
int shlType(char **args);
int shlPwd(char **args);
int shlCd(char **args);
int shlHistory(char **args);
int shlJobs(char **args);
int shlComplete(char **args);
int shlDeclare(char **args);

/** builtin commands **/

static int appendidx = 0;

char *builtinStr[] = {
    "exit",
    "echo",
    "help",
    "type",
    "pwd",
    "cd",
    "history",
    "jobs",
    "complete",
    "declare"
};

char *autocompleteStr[] = {
    "exit",
    "echo",
    "help",
    "type",
    "pwd",
    "cd",
    "history",
    "jobs",
    "complete",
    "declare"
};

int (*builtinFunc[]) (char **) = {
    &shlExit,
    &shlEcho,
    &shlHelp,
    &shlType,
    &shlPwd,
    &shlCd,
    &shlHistory,
    &shlJobs,
    &shlComplete,
    &shlDeclare
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

int shlJobs(char **args) {
    (void)args;
    shlUpdateJobs(jobList);

    Job *iter = jobList->head;
    size_t pos = 1;
    size_t size = jobList->size;

    while (iter != NULL) {
        char marker;
        if      (pos == size)     marker = '+';
        else if (pos == size - 1) marker = '-';
        else                      marker = ' ';

        printf("[%d]%c  %-12s %s\n", iter->jNum, marker, iter->status, iter->cmd);
        iter = iter->next;
        pos++;
    }

    Job **pp = &jobList->head;
    while (*pp != NULL) {
        Job *cur = *pp;
        if (strcmp(cur->status, "Done") == 0) {
            *pp = cur->next;
            free(cur->cmd);
            free(cur->status);
            free(cur);
            jobList->size--;
        } else {
            pp = &(*pp)->next;
        }
    }
    jobList->tail = NULL;
    Job *iter2 = jobList->head;
    while (iter2 != NULL) {
        jobList->tail = iter2;
        iter2 = iter2->next;
    }

    return 1;
}

int shlComplete(char **args){
    int argc = 0;
    while (args[argc]) argc++;

    char* flag = args[1];

    Completion *comp = compDict->head;

    if(strcmp(flag, "-p") == 0 && argc != 3 || strcmp(flag, "-r") == 0 && argc != 3 || strcmp(flag, "-C") == 0 && argc != 4){
        fprintf(stderr, "Syntax error: Invalid number of flags");
        return 1;
    }

    if(strcmp(flag, "-p") == 0){
        bool registered = false;
        char* name = args[2];
        if(compDict->size == 0){
            printf("complete: %s: no completion specification\n", name);
            return 1;
        }
        while(comp != NULL){
            if(strcmp(name, comp->name) == 0){
                printf("complete -C '%s' %s\n", comp->path, comp->name);
                registered = true;
                break;
            }
            comp = comp->next;
        }
        if(comp == NULL && registered == false){
            printf("complete: %s: no completion specification\n", name);
            return 1;
        }
    }else if(strcmp(flag, "-r") == 0){
        bool found = false;
        char* targetName = args[2];
        Completion *targetPrev = NULL;
        if(compDict->size == 0){
            printf("complete: %s: no completion specification\n", targetName);
            return 1;
        }
        if((strcmp(targetName, comp->name) == 0) && targetPrev == NULL){
            compDict->head = NULL;
            free(comp);
            found = true;
        }else{
            while(comp != NULL){
                if(strcmp(targetName, comp->name) == 0){
                    targetPrev->next = comp->next;
                    free(comp);
                    found = true;
                    break;
                }
                targetPrev = comp;
                comp = comp->next;
            }
        }
        if(comp == NULL && found == false){
            printf("complete: %s: no completion specification\n", targetName);
            return 1;
        }
    }else if(strcmp(flag, "-C") == 0){
        char* name = args[3];
        char* path = args[2];
        appendDict(compDict, name, path);
    }else{
        fprintf(stderr, "Syntax error: Invalid flag: %s", flag);
        return 1;
    }
    return 1;
}

int shlDeclare(char **args) {
    int argc = 0;
    while (args[argc]) argc++;

    if (argc < 2) {
        fprintf(stderr, "declare: usage: declare [-p] [NAME=VALUE]\n");
        return 1;
    }

    if (strcmp(args[1], "-p") == 0) {
        if (argc == 2) {
            Variable *var = varList->head;
            while (var != NULL) {
                printf("declare -- %s=\"%s\"\n", var->name, var->value);
                var = var->next;
            }
            return 1;
        }

        char *name = args[2];
        Variable *var = varList->head;
        while (var != NULL) {
            if (strcmp(var->name, name) == 0) {
                printf("declare -- %s=\"%s\"\n", var->name, var->value);
                return 1;
            }
            var = var->next;
        }
        fprintf(stderr, "declare: %s: not found\n", name);
        return 1;
    }

    for (int i = 1; args[i]; i++) {
        char *eq = strchr(args[i], '=');
        if (!eq) {
            fprintf(stderr, "declare: %s: not a valid assignment\n", args[i]);
            continue;
        }
        *eq = '\0';
        char *name  = args[i];
        char *value = eq + 1;
        if (!isValidIdentifier(name)) {
            fprintf(stderr, "declare: `%s=%s': not a valid identifier\n", name, value);
            *eq = '=';
            continue;
        }
        appendVList(varList, name, value);
        *eq = '=';
    }
    return 1;
}

/** utilities **/

int shlNextJobNum(JobList *list) {
    int n = 1;
    while (1) {
        int taken = 0;
        Job *iter = list->head;
        while (iter != NULL) {
            if (iter->jNum == n) { taken = 1; break; }
            iter = iter->next;
        }
        if (!taken) return n;
        n++;
    }
}

void shlReapJobs(JobList *list) {
    Job *iter = list->head;
    while (iter != NULL) {
        int status;
        pid_t result = waitpid(iter->pid, &status, WNOHANG);
        if (result > 0 && (WIFEXITED(status) || WIFSIGNALED(status))) {
            free(iter->status);
            iter->status = strdup("Done");
        }
        iter = iter->next;
    }

    Job **pp = &list->head;
    while (*pp != NULL) {
        Job *cur = *pp;
        if (strcmp(cur->status, "Done") == 0) {
            printf("[%d]+  Done                 %s\n", cur->jNum, cur->cmd);
            *pp = cur->next;
            free(cur->cmd);
            free(cur->status);
            free(cur);
            list->size--;
        } else {
            pp = &(*pp)->next;
        }
    }

    list->tail = NULL;
    Job *iter2 = list->head;
    while (iter2 != NULL) {
        list->tail = iter2;
        iter2 = iter2->next;
    }
}

char *argsToCmd(char **args) {
    size_t len = 0;
    for (int i = 0; args[i]; i++) len += strlen(args[i]) + 1;
    char *cmd = malloc(len + 1);
    if (!cmd) return NULL;
    cmd[0] = '\0';
    for (int i = 0; args[i]; i++) {
        strcat(cmd, args[i]);
        if (args[i + 1]) strcat(cmd, " ");
    }
    return cmd;
}

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

char *progCompGen(const char *text, int state) {
    static char **lines = NULL;
    static int nlines = 0;
    static int index = 0;
    static int len;

    if (state == 0) {
        if (lines) {
            for (int i = 0; i < nlines; i++) free(lines[i]);
            free(lines);
            lines = NULL;
        }
        nlines = 0;
        index = 0;
        len = strlen(text);

        char linecopy[1024];
        int point = rl_point;
        if (point > (int)sizeof(linecopy) - 1) point = sizeof(linecopy) - 1;
        strncpy(linecopy, rl_line_buffer, point);
        linecopy[point] = '\0';

        char *words[64];
        int nwords = 0;
        char *tok = strtok(linecopy, " \t");
        while (tok && nwords < 64) {
            words[nwords++] = tok;
            tok = strtok(NULL, " \t");
        }

        if (nwords == 0) return NULL;

        char *cmd = words[0];

        char prevword[256] = "";
        if (nwords >= 1 && strcmp(words[nwords - 1], text) == 0) {
            if (nwords >= 2)
                strncpy(prevword, words[nwords - 2], sizeof(prevword) - 1);
        } else {
            strncpy(prevword, words[nwords - 1], sizeof(prevword) - 1);
        }
        prevword[sizeof(prevword) - 1] = '\0';

        Completion *comp = compDict->head;
        char *path = NULL;
        while (comp != NULL) {
            if (strcmp(comp->name, cmd) == 0) {
                path = comp->path;
                break;
            }
            comp = comp->next;
        }
        if (!path) return NULL;

        char comp_point_str[32];
        snprintf(comp_point_str, sizeof(comp_point_str), "%d", rl_point);

        int pipefd[2];
        if (pipe(pipefd) < 0) return NULL;

        pid_t pid = fork();
        if (pid < 0) {
            close(pipefd[0]);
            close(pipefd[1]);
            return NULL;
        }

        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);

            setenv("COMP_LINE",  rl_line_buffer, 1);
            setenv("COMP_POINT", comp_point_str, 1);

            char *child_args[] = { path, cmd, (char *)text, prevword, NULL };
            execvp(path, child_args);
            _exit(1);
        }

        close(pipefd[1]);

        FILE *fp = fdopen(pipefd[0], "r");
        if (!fp) {
            close(pipefd[0]);
            waitpid(pid, NULL, 0);
            return NULL;
        }

        char buf[1024];
        int cap = 16;
        lines = malloc(cap * sizeof(char *));
        while (fgets(buf, sizeof(buf), fp)) {
            size_t l = strlen(buf);
            if (l > 0 && buf[l - 1] == '\n') buf[l - 1] = '\0';
            if (nlines >= cap) {
                cap *= 2;
                lines = realloc(lines, cap * sizeof(char *));
            }
            lines[nlines++] = strdup(buf);
        }
        fclose(fp);
        waitpid(pid, NULL, 0);
    }
    while (index < nlines) {
        char *candidate = lines[index++];
        if (strncmp(candidate, text, len) == 0) return strdup(candidate);
    }
    return NULL;
}

char *autocompGen(const char *text, int state) {
    static int index;
    static int len;
    const char *name;
    static DIR *dir;
    static char *path;
    static char *env_p;
    static char path_copy[4096];
    static char *saveptr;
    static DIR *cwd_dir;

    if (state == 0) {
        index = 0;
        len = strlen(text);

        if (dir) { closedir(dir); dir = NULL; }
        if (cwd_dir) { closedir(cwd_dir); cwd_dir = NULL; }  // NEW

        env_p = getenv(ENV_VARIABLE);
        saveptr = NULL;
        if (env_p) {
            strncpy(path_copy, env_p, sizeof(path_copy) - 1);
            path_copy[sizeof(path_copy) - 1] = '\0';
            path = strtok_r(path_copy, ":", &saveptr);
        } else {
            path = NULL;
        }

        cwd_dir = opendir(".");
    }

    while ((name = builtinStr[index++])) if (strncmp(name, text, len) == 0) return strdup(name);

    while (path) {
        if (!dir) {
            dir = opendir(path);
            if (!dir) { path = strtok_r(NULL, ":", &saveptr); continue; }
        }
        struct dirent *entry;
        while ((entry = readdir(dir))) {
            if (strncmp(entry->d_name, text, len) != 0) continue;
            char fullpath[PATH_MAX];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
            if (access(fullpath, X_OK) == 0) return strdup(entry->d_name);
        }
        closedir(dir);
        dir = NULL;
        path = strtok_r(NULL, ":", &saveptr);
    }

    if (cwd_dir) {
        struct dirent *entry;
        while ((entry = readdir(cwd_dir))) {
            if (entry->d_name[0] == '.' && (len == 0 || text[0] != '.'))
                continue;
            if (strncmp(entry->d_name, text, len) == 0) {
                closedir(cwd_dir); 
                cwd_dir = NULL;
                return strdup(entry->d_name);
            }
        }
        closedir(cwd_dir);
        cwd_dir = NULL;
    }

    return NULL;
}

char** shlAutocomplete(const char* input, int start, int end){
    (void)end;

    if (start == 0) return rl_completion_matches(input, autocompGen);

    char linecopy[1024];
    strncpy(linecopy, rl_line_buffer, sizeof(linecopy) - 1);
    linecopy[sizeof(linecopy) - 1] = '\0';
    char *cmd = strtok(linecopy, " \t");

    if (cmd) {
        Completion *comp = compDict->head;
        while (comp != NULL) {
            if (strcmp(comp->name, cmd) == 0)
                return rl_completion_matches(input, progCompGen);
            comp = comp->next;
        }
    }

    return rl_completion_matches(input, rl_filename_completion_function);
}

/** input processing **/

char *shlReadLine() {
    rl_attempted_completion_function = shlAutocomplete;

    shlReapJobs(jobList);

    char *line = readline("$ ");
    if (!line) {
        printf("\n");
        exit(EXIT_SUCCESS);
    }

    if (*line) add_history(line);
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

void expandArgs(char **args) {
    for (int i = 0; args[i]; i++) {
        if (strchr(args[i], '$')) {
            char *expanded = expandVar(args[i]);
            free(args[i]);

            if (expanded == NULL || expanded[0] == '\0') {
                free(expanded);
                int j = i;
                while (args[j] != NULL) {
                    args[j] = args[j + 1];
                    j++;
                }
                i--;
            } else {
                args[i] = expanded;
            }
        }
    }
}

/** process execution **/

// non-system //

int shlLaunch(char **args, int background) {
    pid_t pid, wpid;
    int status;

    int argc = 0;
    while (args[argc]) argc++;
    if (argc > 0 && strcmp(args[argc - 1], "&") == 0) {
        args[argc - 1] = NULL;
        background = 1;
    }

    char *cmd = background ? argsToCmd(args) : NULL;

    pid = fork();
    if (pid == 0) {
        if (background) setpgid(0, 0);
        if (shlRedir(args) < 0) exit(EXIT_FAILURE);
        execvp(args[0], args);
        printf("%s: command not found\n", args[0]);
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("shl");
        free(cmd);
    } else {
        if (background) {
            int jobId = shlNextJobNum(jobList);
            appendJList(jobList, jobId, pid, cmd, "Running");  // ADD
            printf("[%d] %d\n", jobId, pid);
            while (waitpid(-1, &status, WNOHANG) > 0);
        } else {
            do {
                wpid = waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
        free(cmd);
    }
    return 1;
}

// system //

int shlExecute(char **args) {
    if (!args[0]) return 1;

    expandArgs(args);

    int argc = 0;
    while (args[argc]) argc++;
    int background = (argc > 0 && strcmp(args[argc - 1], "&") == 0);
    if (background) args[--argc] = NULL;

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
        if (saved_stdout < 0 || saved_stderr < 0) { perror("dup"); return 1; }

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

        int status = shlLaunch(cmds[0], background); 
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
    jobList = initJList();
    compDict = initDict();
    varList = initVList();
    read_history(getenv("HISTFILE"));
    appendidx = history_length;

    shlLoop();
    freeJList(jobList);
    freeDict(compDict);
    freeVList(varList);
    return 0;
}
