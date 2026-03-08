#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

/* ---------- Forward declarations ---------- */
void shell();
void builtin_echo(char **args);
void builtin_export(char **args);
void expand_variables(char *input);

/* ---------- Variable storage ---------- */
typedef struct {
    char name[64];
    char value[256];
} ShellVar;

ShellVar var_table[64];
int var_count = 0;

/* Store or update a shell variable */
void set_var(char *name, char *value) {
    // update if exists
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) {
            strcpy(var_table[i].value, value);
            return;
        }
    }
    // add new
    strcpy(var_table[var_count].name, name);
    strcpy(var_table[var_count].value, value);
    var_count++;
}

/* Look up a shell variable, returns NULL if not found */
char *get_var(char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0)
            return var_table[i].value;
    }
    return NULL;
}

/* Replace $VAR tokens in input with their values */
void expand_variables(char *input) {
    char result[1024] = "";
    int i = 0, j = 0;

    while (input[i] != '\0') {
        if (input[i] == '$') {
            i++;
            char varname[64] = "";
            int k = 0;
            while (input[i] != '\0' && input[i] != ' ' &&
                   input[i] != '"') {
                varname[k++] = input[i++];
            }
            varname[k] = '\0';
            char *val = get_var(varname);
            if (val == NULL) val = getenv(varname);
            if (val != NULL) {
                strcat(result, val);
                j += strlen(val);
            }
        } else {
            result[j++] = input[i++];
            result[j] = '\0';
        }
    }
    strcpy(input, result); // copy expanded result back into input
}

/* Handle: export NAME=VALUE */
void builtin_export(char **args) {
    if (args[1] == NULL) {
        printf("export: missing arguments\n");
        return;
    }

    // find the = sign
    char *eq = strchr(args[1], '=');
    if (eq == NULL) {
        printf("export: invalid syntax\n");
        return;
    }

    // split into name and value
    *eq = '\0'; // terminate name at =
    char *name = args[1];

    // rejoin everything after = into one value
    // (handles cases where value had spaces)
    char value[256] = "";
    strcat(value, eq + 1); // first part after =
    for (int i = 2; args[i] != NULL; i++) {
        strcat(value, " ");
        strcat(value, args[i]);
    }

    // strip surrounding quotes
    char *v = value;
    int vlen = strlen(v);
    if (vlen >= 2 && v[0] == '"' && v[vlen - 1] == '"') {
        v[vlen - 1] = '\0';
        v++;
    }

    set_var(name, v);
    setenv(name, v, 1); // also set in real env for child processes
}

/* Handle: echo [text] */
void builtin_echo(char **args) {
    if (args[1] == NULL) {
        printf("\n");
        return;
    }

    // rejoin all tokens after echo into one string
    char combined[1024] = "";
    for (int i = 1; args[i] != NULL; i++) {
        if (i > 1) strcat(combined, " ");
        strcat(combined, args[i]);
    }

    // strip surrounding quotes
    char *start = combined;
    int len = strlen(start);
    if (len >= 2 && start[0] == '"' && start[len - 1] == '"') {
        start[len - 1] = '\0';
        start++;
    }

    printf("%s\n", start);
}

/* Signal handler: reap finished background children */
void on_child_exit(int signo) {
    // reap all finished children without blocking
    while (waitpid(-1, NULL, WNOHANG) > 0);

    // write to log file
    FILE *fp = fopen("shell_log.txt", "a");
    if (fp) {
        fprintf(fp, "Child process was terminated\n");
        fclose(fp);
    }
}

/* ---------- Entry point ---------- */
int main() {
    signal(SIGCHLD, on_child_exit);
    shell();
    return 0;
}

/* ---------- Main shell loop ---------- */
void shell() {
    char input[1024];

    while (1) {
        printf("Shell> ");
        fflush(stdout);                          // forces buffer to output prompt
        fgets(input, sizeof(input), stdin);      // reads input
        input[strcspn(input, "\n")] = '\0';      // replaces \n with \0
        expand_variables(input);                 // expand $variables

        char *args[64];
        int argc = 0;

        char *token = strtok(input, " ");
        while (token != NULL) {
            args[argc++] = token;
            token = strtok(NULL, " ");
        }
        args[argc] = NULL;

        if (argc == 0) continue;             // skip empty input

        if (strcmp(args[0], "exit") == 0) {  // handle exit
            exit(0);
        }

        // check for background (&)
        int background = 0;
        if (argc > 0 && !strcmp(args[argc - 1], "&")) {
            background = 1;
            args[--argc] = NULL;
        }

        // built-in: export
        if (strcmp(args[0], "export") == 0) {
            builtin_export(args);
            continue;
        }

        // built-in: echo
        if (strcmp(args[0], "echo") == 0) {
            builtin_echo(args);
            continue;
        }

        // built-in: cd
        if (strcmp(args[0], "cd") == 0) {
            char *path;
            if (args[1] == NULL || !strcmp(args[1], "~")) {
                path = getenv("HOME");
            } else {
                path = args[1];
            }
            if (chdir(path) != 0) {
                perror("cd failed");
            }
            continue; // skip fork, go back to prompt
        }

        // external command — fork + exec
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork Failed!");
        } else if (pid == 0) {
            execvp(args[0], args);
            perror("Command Not Found!");
            exit(1);
        } else {
            if (!background) {
                waitpid(pid, NULL, 0);
            }
        }
    }
}