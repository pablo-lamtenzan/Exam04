
# include <stdbool.h>
# include <unistd.h>
# include <stdlib.h>
# include <sys/wait.h>
# include <string.h>

# include <stdio.h> // debug

// <cmd> ; <cmd> ; -> SEGFAULT (obious, executes an empty av)

# define STD_ERR_MSG "error: fatal\n"
# define TRUE 0
# define FALSE 1

// Tail recursion is optimized by the compiler to a simple loop.
// I do this for fun, i know that for a large input, my stack will be huge.
int main(int ac, const char** av, const char** env);

typedef struct      data
{
    int             ac;
    const char**    av;
    const char**    env;
    int             fds[2];
    bool            out;
    bool            in;
}                   t_data;

static size_t ft_strlen(const char* s)
{
    char* it = (char*)s;
    while (*it)
        it++;
    return (it - s);
}

static int err(const char* const msg)
{
    write(STDERR_FILENO, msg, ft_strlen(msg));
    exit(EXIT_FAILURE);
    return (FALSE);
}

static bool dup2_and_close(int fd, int fd2)
{
    return (!(dup2(fd, fd2) < 0 || close(fd) < 0));
}

static bool close_pipes(int fds[2])
{
    for (int i = 0 ; i < 2 ; i++)
        if (fds[i] != i && close(fds[i]) < 0)
            return (FALSE);
    return (TRUE);
}

static int builin_cd(t_data* const d)
{
    if (d->av[1] == 0)
        return (TRUE);
    if (chdir(d->av[1]))
    {
        err("error: cd: cannot change directory to ");
        err(d->av[1]);
        err("\n");
        return (FALSE);
    }
    return (TRUE);
}

static int excute_job(t_data* const d)
{
    if (!strncmp(*d->av, "cd", 3UL))
        return (builin_cd(d));

    int         ret = FALSE;
    const int   prev_in = d->fds[0];

    if (d->out && !pipe(d->fds))
        return (FALSE);

    int pid = fork();
    if (pid < 0)
        return (FALSE);
    else if (pid == 0)
    {
        if ((d->in && !dup2_and_close(prev_in, STDIN_FILENO))
        || (d->out && !dup2_and_close(d->fds[1], STDOUT_FILENO)))
            return (FALSE);
        ret = execve(*d->av, (char* const *)d->av, (char* const *)d->env);
        err("error: cannot execute ");
        err(*d->av);
        err("\n");
        exit(ret);
    }
    // NOTE: In the true bash pipes aren't waited before the end of the piped command.
    int         wstatus;
    while (waitpid(pid, &wstatus, 0) <= 0);
    if (WIFEXITED(wstatus))
        ret = WEXITSTATUS(wstatus);
    return (close_pipes(d->fds) == FALSE ? FALSE : ret);
}

static size_t next_symbol(t_data* const d, char* const found)
{
    size_t distance = 0;
    *found = 0;

    while (d->av[distance])
    {
        if (!strncmp(d->av[distance], "|", 2UL))
        {
            *found = '|';
            break ;
        }
        else if (!strncmp(d->av[distance], ";", 2UL))
        {
            *found = ';';
            break ;
        }
        distance++;
    }
    d->av[distance] = 0;
    return (*found ? distance : 0UL);
}

static int run_command(t_data* const d)
{
    static int ret = FALSE;

    // Represent next symbol (pipe or semicolon)
    char            next;

    // Set the fd designed to hold piped stdout to it default value (useful for last pipe)
    d->fds[1] = STDOUT_FILENO;
    // Skip av[0] or replaced by 0 pipes/semicolons
    ++d->av;

    // Get next pipe or semicolon and the distance until the found symbol (or 0 if no found)
    const size_t    cmd_lenght = next_symbol(d, &next);

    // Start the pipe
    d->out = next == '|';

    // execute using pipes or not-piped commands
    if ((ret = excute_job(d)) == FALSE)
        return (err(STD_ERR_MSG));

    // No more commands, the end
    if (cmd_lenght == 0UL)
        return (ret);

    // Complete the pipe (for next iteration)
    d->in = d->out;

    // Iterate to next command - 1 (reset data if there's a semicolon)
    d->av += cmd_lenght;
    return (next != ';' ? run_command(d) : main(d->ac, d->av, d->env));
}

int main(int ac, const char** av, const char** env)
{
    if (ac <= 1)
        return (0); // or another value ?

    t_data data = (t_data){
        .ac = ac,
        .av = av,
        .env = env,
        .fds[0] = STDIN_FILENO,
        .fds[1] = STDOUT_FILENO,
        .out = false,
        .in = false
        };

    return (run_command(&data));
}