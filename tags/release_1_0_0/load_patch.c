
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "loki_patch.h"
#include "load_patch.h"
#include "log_output.h"

#define BASE "data"


int load_add_file(FILE *file, int *line_num, const char *dst, loki_patch *patch)
{
    struct op_add_file *op;
    char line[1024];
    char *key, *value;

    /* Allocate memory for the operation */
    op = (struct op_add_file *)malloc(sizeof *op);
    if ( ! op ) {
        log(LOG_ERROR, "Out of memory\n");
        return(-1);
    }
    memset(op, 0, (sizeof *op));

    /* Load the information for this section */
    while ( fgets(line, sizeof(line), file) ) {
        /* Chop the newline */
        ++*line_num;
        line[strlen(line)-1] = '\0';

        /* Is this the end of the section? */
        if ( ! line[0] ) {
            break;
        }

        /* Skip comment lines */
        if ( line[0] == '#' ) {
            continue;
        }

        /* Make sure it's a format we know */
        key = line;
        value = strchr(key, '=');
        if ( ! value ) {
            log(LOG_ERROR, "Unknown patch line %d: %s\n", *line_num, line);
            return(-1);
        }
        *value++ = '\0';

        /* See if we recognize the entry */
        if ( strcmp(key, "src") == 0 ) {
            op->src = strdup(value);
        } else
        if ( strcmp(key, "sum") == 0 ) {
            strncpy(op->sum, value, CHECKSUM_SIZE);
        } else
        if ( strcmp(key, "mode") == 0 ) {
            op->mode = strtol(value, 0, 0);
        } else
        if ( strcmp(key, "size") == 0 ) {
            op->size = strtol(value, 0, 0);
        } else {
            log(LOG_ERROR, "Unknown ADD FILE key %d: %s\n", *line_num, key);
            return(-1);
        }
    }

    /* Make sure we have all the information we need */
    if ( ! op->src || ! *op->sum || ! op->mode ) {
        log(LOG_ERROR, "Incomplete ADD FILE entry above line %d\n", *line_num);
        return(-1);
    }
    op->dst = strdup(dst);

    /* Add the operation to the end of our list */
    if ( patch->add_file_list ) {
        struct op_add_file *here;

        for ( here=patch->add_file_list; here->next; here=here->next )
            ;
        here->next = op;
    } else {
        patch->add_file_list = op;
    }

    return(0);
}

int load_add_path(FILE *file, int *line_num, const char *dst, loki_patch *patch)
{
    struct op_add_path *op;
    char line[1024];
    char *key, *value;

    /* Allocate memory for the operation */
    op = (struct op_add_path *)malloc(sizeof *op);
    if ( ! op ) {
        log(LOG_ERROR, "Out of memory\n");
        return(-1);
    }
    memset(op, 0, (sizeof *op));

    /* Load the information for this section */
    while ( fgets(line, sizeof(line), file) ) {
        /* Chop the newline */
        ++*line_num;
        line[strlen(line)-1] = '\0';

        /* Is this the end of the section? */
        if ( ! line[0] ) {
            break;
        }

        /* Skip comment lines */
        if ( line[0] == '#' ) {
            continue;
        }

        /* Make sure it's a format we know */
        key = line;
        value = strchr(key, '=');
        if ( ! value ) {
            log(LOG_ERROR, "Unknown patch line %d: %s\n", *line_num, line);
            return(-1);
        }
        *value++ = '\0';

        /* See if we recognize the entry */
        if ( strcmp(key, "mode") == 0 ) {
            op->mode = strtol(value, 0, 0);
        } else {
            log(LOG_ERROR, "Unknown ADD PATH key %d: %s\n", *line_num, key);
            return(-1);
        }
    }

    /* Make sure we have all the information we need */
    if ( ! op->mode ) {
        log(LOG_ERROR, "Incomplete ADD PATH entry above line %d\n", *line_num);
        return(-1);
    }
    op->dst = strdup(dst);

    /* Add the operation to the end of our list */
    if ( patch->add_path_list ) {
        struct op_add_path *here;

        for ( here=patch->add_path_list; here->next; here=here->next )
            ;
        here->next = op;
    } else {
        patch->add_path_list = op;
    }

    return(0);
}

/* Note, there are a lot of memory leaks in here if parse fails.
 */
