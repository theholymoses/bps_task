#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>

/* ******************************************************************************** */
const char *progname = "sotest";

#define IBLEN (1 << 16)     // input buffer length
char    ibuf[IBLEN];        // input buffer

int interactive = 0;        // interactive mode flag

const char comment = ';';   // comment character

typedef struct
{
    char *path;
    void *handle;
} dlib_t;

size_t dlib_len = 0;        // current count of loaded libs
size_t dlib_i   = 0;        // next index of loaded lib
dlib_t *dlib    = 0;        // pointer to array of loaded libs

typedef void (*function)(void);

/* ******************************************************************************** */
enum state_t
{
    STATE_EXPECT_CMD,
    STATE_EXPECT_ARG,
    STATE_EXPECT_EOL,
    STATE_SKIP_TO_EOL
};

enum cmd_t
{
    CMD_USE,
    CMD_CALL,
    CMD_END
};

const char *cmd_s[] =
{
    "use",
    "call",
    0
};

size_t cmd_slen[] =
{
    sizeof ("use")  - 1,
    sizeof ("call") - 1,
    0
};

const enum cmd_t cmd_start = CMD_USE;
const enum cmd_t cmd_end   = CMD_END;

/* ******************************************************************************** */
// Reads bytes from file descriptor 'fd', that is file named 'sc' in input buffer 'ibuf'
// Sends read bytes to interpreter function
// If interpreter function wasn't able to parse some bytes - move those bytes to the beginning
// of a buffer, read some more bytes and try to parse them again
void
read_loop (int fd, const char *sc);

// print interpreter error
void
print_interpreter_warning (const char *error, size_t line);

// Interpret the string of size len
ssize_t
interpret (char *str, ssize_t len, size_t *line);

// runs specified command
void
exec_command (enum cmd_t cmd, const char *argument, size_t len);

// Loads specified library
void
use (const char *argument, size_t len);

// Loads specified symbol from the first loaded library it finds it in
void
call (const char *argument);

/* ******************************************************************************** */
int
main (int argc, char *argv[])
{
    struct stat st;             // statbuf to check that script file is a refular file
    int         fd;             // file descriptor
    const char  *sc_path;       // path to script file

    if (argc == 1)              // no parameters provided - run in interactive mode
    {
        sc_path     = 0;
        fd          = STDIN_FILENO;
        interactive = 1;
    }
    else                        // at least one parameter is provided - check if it's refular file
    {                           // and try to open it
        sc_path = argv[1];

        if (stat (sc_path, &st) != 0)
        {
            fprintf (stderr, "%s: Error on calling stat for file '%s': %s\n", progname, sc_path, strerror (errno));
            exit (1);
        }

        if (!S_ISREG (st.st_mode))
        {
            fprintf (stderr, "%s: File '%s' is not a regular file.\n", progname, sc_path);
            exit (1);
        }

        if ((fd = open (sc_path, O_RDONLY)) == -1)
        {
            fprintf (stderr, "%s: Error opening file '%s': %s\n", progname, sc_path, strerror (errno));
            exit (1);
        }
    }

    if (interactive)
        fprintf (stdout, "Running in interactive mode. Press ^C to exit.\n");

    read_loop (fd, sc_path);

    if (fd != STDIN_FILENO)
        close (fd);             // no need to check the result, programm terminates anyway

    for (size_t i = 0; i < dlib_i; ++i)
        free (dlib[i].path);
    if (dlib_i)
        free (dlib);

    exit (0);
}

/* ******************************************************************************** */
void
read_loop (int fd, const char *sc_path)
{
    char    *begin = ibuf;          // position in buffer at which to read to (iterator start)
    char    *end   = ibuf + IBLEN;  // end of buffer pointer (iterator end)
    ssize_t b_read = 0;             // bytes read at once
    ssize_t b_left = 0;             // uninterpreted bytes left after interpreting
    size_t  line   = 1;             // line number

    // if some uninterpreted bytes left - move them to start of the buffer, read some more,
    // and try to interpret again
    while ((b_read = read (fd, begin, end - begin)))
    {
        if (b_read == -1)
        {
            if (interactive)
                fprintf (stderr, "%s: Error while reading stdin: %s\n", progname, strerror (errno));
            else
                fprintf (stderr, "%s: Error while reading file %s: %s\n", progname, sc_path, strerror (errno));
            break;
        }

        if (b_left = interpret (ibuf, b_left + b_read, &line))
        {
            memmove (ibuf, begin + b_read - b_left, b_left);
            begin = ibuf + b_left;
        }
        else
            begin = ibuf;
    }

    if (b_left)
        interpret (ibuf, b_left, &line);
}

void
print_interpreter_warning (const char *error, size_t line)
{
    fprintf (stderr, "%s: %s", progname, error);
    if (!interactive)
        fprintf (stderr, " at line %zu", line);
    fputc ('\n', stderr);
}

