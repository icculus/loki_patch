
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "loki_patch.h"
#include "load_patch.h"
#include "tree_patch.h"
#include "save_patch.h"
#include "log_output.h"

static void print_usage(const char *argv0)
{
    fprintf(stderr,
"Loki Patch Tools " VERSION "\n");
    fprintf(stderr,
"Usage: %s patch-file command arguments\n"
"Where command and arguments are one of:\n"
"   delta-install old-tree1 [old-tree2] [old-tree3] new-tree\n"
"   delta-file old-file new-file installed-name\n"
"   add-tarfile tar-archive\n"
"   add-path new-path installed-name\n"
"   add-file new-file installed-name\n"
"   symlink-file link installed-name\n"
"   del-path installed-path\n"
"   del-file installed-file\n"
"   load-file commands-file\n",
    argv0);
}

/* Split a line into arguments */
static int split_line(char *line, char **args)
{
    int argc;
    char sep[2];
    char *end;

    argc = 0;
    while ( *line ) {
        /* Skip leading spaces */
        while ( isspace(*line) ) {
            ++line;
        }
        if ( ! *line ) {
            break;
        }
        /* Set up the initial argument */
        if ( *line == '"' ) {
            /* Skip past quote */
            ++line;
            sep[0] = '"';
            sep[1] = '\0';
        } else {
            sep[0] = ' ';
            sep[1] = '\t';
        }
        /* Set the argument pointer */
        if ( args ) {
            args[argc] = line;
        }
        /* Skip to next unescaped separator */
        while ( *line &&
               (((*line != sep[0]) && (*line != sep[1])) ||
                (*(line-1) == '\\')) ) {
            ++line;
        }
        end = line;
        /* Skip separator character */
        if ( *line ) {
            ++line;
        }
        /* Terminate the argument pointer */
        if ( args ) {
            *end = '\0';
        }
        ++argc;
    }
    if ( args ) {
        args[argc] = (char *)0;
    }
    return(argc);
}

static int interpret_args(const char *argv0, int argc, char *args[],
                                                    loki_patch *patch)
{
    if ( strcmp(args[0], "delta-install") == 0 ) {
        int i, result;

        if ( argc < 3 ) {
            fprintf(stderr, "delta-install requires at least two arguments\n");
            print_usage(argv0);
            return(-1);
        }
        result = 0;
        for ( i=1; (result == 0) && i < (argc-1); ++i ) {
            printf("delta-install %s %s\n", args[i], args[argc-1]);
            result = tree_patch(args[i], "", args[argc-1], "", patch);
        }
        return(result);
    }

    if ( strcmp(args[0], "delta-file") == 0 ) {
        if ( argc != 4 ) {
            fprintf(stderr, "delta-file requires 3 arguments\n");
            print_usage(argv0);
            return(-1);
        }
        printf("delta-file %s %s %s\n", args[1], args[2], args[3]);
        return tree_patch_file(args[1], args[2], args[3], patch);
    }

    if ( strcmp(args[0], "add-tarfile") == 0 ) {
        if ( argc != 2 ) {
            fprintf(stderr, "add-tarfile requires an argument\n");
            print_usage(argv0);
            return(-1);
        }
        printf("add-tarfile %s\n", args[1]);
        return tree_tarfile(args[1], patch);
    }

    if ( strcmp(args[0], "add-path") == 0 ) {
        if ( argc != 3 ) {
            fprintf(stderr, "add-path requires 2 arguments\n");
            print_usage(argv0);
            return(-1);
        }
        printf("add-path %s %s\n", args[1], args[2]);
        return tree_add_path(args[1], args[2], patch);
    }

    if ( strcmp(args[0], "add-file") == 0 ) {
        if ( argc != 3 ) {
            fprintf(stderr, "add-file requires 2 arguments\n");
            print_usage(argv0);
            return(-1);
        }
        printf("add-file %s %s\n", args[1], args[2]);
        return tree_add_file(args[1], args[2], patch);
    }

    if ( strcmp(args[0], "symlink-file") == 0 ) {
        if ( argc != 3 ) {
            fprintf(stderr, "symlink-file requires 2 arguments\n");
            print_usage(argv0);
            return(-1);
        }
        printf("symlink-file %s %s\n", args[1], args[2]);
        return tree_symlink_file(args[1], args[2], patch);
    }

    if ( strcmp(args[0], "del-path") == 0 ) {
        if ( argc != 2 ) {
            fprintf(stderr, "del-path requires 1 argument\n");
            print_usage(argv0);
            return(-1);
        }
        printf("del-path %s\n", args[1]);
        return tree_del_path(args[1], patch);
    }

    if ( strcmp(args[0], "del-file") == 0 ) {
        if ( argc != 2 ) {
            fprintf(stderr, "del-file requires 1 argument\n");
            print_usage(argv0);
            return(-1);
        }
        printf("del-file %s\n", args[1]);
        return tree_del_file(args[1], patch);
    }

    if ( strcmp(args[0], "load-file") == 0 ) {
        int result;
        FILE *file;
        char line[1024];
        int file_argc;
        char **file_args = (char **)0;

        if ( argc != 2 ) {
            fprintf(stderr, "load-file requires 1 argument\n");
            print_usage(argv0);
            return(-1);
        }
        file = fopen(args[1], "r");
        if ( ! file ) {
            fprintf(stderr, "Unable to read %s\n", args[1]);
            return(-1);
        }
        result = 0;
        while ( (result == 0) && fgets(line, sizeof(line), file) ) {
            /* Trim newlines */
            line[strlen(line)-1] = '\0';

            /* See how many arguments there are */
            file_argc = split_line(line, (char **)0);

            /* Skip blank/comment lines */
            if ( ! file_argc || (line[0] == '#') ) {
                continue;
            }

            /* Parse and execute the line */
            file_args =(char **)realloc(file_args,(file_argc+1)*sizeof(char *));
            file_argc = split_line(line, file_args);
            result = interpret_args(argv0, file_argc, file_args, patch);
        }
        fclose(file);        
        return(result);
    }

    /* No matches */
    fprintf(stderr, "Unknown command: %s\n", args[0]);
    print_usage(argv0);
    return(-1);
}

int main(int argc, char *argv[])
{
    loki_patch *patch;

    set_logging(LOG_VERBOSE);
    if ( argc < 3 ) {
        print_usage(argv[0]);
        exit(1);
    }
    patch = load_patch(argv[1]);
    if ( ! patch ) {
        exit(2);
    }

    if ( interpret_args(argv[0], argc-2, argv+2, patch) < 0 ) {
        exit(3);
    }

    if ( save_patch(patch, argv[1]) ) {
        exit(4);
    }
    free_patch(patch);

    return(0);
}