int load_patch_file(FILE *file, int *line_num, const char *dst,
                                            loki_patch *patch)
{
    struct op_patch_file *op;
    char line[1024];
    char *key, *value;
    struct delta_option *option;

    /* Allocate memory for the operation */
    op = (struct op_patch_file *)malloc(sizeof *op);
    if ( ! op ) {
        log(LOG_ERROR, "Out of memory\n");
        return(-1);
    }
    memset(op, 0, (sizeof *op));

    /* Load the information for this section */
    option = (struct delta_option *)0;
    while ( fgets(line, sizeof(line), file) ) {
        /* Chop the newline */
        ++*line_num;
        line[strlen(line)-1] = '\0';

        /* Is this the end of the section? */
        if ( ! line[0] ) {
            break;
        }

        /* Skip comment lines */
        if ( line[0] == '#' ) {
            continue;
        }

        /* Make sure it's a format we know */
        key = line;
        value = strchr(key, '=');
        if ( ! value ) {
            log(LOG_ERROR, "Unknown patch line %d: %s\n", *line_num, line);
            return(-1);
        }
        *value++ = '\0';

        if ( (strcmp(key, "oldsum") == 0) ||
             (strcmp(key, "src") == 0) ||
             (strcmp(key, "newsum") == 0) ) {
            if ( !option ) {
                option = (struct delta_option *)malloc(sizeof *option);
                if ( ! option ) {
                    log(LOG_ERROR, "Out of memory\n");
                    return(-1);
                }
                memset(option, 0, (sizeof *option));
            }
            if ( strcmp(key, "src") == 0 ) {
                if ( option->src ) {
                    log(LOG_ERROR, "Patch option not complete at line %d\n",
                                                                    *line_num);
                    return(-1);
                }
                option->src = strdup(value);
            } else
            if ( strcmp(key, "oldsum") == 0 ) {
                if ( *option->oldsum ) {
                    log(LOG_ERROR, "Patch option not complete at line %d\n",
                                                                    *line_num);
                    return(-1);
                }
                strncpy(option->oldsum, value, CHECKSUM_SIZE);
            } else
            if ( strcmp(key, "newsum") == 0 ) {
                if ( *option->newsum ) {
                    log(LOG_ERROR, "Patch option not complete at line %d\n",
                                                                    *line_num);
                    return(-1);
                }
                strncpy(option->newsum, value, CHECKSUM_SIZE);
            }
            /* If we have a complete entry, add it */
            if ( option->src && *option->oldsum && *option->newsum ) {
                if ( op->options ) {
                    struct delta_option *here;

                    for ( here=op->options; here->next; here=here->next )
                        ;
                    here->next = option;
                } else {
                    op->options = option;
                }
                option = (struct delta_option *)0;
            }
        } else
        if ( strcmp(key, "mode") == 0 ) {
            op->mode = strtol(value, 0, 0);
        } else
        if ( strcmp(key, "size") == 0 ) {
            op->size = strtol(value, 0, 0);
        } else
        if ( strcmp(key, "optional") == 0 ) {
            op->optional = strtol(value, 0, 0);
        } else {
            log(LOG_ERROR, "Unknown PATCH FILE key %d: %s\n", *line_num, key);
            return(-1);
        }
    }

    /* Make sure we have all the information we need */
    if ( ! op->mode || ! op->options || option ) {
        log(LOG_ERROR, "Incomplete PATCH FILE entry above line %d\n",
                                                            *line_num);
        return(-1);
    }
    op->dst = strdup(dst);

    /* Add the operation to the end of our list */
    if ( patch->patch_file_list ) {
        struct op_patch_file *here;

        for ( here=patch->patch_file_list; here->next; here=here->next )
            ;
        here->next = op;
    } else {
        patch->patch_file_list = op;
    }

    return(0);
}