ssize_t
interpret (char *b, ssize_t len, size_t *line)
{
    static enum state_t st  = STATE_EXPECT_CMD;  // current parsing state
    static enum cmd_t   cmd = cmd_start;         // current command

    while (len > 0)
    {
        int c = *b;

        if (c == ' ' || c == '\t')
        {
            ++b;
            --len;
            continue;
        }

        if (c == '\n')
        {
            ++b;
            --len;

            if (st == STATE_EXPECT_ARG)
                print_interpreter_warning ("No argument provided for command", *line);

            *line += 1;
            st = STATE_EXPECT_CMD;

            if (interactive)
                break;
            continue;
        }

        if (c == comment)
        {
            ++b;
            --len;

            if (st == STATE_EXPECT_ARG)
                print_interpreter_warning ("No argument provided for command", *line);

            st = STATE_SKIP_TO_EOL;
            continue;
        }

        if (st == STATE_SKIP_TO_EOL)
        {
            ++b;
            --len;
            continue;
        }

        if (st == STATE_EXPECT_CMD)
        {
            int probably_read_partially = 0;

            for (cmd = cmd_start; cmd < cmd_end; ++cmd)
            {
                if ((size_t)len < cmd_slen[cmd] + 1)
                {
                    probably_read_partially = 1;
                    continue;
                }

                if (strncmp (b, cmd_s[cmd], cmd_slen[cmd]) == 0)
                {
                    b   += cmd_slen[cmd];
                    len -= cmd_slen[cmd];

                    // to be sure that command is a separate word
                    if (*b == ' ' || *b == '\t')
                    {
                        ++b;
                        --len;
                        st = STATE_EXPECT_ARG;
                        probably_read_partially = 0;
                    }
                    else if (*b == '\n')
                    {
                        ++b;
                        --len;

                        print_interpreter_warning ("No argument provided", *line);

                        *line += 1;
                        st = STATE_EXPECT_CMD;
                        continue;
                    }
                    else
                        cmd = cmd_end;
                    break;
                }
            }

            if (probably_read_partially && !interactive)
                break;

            if (cmd == cmd_end)
            {
                print_interpreter_warning ("Unrecongized command", *line);
                st = STATE_SKIP_TO_EOL;    // ignore everything else on this line
            }
            continue;
        }

        if (st == STATE_EXPECT_ARG)
        {
            // first make sure that argument was not read partially
            char *b_temp = b;
            ssize_t len_temp = len;

            while (len_temp > 0)
            {
                if (*b_temp == '\n' || *b_temp == '\t' ||
                    *b_temp == ' '  || *b_temp == comment)
                    break;

                ++b_temp;
                --len_temp;
            }

            if (len_temp == 0)
                break;
            else if (len_temp == len)
            {
                print_interpreter_warning ("No argument provided", *line);
                st = STATE_SKIP_TO_EOL;    // ignore everything else on this line
                continue;
            }

            if (cmd == CMD_USE && b_temp - b > PATH_MAX - 1)
            {
                print_interpreter_warning ("Argument to 'use' exceedes PATH_MAX.", *line);
                st = STATE_SKIP_TO_EOL;
                continue;
            }

            char ch = *b_temp;
            *b_temp = 0;
            exec_command (cmd, b, b_temp - b);
            *b_temp = ch;

            b   = b_temp;
            len = len_temp;
            st  = STATE_EXPECT_EOL;
            continue;
        }

        if (st == STATE_EXPECT_EOL)
        {
            print_interpreter_warning ("Garbage at an end of a line", *line);
            st = STATE_SKIP_TO_EOL;
            continue;
        }
    }

    return (len);
}

void
exec_command (enum cmd_t cmd, const char *argument, size_t len)
{
    switch (cmd)
    {
        case CMD_USE:
        {
            use (argument, len);
            break;
        }
        case CMD_CALL:
        {
            call (argument);
            break;
        }
        default:
        {
            fprintf (stderr, "%s: Unknown command\n", progname);
            if (!interactive)
                exit (1);
        }
    }
}

void
use (const char *argument, size_t len)
{
    if (dlib_i == dlib_len)
    {
        dlib_len += 10;
        if (!(dlib = realloc (dlib, sizeof (dlib_t) * dlib_len)))
        {
            fprintf (stderr, "%s: Not enough memory: %s.\n", progname, strerror (errno));
            exit (1);
        }
    }

    void *lib;
    if (!(lib = dlmopen (LM_ID_NEWLM, argument, RTLD_LAZY)))
    {
        fprintf (stderr, "%s: Error while opening %s: %s.\n", progname, argument, dlerror ());

        if (interactive)
            return;
        else
            exit (1);
    }

    char *path;
    if (!(path = malloc (len + 1)))
    {
        fprintf (stderr, "%s: Not enough memory: %s.\n", progname, strerror (errno));
        exit (1);
    }
    else
    {
        strncpy (path, argument, len);
        path[len] = 0;
    }

    dlib[dlib_i].handle = lib;
    dlib[dlib_i].path   = path;
    ++dlib_i;
}

void
call (const char *argument)
{
    function symbol = 0;

    for (size_t i = 0; i < dlib_i; ++i)
        if (symbol = (function)dlsym (dlib[i].handle, argument))
            break;

    if (!symbol)
    {
        fprintf (stderr, "%s: No function named %s was found.\n", progname, argument);

        if (interactive)
            return;
        else
            exit (1);
    }

    // call the function in the child process
    pid_t pid = fork ();
    switch (pid)
    {
        case -1:
            fprintf (stderr, "%s: Error while forking: %s.\n", progname, strerror (errno));
            break;
        case 0:
            symbol ();
            exit (0);
        default:
        {
            int status;
            waitpid (pid, &status, 0);  // don't really care for child status - just wait until child terminates
            break;
        }
    }
}

