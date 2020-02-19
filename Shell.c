#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <readline/history.h>
#include <readline/readline.h>

#define SH_BUFSIZE 64
#define SH_TOK_DELIM " \t\r\n\a"

int shell_cd(char **args);
int shell_exit(char **args);
int shell_help(char **args);

char *builtin_str[] = {
    "cd",
    "help",
    "exit"};

int (*builtin_func[])(char **) = {
    &shell_cd,
    &shell_help,
    &shell_exit};

int shell_nr_builtins()
{
    return sizeof(builtin_str) / sizeof(char *);
}

int shell_cd(char **args)
{
    if (args[1] == NULL)
    {
        fprintf(stderr, "Lipsa argument to \"cd\" \n ");
    }
    else
    {
        if (chdir(args[1]) != 0)
        {
            perror(NULL);
            return errno;
        }
    }
    return 1;
}

int shell_help(char **args)
{
    int i;
    printf("MyShell\n");
    printf("Urmatoarele programe sunt incorporate:\n");

    for (i = 0; i < shell_nr_builtins(); i++)
    {
        printf("%s\n", builtin_str[i]);
    }
    return 1;
}

int shell_exit(char **args)
{
    return 0;
}

int shell_launch(char **args)
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "Fork Failed");
        return 1;
    }
    else if (pid == 0)
    {
        if (execvp(args[0], args) == -1)
        {
            perror(NULL);
            return errno;
        }
        exit(EXIT_FAILURE);
    }
    else
    {
        do
        {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;
}

int shell_execute(char **args)
{
    int i;

    if (args[0] == NULL)
    {
        return 1;
    }

    for (i = 0; i < shell_nr_builtins(); i++)
    {
        if (strcmp(args[0], builtin_str[i]) == 0)
        {
            return (*builtin_func[i])(args);
        }
    }
    return shell_launch(args);
}

int shell_execute_pipe(char **args, char **argspipe)
{
    // 0 read end, 1 write end
    int pipefd[2];
    pid_t p1, p2;
    int status1, status2;

    if (pipe(pipefd) < 0)
    {
        fprintf(stderr, "Piep Failed");
        return 1;
    }

    p1 = fork();
    if (p1 < 0)
    {
        fprintf(stderr, "Fork Failed");
        return 1;
    }

    if (p1 == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        if (execvp(args[0], args) == -1)
        {
            fprintf(stderr, "Failed");
            return 1;
        }
    }
    else
    {
        p2 = fork();
        if (p2 < 0)
        {
            fprintf(stderr, "Fork Failed");
            return 1;
        }

        if (p2 == 0)
        {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);

            if (execvp(argspipe[0], argspipe) == -1)
            {
                fprintf(stderr, "Failed");
                return 1;
            }
        }
        else
        {
            do
            {
                waitpid(p1, &status1, WUNTRACED);
            } while (!WIFEXITED(status1) && !WIFSIGNALED(status1));

            do
            {
                waitpid(p2, &status2, WUNTRACED);
            } while (!WIFEXITED(status2) && !WIFSIGNALED(status2));
        }
    }
    return 1;
}

char *shell_read_line()
{
    // char *line = NULL;
    // ssize_t bufsize = 0;
    // if (getline(&line, &bufsize, stdin) != -1)
    // {
    //     return line;
    // }
    // else
    // {
    //     fprintf(stderr, "Eroare citire linie comanda!\n");
    //     exit(EXIT_FAILURE);
    // }

    char *buf;

    buf = readline("L_R_sh> ");
    if (strlen(buf) != 0)
    {
        add_history(buf);
        return buf;
    }
    else
    {
        fprintf(stderr, "Eroare citire linie comanda!\n");
        exit(EXIT_FAILURE);
    }
}

char **shell_split_line(char *line)
{
    int bufsize = SH_BUFSIZE, position = 0;
    char **tokens = (char **)calloc(bufsize, sizeof(char *));
    char *token;

    if (!tokens)
    {
        fprintf(stderr, "Eroare alocare memorie\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, SH_TOK_DELIM);
    while (token != NULL)
    {
        tokens[position] = token;
        position++;

        if (position >= bufsize)
        {
            bufsize += SH_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens)
            {
                fprintf(stderr, "Eroare alocare memorie\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, SH_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

int parsePipe(char *line, char **strpiped)
{
    int i;
    char *token;
    token = strtok(line, "|");
    for (i = 0; i < 2; i++)
    {
        strpiped[i] = token;

        if (strpiped[i] == NULL)
        {
            break;
        }
        token = strtok(NULL, "|");
    }

    if (strpiped[i] == NULL)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

void shell_loop()
{
    char *line;
    char **args;
    char **argspipe;
    char **strpiped;
    int piped = 0;
    int status;

    do
    {
        strpiped = malloc(2 * sizeof(char *));
        line = shell_read_line();

        piped = parsePipe(line, strpiped);

        if (piped == 1)
        {
            args = shell_split_line(strpiped[0]);
            argspipe = shell_split_line(strpiped[1]);

            status = shell_execute_pipe(args, argspipe);

            free(argspipe);
        }

        else
        {
            args = shell_split_line(line);

            status = shell_execute(args);
        }

        free(line);
        free(args);
        free(strpiped);

    } while (status);
}

int main(int argc, char **argv)
{
    shell_loop();
}