int load_symlink_file(FILE *file, int *line_num, const char *dst, loki_patch *patch)
{
    struct op_symlink_file *op;
    char line[1024];
    char *key, *value;

    /* Allocate memory for the operation */
    op = (struct op_symlink_file *)malloc(sizeof *op);
    if ( ! op ) {
        log(LOG_ERROR, "Out of memory\n");
        return(-1);
    }
    memset(op, 0, (sizeof *op));

    /* Load the information for this section */
    while ( fgets(line, sizeof(line), file) ) {
        /* Chop the newline */
        ++*line_num;
        line[strlen(line)-1] = '\0';

        /* Is this the end of the section? */
        if ( ! line[0] ) {
            break;
        }

        /* Skip comment lines */
        if ( line[0] == '#' ) {
            continue;
        }

        /* Make sure it's a format we know */
        key = line;
        value = strchr(key, '=');
        if ( ! value ) {
            log(LOG_ERROR, "Unknown patch line %d: %s\n", *line_num, line);
            return(-1);
        }
        *value++ = '\0';

        if ( strcmp(key, "link") == 0 ) {
            op->link = strdup(value);
        } else {
            log(LOG_ERROR, "Unknown SYMLINK FILE key %d: %s\n", *line_num, key);
            return(-1);
        }
    }

    /* Make sure we have all the information we need */
    if ( ! op->link ) {
        log(LOG_ERROR, "Incomplete SYMLINK FILE entry above line %d\n",
                                                            *line_num);
        return(-1);
    }
    op->dst = strdup(dst);

    /* Add the operation to the end of our list */
    if ( patch->symlink_file_list ) {
        struct op_symlink_file *here;

        for ( here=patch->symlink_file_list; here->next; here=here->next )
            ;
        here->next = op;
    } else {
        patch->symlink_file_list = op;
    }

    return(0);
}

int load_del_file(FILE *file, int *line_num, const char *dst, loki_patch *patch)
{
    struct op_del_file *op;
    char line[1024];
    char *key, *value;

    /* Allocate memory for the operation */
    op = (struct op_del_file *)malloc(sizeof *op);
    if ( ! op ) {
        log(LOG_ERROR, "Out of memory\n");
        return(-1);
    }
    memset(op, 0, (sizeof *op));

    /* Load the information for this section */
    while ( fgets(line, sizeof(line), file) ) {
        /* Chop the newline */
        ++*line_num;
        line[strlen(line)-1] = '\0';

        /* Is this the end of the section? */
        if ( ! line[0] ) {
            break;
        }

        /* Skip comment lines */
        if ( line[0] == '#' ) {
            continue;
        }

        /* Make sure it's a format we know */
        key = line;
        value = strchr(key, '=');
        if ( ! value ) {
            log(LOG_ERROR, "Unknown patch line %d: %s\n", *line_num, line);
            return(-1);
        }
        *value++ = '\0';

        log(LOG_ERROR, "Unknown DEL FILE key %d: %s\n", *line_num, key);
        return(-1);
    }

    /* Make sure we have all the information we need */
    op->dst = strdup(dst);

    /* Add the operation to the end of our list */
    if ( patch->del_file_list ) {
        struct op_del_file *here;

        for ( here=patch->del_file_list; here->next; here=here->next )
            ;
        here->next = op;
    } else {
        patch->del_file_list = op;
    }

    return(0);
}

int load_del_path(FILE *file, int *line_num, const char *dst, loki_patch *patch)
{
    struct op_del_path *op;
    char line[1024];
    char *key, *value;

    /* Allocate memory for the operation */
    op = (struct op_del_path *)malloc(sizeof *op);
    if ( ! op ) {
        log(LOG_ERROR, "Out of memory\n");
        return(-1);
    }
    memset(op, 0, (sizeof *op));

    /* Load the information for this section */
    while ( fgets(line, sizeof(line), file) ) {
        /* Chop the newline */
        ++*line_num;
        line[strlen(line)-1] = '\0';

        /* Is this the end of the section? */
        if ( ! line[0] ) {
            break;
        }

        /* Skip comment lines */
        if ( line[0] == '#' ) {
            continue;
        }

        /* Make sure it's a format we know */
        key = line;
        value = strchr(key, '=');
        if ( ! value ) {
            log(LOG_ERROR, "Unknown patch line %d: %s\n", *line_num, line);
            return(-1);
        }
        *value++ = '\0';

        log(LOG_ERROR, "Unknown DEL PATH key %d: %s\n", *line_num, key);
        return(-1);
    }

    /* Make sure we have all the information we need */
    op->dst = strdup(dst);

    /* Add the operation to the end of our list */
    if ( patch->del_path_list ) {
        struct op_del_path *here;

        for ( here=patch->del_path_list; here->next; here=here->next )
            ;
        here->next = op;
    } else {
        patch->del_path_list = op;
    }

    return(0);
}

static struct optional_field *add_optional_field(loki_patch *patch,
                                  const char *key, const char *val)
{
    struct optional_field *field, *prev;

    /* Look for the end of the list */
    prev = NULL;
    for ( field = patch->optional_fields; field; field = field->next ) {
        prev = field;
    }

    /* Create a new optional field */
    field = (struct optional_field *)malloc(sizeof *field);
    if ( field ) {
        field->key = strdup(key);
        field->val = strdup(val);
        field->next = NULL;
        if ( ! field->key || ! field->val ) {
            if ( field->key ) {
                free(field->key);
            }
            if ( field->val ) {
                free(field->val);
            }
            free(field);
            field = NULL;
        }
    }

    /* Add it to the list and return it */
    if ( prev ) {
        prev->next = field;
    } else {
        patch->optional_fields = field;
    }
    return(field);
}

static struct {
    const char *key;
    int (*func)(FILE *file, int *line_num, const char *dst, loki_patch *patch);
} load_table[] = {
    {   "ADD FILE ",        load_add_file       },
    {   "ADD PATH ",        load_add_path       },
    {   "PATCH FILE ",      load_patch_file     },
    {   "SYMLINK FILE ",    load_symlink_file   },
    {   "DEL FILE ",        load_del_file       },
    {   "DEL PATH ",        load_del_path       }
};

loki_patch *load_patch(const char *patchfile)
{
    loki_patch *patch;
    FILE *file;
    int i, valid, keylen;
    char line[1024], *token;
    int line_num;

    /* Allocate memory for the patch */
    patch = (loki_patch *)malloc(sizeof *patch);
    if ( ! patch ) {
        return (loki_patch *)0;
    }
    memset(patch, 0, (sizeof *patch));

    /* Try to open the patch file */
    file = fopen(patchfile, "r");
    if ( ! file ) {
        log(LOG_ERROR, "Unable to open patch file: %s\n", patchfile);
        free_patch(patch);
        return (loki_patch *)0;
    }

    /* Get the data directory for the patch */
    if ( strrchr(patchfile, '/') != NULL ) {
        patch->base = (char *)malloc(strlen(patchfile) + strlen("/" BASE) + 1);
        if ( ! patch->base ) {
            log(LOG_ERROR, "Out of memory\n");
            free_patch(patch);
            return (loki_patch *)0;
        }
        strcpy(patch->base, patchfile);
        *strrchr(patch->base, '/') = '\0';
        strcat(patch->base, "/" BASE);
    } else {
        patch->base = (char *)malloc(strlen("./" BASE) + 1);
        if ( ! patch->base ) {
            log(LOG_ERROR, "Out of memory\n");
            free_patch(patch);
            return (loki_patch *)0;
        }
        strcpy(patch->base, "./" BASE);
    }

    /* Load the patch header */
    line_num = 0;
    while ( fgets(line, sizeof(line), file) ) {
        /* Chop the newline */
        line_num++;
        line[strlen(line)-1] = '\0';

        /* Is this the end of the header? */
        if ( ! line[0] ) {
            break;
        }

        /* Skip comment lines */
        if ( line[0] == '#' ) {
            continue;
        }

        /* Parse the line into tokens */
        for ( token=line; *token; ++token ) {
            if ( *token == ':' ) {
                *token++ = '\0';
                while ( isspace(*token) ) {
                    ++token;
                }
                break;
            }
        }

        /* See if we recognize the token */
        if ( strcasecmp(line, "Product") == 0 ) {
            patch->product = strdup(token);
        } else
        if ( strcasecmp(line, "Component") == 0 ) {
            if ( *token ) {
                patch->component = strdup(token);
            }
        } else
        if ( strcasecmp(line, "Version") == 0 ) {
            patch->version = strdup(token);
        } else
        if ( strcasecmp(line, "Size") == 0 ) {
            /* Discard the size attribute - it's recalculated */ ;
        } else
        if ( strcasecmp(line, "Prepatch") == 0 ) {
            patch->prepatch = strdup(token);
        } else
        if ( strcasecmp(line, "Postpatch") == 0 ) {
            patch->postpatch = strdup(token);
        } else {
            if ( ! add_optional_field(patch, line, token) ) {
                log(LOG_ERROR, "Out of memory\n");
                free_patch(patch);
                return (loki_patch *)0;
            }
        }
    }

    /* Verify that the header is complete */
    if ( ! patch->product || ! patch->version ) {
        log(LOG_ERROR,
    "The patch file doesn't contain a complete patch header:\n"
    SAMPLE_HEADER
        );
        free_patch(patch);
        return (loki_patch *)0;
    }

    /* Special token, version checking */
    fgets(line, sizeof(line), file);
    if ( (line[0] != '%') ||
         (strncmp(&line[1], LOKI_VERSION, strlen(LOKI_VERSION)) != 0) ) {
        log(LOG_ERROR, "Patch version doesn't match "LOKI_VERSION"\n");
        free_patch(patch);
        return (loki_patch *)0;
    }
        
    /* Load the current set of operations */
    while ( fgets(line, sizeof(line), file) ) {
        /* Chop the newline */
        line_num++;
        line[strlen(line)-1] = '\0';

        /* Skip blank and comment lines */
        if ( ! line[0] || (line[0] == '#') ) {
            continue;
        }

        /* Look for a recognized operation */
        valid = 0;
        for ( i=0; i<(sizeof load_table)/(sizeof load_table[0]); ++i ) {
            keylen = strlen(load_table[i].key);
            if ( strncmp(line, load_table[i].key, keylen) == 0 ) {
                if (load_table[i].func(file,&line_num,line+keylen,patch) < 0) {
                    free_patch(patch);
                    return (loki_patch *)0;
                }
                valid = 1;
                break;
            }
        }
        if ( ! valid ) {
            log(LOG_ERROR, "Unknown section header at line %d\n", line_num);
            free_patch(patch);
            return (loki_patch *)0;
        }
    }

    /* We're done! */
    return patch;
}

static void free_optional_fields(struct optional_field *fields)
{
    struct optional_field *field, *freeable;

    field = fields;
    while ( field ) {
        freeable = field;
        field = field->next;
        free(freeable->key);
        free(freeable->val);
        free(freeable);
    }
}

void free_add_path(struct op_add_path *add_path_list)
{
    struct op_add_path *freeable;

    while ( add_path_list ) {
        freeable = add_path_list;
        add_path_list = add_path_list->next;
        free(freeable->dst);
        free(freeable);
    }
}

void free_add_file(struct op_add_file *add_file_list)
{
    struct op_add_file *freeable;

    while ( add_file_list ) {
        freeable = add_file_list;
        add_file_list = add_file_list->next;
        free(freeable->dst);
        free(freeable->src);
        free(freeable);
    }
}

void free_patch_file(struct op_patch_file *patch_file_list)
{
    struct op_patch_file *freeable;
    struct delta_option *option, *o;

    while ( patch_file_list ) {
        freeable = patch_file_list;
        patch_file_list = patch_file_list->next;
        option=freeable->options;
        while ( option ) {
            o = option;
            option = option->next;
            if ( o->src ) {
                free(o->src);
            }
            free(o);
        }
        free(freeable->dst);
        free(freeable);
    }
}

void free_symlink_file(struct op_symlink_file *symlink_file_list)
{
    struct op_symlink_file *freeable;

    while ( symlink_file_list ) {
        freeable = symlink_file_list;
        symlink_file_list = symlink_file_list->next;
        free(freeable->dst);
        free(freeable->link);
        free(freeable);
    }
}

void free_del_file(struct op_del_file *del_file_list)
{
    struct op_del_file *freeable;

    while ( del_file_list ) {
        freeable = del_file_list;
        del_file_list = del_file_list->next;
        free(freeable->dst);
        free(freeable);
    }
}

void free_del_path(struct op_del_path *del_path_list)
{
    struct op_del_path *freeable;

    while ( del_path_list ) {
        freeable = del_path_list;
        del_path_list = del_path_list->next;
        free(freeable->dst);
        free(freeable);
    }
}

static void free_removed_paths(struct removed_path *removed_paths)
{
    struct removed_path *freeable;

    while ( removed_paths ) {
        freeable = removed_paths;
        removed_paths = removed_paths->next;
        if ( freeable->path ) {
            free(freeable->path);
        }
        free(freeable);
    }
}

void free_patch(loki_patch *patch)
{
    if ( patch ) {
        if ( patch->base ) {
            free(patch->base);
        }
        if ( patch->product ) {
            free(patch->product);
        }
        if ( patch->version ) {
            free(patch->version);
        }
        if ( patch->component ) {
            free(patch->component);
        }
        free_optional_fields(patch->optional_fields);
        if ( patch->prepatch ) {
            free(patch->prepatch);
        }
        if ( patch->postpatch ) {
            free(patch->postpatch);
        }
        free_add_path(patch->add_path_list);
        free_add_file(patch->add_file_list);
        free_patch_file(patch->patch_file_list);
        free_symlink_file(patch->symlink_file_list);
        free_del_file(patch->del_file_list);
        free_del_path(patch->del_path_list);
        free_removed_paths(patch->removed_paths);
        free(patch);
    }
}
