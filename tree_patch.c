
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <zlib.h>

#include "loki_patch.h"
#include "load_patch.h"
#include "tree_patch.h"
#include "loki_xdelta.h"
#include "mkdirhier.h"
#include "md5.h"
#include "log_output.h"


/* Remove a path from the specified portion of the patch
 */
static void remove_path(patch_op op, const char *dst, loki_patch *patch)
{
    char path[PATH_MAX];

    switch (op) {
        case OP_NONE: {
            remove_path(OP_ADD_PATH, dst, patch);
            remove_path(OP_ADD_FILE, dst, patch);
            remove_path(OP_DEL_PATH, dst, patch);
            remove_path(OP_DEL_FILE, dst, patch);
            remove_path(OP_PATCH_FILE, dst, patch);
            remove_path(OP_SYMLINK_FILE, dst, patch);
        }
        break;

        case OP_ADD_PATH: {
            struct op_add_path *elem, *prev, *freeable;

            prev = NULL;
            elem = patch->add_path_list;
            while ( elem ) {
                if ( strcmp(elem->dst, dst) == 0 ) {
                    freeable = elem;
                    elem = elem->next;
                    if ( prev ) {
                        prev->next = elem;
                    } else {
                        patch->add_path_list = elem;
                    }
                    freeable->next = NULL;
                    free_add_path(freeable);
                } else {
                    prev = elem;
                    elem = elem->next;
                }
            }
        }
        break;
        
        case OP_ADD_FILE: {
            struct op_add_file *elem, *prev, *freeable;

            prev = NULL;
            elem = patch->add_file_list;
            while ( elem ) {
                if ( strcmp(elem->dst, dst) == 0 ) {
                    freeable = elem;
                    elem = elem->next;
                    if ( prev ) {
                        prev->next = elem;
                    } else {
                        patch->add_file_list = elem;
                    }
                    freeable->next = NULL;
                    sprintf(path, "%s/%s", patch->base, freeable->src);
                    unlink(path);
                    free_add_file(freeable);
                } else {
                    prev = elem;
                    elem = elem->next;
                }
            }
        }
        break;

        case OP_DEL_PATH: {
            struct op_del_path *elem, *prev, *freeable;

            prev = NULL;
            elem = patch->del_path_list;
            while ( elem ) {
                if ( strcmp(elem->dst, dst) == 0 ) {
                    freeable = elem;
                    elem = elem->next;
                    if ( prev ) {
                        prev->next = elem;
                    } else {
                        patch->del_path_list = elem;
                    }
                    freeable->next = NULL;
                    free_del_path(freeable);
                } else {
                    prev = elem;
                    elem = elem->next;
                }
            }
        }
        break;
        
        case OP_DEL_FILE: {
            struct op_del_file *elem, *prev, *freeable;

            prev = NULL;
            elem = patch->del_file_list;
            while ( elem ) {
                if ( strcmp(elem->dst, dst) == 0 ) {
                    freeable = elem;
                    elem = elem->next;
                    if ( prev ) {
                        prev->next = elem;
                    } else {
                        patch->del_file_list = elem;
                    }
                    freeable->next = NULL;
                    free_del_file(freeable);
                } else {
                    prev = elem;
                    elem = elem->next;
                }
            }
        }
        break;
        
        case OP_PATCH_FILE: {
            struct op_patch_file *elem, *prev, *freeable;

            prev = NULL;
            elem = patch->patch_file_list;
            while ( elem ) {
                if ( strcmp(elem->dst, dst) == 0 ) {
                    struct delta_option *here;

                    freeable = elem;
                    elem = elem->next;
                    if ( prev ) {
                        prev->next = elem;
                    } else {
                        patch->patch_file_list = elem;
                    }
                    freeable->next = NULL;
                    for ( here=elem->options; here; here=here->next ) {
                        sprintf(path, "%s/%s", patch->base, here->src);
                        unlink(path);
                    }
                    free_patch_file(freeable);
                } else {
                    prev = elem;
                    elem = elem->next;
                }
            }
        }
        break;
        
        case OP_SYMLINK_FILE: {
            struct op_symlink_file *elem, *prev, *freeable;

            prev = NULL;
            elem = patch->symlink_file_list;
            while ( elem ) {
                if ( strcmp(elem->dst, dst) == 0 ) {
                    freeable = elem;
                    elem = elem->next;
                    if ( prev ) {
                        prev->next = elem;
                    } else {
                        patch->symlink_file_list = elem;
                    }
                    freeable->next = NULL;
                    free_symlink_file(freeable);
                } else {
                    prev = elem;
                    elem = elem->next;
                }
            }
        }
        break;
    }
}

/* See if a path is already in the patch list for the specified operation
 */
static int is_in_patch(patch_op op, const char *dst, loki_patch *patch)
{
    int in_patch;

    in_patch = 0;
    switch (op) {
        case OP_NONE: {
            in_patch += is_in_patch(OP_ADD_PATH, dst, patch);
            in_patch += is_in_patch(OP_ADD_FILE, dst, patch);
            in_patch += is_in_patch(OP_DEL_PATH, dst, patch);
            in_patch += is_in_patch(OP_DEL_FILE, dst, patch);
            in_patch += is_in_patch(OP_PATCH_FILE, dst, patch);
            in_patch += is_in_patch(OP_SYMLINK_FILE, dst, patch);
        }
        break;

        case OP_ADD_PATH: {
            struct op_add_path *elem;

            for (elem=patch->add_path_list; elem && !in_patch; elem=elem->next){
                if ( strcmp(elem->dst, dst) == 0 ) {
                    ++in_patch;
                }
            }
        }
        break;
        
        case OP_ADD_FILE: {
            struct op_add_file *elem;

            for (elem=patch->add_file_list; elem && !in_patch; elem=elem->next){
                if ( strcmp(elem->dst, dst) == 0 ) {
                    ++in_patch;
                }
            }
        }
        break;

        case OP_DEL_PATH: {
            struct op_del_path *elem;

            for (elem=patch->del_path_list; elem && !in_patch; elem=elem->next){
                if ( strcmp(elem->dst, dst) == 0 ) {
                    ++in_patch;
                }
            }
        }
        break;
        
        case OP_DEL_FILE: {
            struct op_del_file *elem;

            for (elem=patch->del_file_list; elem && !in_patch; elem=elem->next){
                if ( strcmp(elem->dst, dst) == 0 ) {
                    ++in_patch;
                }
            }
        }
        break;
        
        case OP_PATCH_FILE: {
            struct op_patch_file *elem;

            for (elem=patch->patch_file_list; elem&&!in_patch; elem=elem->next){
                if ( strcmp(elem->dst, dst) == 0 ) {
                    ++in_patch;
                }
            }
        }
        break;
        
        case OP_SYMLINK_FILE: {
            struct op_symlink_file *elem;

            for (elem=patch->symlink_file_list;elem&&!in_patch;elem=elem->next){
                if ( strcmp(elem->dst, dst) == 0 ) {
                    ++in_patch;
                }
            }
        }
        break;
    }
    return in_patch;
}

int tree_add_file(const char *path, const char *dst, loki_patch *patch)
{
    struct op_add_file *op;
    char pat_path[PATH_MAX];
    struct stat sb;
    FILE *src_fp;
    gzFile pat_zfp;
    int len;
    char data[4096];

    /* See if the file is a symbolic link, and add it, if so */
    if ( lstat(path, &sb) < 0 ) {
        logme(LOG_ERROR, "Unable to stat %s\n", path);
        return(-1);
    }
    if ( S_ISLNK(sb.st_mode) ) {
        int i;
        char link[PATH_MAX];
        
        i = readlink(path, link, sizeof(link)-1);
        if ( i < 0 ) {
            logme(LOG_ERROR, "Unable to read symlink %s\n", path);
            return(-1);
        }
        link[i] = '\0';
        return tree_symlink_file(link, dst, patch);
    }

    logme(LOG_VERBOSE, "-> ADD FILE %s\n", dst);

    /* See if the path is used by any other portion of the patch */
    remove_path(OP_ADD_FILE, dst, patch);
    remove_path(OP_SYMLINK_FILE, dst, patch);
    remove_path(OP_PATCH_FILE, dst, patch);     /* add supercedes patch */
    if ( is_in_patch(OP_NONE, dst, patch) ) {
        logme(LOG_ERROR, "Path %s is already in patch\n", dst);
        return(-1);
    }

    /* Allocate memory for the operation */
    op = (struct op_add_file *)malloc(sizeof *op);
    if ( ! op ) {
        logme(LOG_ERROR, "Out of memory\n");
        return(-1);
    }

    /* Copy the file to the patch directory */
    sprintf(pat_path, "%s/%s", patch->base, dst);
    if ( mkdirhier(pat_path) < 0 ) {
        free(op);
        return(-1);
    }
    src_fp = fopen(path, "rb");
    if ( src_fp == NULL ) {
        logme(LOG_ERROR, "Unable to open %s\n", path);
        free(op);
        return(-1);
    }
    pat_zfp = gzopen(pat_path, "wb9");
    if ( pat_zfp == NULL ) {
        logme(LOG_ERROR, "Unable to open %s\n", path);
        fclose(src_fp);
        free(op);
        return(-1);
    }
    while ( (len=fread(data, 1, sizeof(data), src_fp)) > 0 ) {
        if ( gzwrite(pat_zfp, data, len) != len ) {
            logme(LOG_ERROR, "Error writing patch data: %s\n", strerror(errno));
            fclose(src_fp);
            gzclose(pat_zfp);
            free(op);
            return(-1);
        }
    }
    fclose(src_fp);
    if ( gzclose(pat_zfp) != Z_OK ) {
        logme(LOG_ERROR, "Error writing patch data: %s\n", strerror(errno));
    }

    /* Put it all together now */
    op->dst = strdup(dst);
    op->src = strdup(dst);
    op->mode = sb.st_mode;
    op->size = sb.st_size;
    md5_compute(path, op->sum, 1);
    op->next = patch->add_file_list;
    patch->add_file_list = op;

    return(0);
}

int tree_add_path(const char *path, const char *dst, loki_patch *patch)
{
    int is_toplevel;
    struct op_add_path *op;
    char child_path[PATH_MAX];
    char child_dst[PATH_MAX];
    struct stat sb;
    DIR *dir;
    struct dirent *entry;

    /* See if we're adding to the toplevel directory */
    is_toplevel = (!dst || !*dst || (strcmp(dst, ".") == 0));

    if ( ! is_toplevel ) {
        logme(LOG_VERBOSE, "-> ADD PATH %s\n", dst);
    }

    /* See if the path is used by any other portion of the patch */
    if ( ! is_toplevel ) {
        remove_path(OP_ADD_PATH, dst, patch);
        if ( is_in_patch(OP_NONE, dst, patch) ) {
            logme(LOG_ERROR, "Path %s is already in patch\n", dst);
            return(-1);
        }
    }

    /* Get the mode information for the path */
    if ( stat(path, &sb) < 0 ) {
        logme(LOG_ERROR, "Unable to stat %s\n", path);
        free(op);
        return(-1);
    }

    if ( ! is_toplevel ) {
        /* Allocate memory for the operation */
        op = (struct op_add_path *)malloc(sizeof *op);
        if ( ! op ) {
            logme(LOG_ERROR, "Out of memory\n");
            return(-1);
        }

        /* Put it all together now */
        op->dst = strdup(dst);
        op->mode = sb.st_mode;
        if ( patch->add_path_list ) {
            struct op_add_path *here;
            /* Insert the directory at the end of the list, so that
               directories are created in the correct order.
             */
            for ( here=patch->add_path_list; here->next; here=here->next )
                ;
            op->next = here->next;
            here->next = op;
        } else {
            patch->add_path_list = op;
        }
        op->next = (struct op_add_path *)0;
    }

    /* Now add everything in the path */
    dir = opendir(path);
    if ( ! dir ) {
        logme(LOG_ERROR, "Unable to list %s\n", path);
        return(-1);
    }
    while ( (entry=readdir(dir)) != NULL ) {
        /* Skip "." and ".." entries */
        if ( (strcmp(entry->d_name, ".") == 0) ||
             (strcmp(entry->d_name, "..") == 0) ) {
            continue;
        }

        /* Add the child path */
        sprintf(child_path, "%s/%s", path, entry->d_name);
        if ( is_toplevel ) {
            strcpy(child_dst, entry->d_name);
        } else {
            sprintf(child_dst, "%s/%s", dst, entry->d_name);
        }
        if ( stat(child_path, &sb) < 0 ) {
            logme(LOG_ERROR, "Unable to stat %s\n", child_path);
            return(-1);
        }
        if ( S_ISDIR(sb.st_mode) ) {
            if ( tree_add_path(child_path, child_dst, patch) < 0 ) {
                return(-1);
            }
        } else {
            if ( tree_add_file(child_path, child_dst, patch) < 0 ) {
                return(-1);
            }
        }
    }
    closedir(dir);

    /* We're finally done */
    return(0);
}

int tree_patch_file(const char *o_path,
                    const char *n_path, const char *dst, loki_patch *patch)
{
    struct op_patch_file *op;
    struct delta_option *option;
    char oldsum[CHECKSUM_SIZE+1];
    char newsum[CHECKSUM_SIZE+1];
    struct stat old_sb, new_sb;
    struct stat sb;
    int i;
    char pat_path[PATH_MAX];

    /* See if either of the files are symbolic links */
    if ( lstat(o_path, &old_sb) < 0 ) {
        logme(LOG_ERROR, "Unable to stat %s\n", o_path);
        return(-1);
    }
    if ( lstat(n_path, &new_sb) < 0 ) {
        logme(LOG_ERROR, "Unable to stat %s\n", n_path);
        return(-1);
    }
    /* New file is symlink, old file is not, then add symlink */
    if ( S_ISLNK(new_sb.st_mode) && !S_ISLNK(old_sb.st_mode) ) {
        char link[PATH_MAX];
        
        i = readlink(n_path, link, sizeof(link)-1);
        if ( i < 0 ) {
            logme(LOG_ERROR, "Unable to read symlink %s\n", n_path);
            return(-1);
        }
        link[i] = '\0';
        return tree_symlink_file(link, dst, patch);
    }
    /* Old file is symlink, new file is not, then add file */
    if ( S_ISLNK(old_sb.st_mode) && !S_ISLNK(new_sb.st_mode) ) {
        return tree_add_file(n_path, dst, patch);
    }
    /* Both files are links, see if they are the same links */
    if ( S_ISLNK(new_sb.st_mode) && S_ISLNK(old_sb.st_mode) ) {
        char old_link[PATH_MAX];
        char new_link[PATH_MAX];
        
        i = readlink(o_path, old_link, sizeof(old_link)-1);
        if ( i < 0 ) {
            logme(LOG_ERROR, "Unable to read symlink %s\n", o_path);
            return(-1);
        }
        old_link[i] = '\0';

        i = readlink(n_path, new_link, sizeof(new_link)-1);
        if ( i < 0 ) {
            logme(LOG_ERROR, "Unable to read symlink %s\n", n_path);
            return(-1);
        }
        new_link[i] = '\0';

        /* If they are the same, nothing to do, otherwise create new link */
        if ( strcmp(old_link, new_link) == 0 ) {
            return(0);
        }
        return tree_symlink_file(new_link, dst, patch);
    }
    /* Okay, neither file is a symbolic link, continue.. */

    /* Make sure the files to compare are readable */
    if ( access(o_path, R_OK) < 0 ) {
        logme(LOG_ERROR, "Unable to read %s\n", o_path);
        return(-1);
    }
    if ( access(n_path, R_OK) < 0 ) {
        logme(LOG_ERROR, "Unable to read %s\n", n_path);
        return(-1);
    }

    /* A previous add supercedes a new patch */
    if ( is_in_patch(OP_ADD_FILE, dst, patch) ) {
        return(0);
    }

    /* See if we need to generate a delta */
    md5_compute(o_path, oldsum, 1);
    md5_compute(n_path, newsum, 1);
    if ( strcmp(oldsum, newsum) == 0 ) {
        struct op_patch_file *elem;
        /* They are the same file - if there is already a delta for this,
           then it becomes an optional delta, since we may be applying a
           patch to both this file and the other, different, file.
         */
        for ( elem = patch->patch_file_list; elem; elem = elem->next ) {
            if ( strcmp(elem->dst, dst) == 0 ) {
                elem->optional = 1;
                break;
            }
        }
        return(0);
    }

    /* See if we already have this delta in our patch */
    for ( op = patch->patch_file_list; op; op=op->next ) {
        if ( strcmp(op->dst, dst) == 0 ) {
            struct delta_option *here;

            for ( here=op->options; here; here=here->next ) {
                if ( (strcmp(here->oldsum, oldsum) == 0) &&
                     (strcmp(here->newsum, newsum) == 0) ) {
                    /* This delta is already in the patch, oh well.. */
                    return(0);
                }
            }
        }
    }

    logme(LOG_VERBOSE, "-> PATCH FILE %s\n", dst);

    /* We can have multiple "PATCH FILE" entries, but no other kind */
    if ( is_in_patch(OP_ADD_PATH, dst, patch) ||
         is_in_patch(OP_DEL_PATH, dst, patch) ||
         is_in_patch(OP_DEL_FILE, dst, patch) ||
         is_in_patch(OP_SYMLINK_FILE, dst, patch) ) {
        logme(LOG_ERROR, "Path %s is already in patch\n", dst);
        return(-1);
    }

    /* Allocate memory for the operation, if needed */
    for ( op = patch->patch_file_list; op; op=op->next ) {
        if ( strcmp(op->dst, dst) == 0 ) {
            break;
        }
    }
    if ( ! op ) {
        op = (struct op_patch_file *)malloc(sizeof *op);
        if ( ! op ) {
            logme(LOG_ERROR, "Out of memory\n");
            return(-1);
        }
        op->dst = strdup(dst);
        op->options = (struct delta_option *)0;
        op->mode = 0;
        op->size = 0;
        op->optional = 0;
        op->next = patch->patch_file_list;
        patch->patch_file_list = op;
    }

    /* The patch size is the size of the largest output file */
    if ( stat(n_path, &sb) < 0 ) {
        logme(LOG_ERROR, "Unable to stat %s\n", n_path);
        return(-1);
    }
    if ( op->size < sb.st_size ) {
        op->size = sb.st_size;
    }
    op->mode = sb.st_mode;

    /* Allocate memory for the option */
    option = (struct delta_option *)malloc(sizeof *option);
    if ( ! option ) {
        logme(LOG_ERROR, "Out of memory\n");
        return(-1);
    }
    if ( op->options ) {
        struct delta_option *here;

        for ( here=op->options; here->next; here=here->next )
            ;
        here->next = option;
    } else {
        op->options = option;
    }
    option->src = (char *)0;
    strcpy(option->oldsum, oldsum);
    strcpy(option->newsum, newsum);
    option->next = (struct delta_option *)0;

    /* Generate a delta between the two versions */
    i = 0;
    sprintf(pat_path, "%s/%s.%d", patch->base, dst, i);
    if ( mkdirhier(pat_path) < 0 ) {
        return(-1);
    }
    while ( stat(pat_path, &sb) == 0 ) {
        sprintf(pat_path, "%s/%s.%d", patch->base, dst, ++i);
    }
    if ( loki_xdelta(o_path, n_path, pat_path) < 0 ) {
        logme(LOG_ERROR, "Failed delta between %s and %s\n", o_path, n_path);
        return(-1);
    }
    option->src = strdup(pat_path+strlen(patch->base)+1);

    /* We're done, successful delta */
    return(0);
}

int tree_symlink_file(const char *link, const char *dst, loki_patch *patch)
{
    struct op_symlink_file *op;

    logme(LOG_VERBOSE, "-> SYMLINK FILE %s -> %s\n", dst, link);

    /* See if the path is used by any other portion of the patch */
    remove_path(OP_ADD_FILE, dst, patch);
    remove_path(OP_SYMLINK_FILE, dst, patch);
    if ( is_in_patch(OP_NONE, dst, patch) ) {
        logme(LOG_ERROR, "Path %s is already in patch\n", dst);
        return(-1);
    }

    /* Allocate memory for the operation */
    op = (struct op_symlink_file *)malloc(sizeof *op);
    if ( ! op ) {
        logme(LOG_ERROR, "Out of memory\n");
        return(-1);
    }

    /* Put it all together now */
    op->dst = strdup(dst);
    op->link = strdup(link);
    op->next = patch->symlink_file_list;
    patch->symlink_file_list = op;

    return(0);
}

int tree_del_path(const char *dst, loki_patch *patch)
{
    struct op_del_path *op;
    char path[PATH_MAX];
    int pathlen;

    logme(LOG_VERBOSE, "-> DEL PATH %s\n", dst);

    /* Need to make sure that this path isn't part of any of the path */
    remove_path(OP_DEL_PATH, dst, patch);
    sprintf(path, "%s/", dst);
    pathlen = strlen(path);
    {
        struct op_add_path *elem;

        for (elem=patch->add_path_list; elem; elem=elem->next){
            if ( (strcmp(elem->dst, dst) == 0) ||
                 (strncmp(elem->dst, path, pathlen) == 0) ) {
                logme(LOG_ERROR,
"Can't delete path %s, used by ADD PATH %s\n", dst, elem->dst);
                return(-1);
            }
        }
    }
    {
        struct op_add_file *elem;

        for (elem=patch->add_file_list; elem; elem=elem->next){
            if ( (strcmp(elem->dst, dst) == 0) ||
                 (strncmp(elem->dst, path, pathlen) == 0) ) {
                logme(LOG_ERROR,
"Can't delete path %s, used by ADD FILE %s\n", dst, elem->dst);
                return(-1);
            }
        }
    }
    {
        struct op_del_file *elem;

        for (elem=patch->del_file_list; elem; elem=elem->next){
            if ( (strcmp(elem->dst, dst) == 0) ||
                 (strncmp(elem->dst, path, pathlen) == 0) ) {
                logme(LOG_ERROR,
"Can't delete path %s, used by DEL FILE %s\n", dst, elem->dst);
                return(-1);
            }
        }
    }
    {
        struct op_patch_file *elem;

        for (elem=patch->patch_file_list; elem; elem=elem->next){
            if ( (strcmp(elem->dst, dst) == 0) ||
                 (strncmp(elem->dst, path, pathlen) == 0) ) {
                logme(LOG_ERROR,
"Can't delete path %s, used by PATCH FILE %s\n", dst, elem->dst);
                return(-1);
            }
        }
    }

    /* Allocate memory for the operation */
    op = (struct op_del_path *)malloc(sizeof *op);
    if ( ! op ) {
        logme(LOG_ERROR, "Out of memory\n");
        return(-1);
    }

    /* Put it all together now */
    op->dst = strdup(dst);
    op->next = patch->del_path_list;
    patch->del_path_list = op;

    return(0);
}

int tree_del_file(const char *dst, loki_patch *patch)
{
    struct op_del_file *op;

    logme(LOG_VERBOSE, "-> DEL FILE %s\n", dst);

    /* See if the path is used by any other portion of the patch */
    remove_path(OP_DEL_FILE, dst, patch);
    if ( is_in_patch(OP_NONE, dst, patch) ) {
        logme(LOG_ERROR, "Path %s is already in patch\n", dst);
        return(-1);
    }

    /* Allocate memory for the operation */
    op = (struct op_del_file *)malloc(sizeof *op);
    if ( ! op ) {
        logme(LOG_ERROR, "Out of memory\n");
        return(-1);
    }

    /* Put it all together now */
    op->dst = strdup(dst);
    op->next = patch->del_file_list;
    patch->del_file_list = op;

    return(0);
}

/* Create a recursive patch between the two trees of files */
int tree_patch(const char *o_top, const char *o_path,
               const char *n_top, const char *n_path, loki_patch *patch)
{
    DIR *old, *new;
    struct dirent *entry;
    char path[PATH_MAX];
    char old_path[PATH_MAX];
    char new_path[PATH_MAX];
    struct stat old_sb, new_sb;
    int status;

    /* No errors yet */
    status = 0;

    /* If both paths are the same, then there's nothing to do */
    sprintf(old_path, "%s/%s", o_top, o_path);
    sprintf(new_path, "%s/%s", n_top, n_path);
    if ( strcmp(old_path, new_path) == 0 ) {
        return(0);
    }
    
    /* First get all the files that are in the old path */
    sprintf(path, "%s/%s", o_top, o_path);
    old = opendir(path);
    if ( ! old ) {
        logme(LOG_ERROR, "Unable to open directory: %s\n", path);
        return(-1);
    }
    while ( (entry=readdir(old)) != NULL ) {
        /* Skip "." and ".." entries */
        if ( (strcmp(entry->d_name, ".") == 0) ||
             (strcmp(entry->d_name, "..") == 0) ) {
            continue;
        }

        /* Make sure we can see the old path */
        sprintf(old_path, "%s/%s/%s", o_top, o_path, entry->d_name);
        if ( lstat(old_path, &old_sb) < 0 ) {
            logme(LOG_ERROR, "Unable to stat path: %s\n", old_path);
            --status;
            continue;
        }

        /* See if the new entry doesn't exist, and we have to remove it */
        sprintf(new_path, "%s/%s/%s", n_top, n_path, entry->d_name);
        if ( lstat(new_path, &new_sb) < 0 ) {
            /* This is an obsolete entry */
            if ( S_ISDIR(old_sb.st_mode) ) {
                if ( tree_del_path(old_path+strlen(o_top)+2, patch) < 0 ) {
                    --status;
                }
            } else {
                if ( tree_del_file(old_path+strlen(o_top)+2, patch) < 0 ) {
                    --status;
                }
            }
            continue;
        }

        /* If they both exist, but are not the same type of file.. */
        if ( S_ISDIR(old_sb.st_mode) != S_ISDIR(new_sb.st_mode) ) {
            logme(LOG_ERROR, "%s is a %s and %s is a %s\n",
                    old_path, S_ISDIR(old_sb.st_mode) ? "directory" : "file",
                    new_path, S_ISDIR(new_sb.st_mode) ? "directory" : "file");
            --status;
            continue;
        }

        if ( S_ISDIR(old_sb.st_mode) ) {
            /* They're both directories, recurse */
            if ( tree_patch(o_top, old_path+strlen(o_top)+1,
                            n_top, new_path+strlen(n_top)+1, patch) < 0 ) {
                --status;
            }
        } else {
            /* They're both files, patch old to new */
            if ( tree_patch_file(old_path, new_path,
                                new_path+strlen(n_top)+2, patch) < 0 ) {
                --status;
            }
        }
    }
    closedir(old);

    /* Now go through and add new files and directories */
    sprintf(path, "%s/%s", n_top, n_path);
    new = opendir(path);
    if ( ! new ) {
        logme(LOG_ERROR, "Unable to open directory: %s\n", path);
        return(-1);
    }
    while ( (entry=readdir(new)) != NULL ) {
        /* Skip "." and ".." entries */
        if ( (strcmp(entry->d_name, ".") == 0) ||
             (strcmp(entry->d_name, "..") == 0) ) {
            continue;
        }

        /* Make sure we can see the new path */
        sprintf(new_path, "%s/%s/%s", n_top, n_path, entry->d_name);
        if ( lstat(new_path, &new_sb) < 0 ) {
            logme(LOG_ERROR, "Unable to stat path: %s\n", new_path);
            --status;
            continue;
        }

        /* See if we can see the old path.  If so, handled above. */
        sprintf(old_path, "%s/%s/%s", o_top, o_path, entry->d_name);
        if ( lstat(old_path, &old_sb) < 0 ) {
            /* This is a new entry of some kind */
            if ( S_ISDIR(new_sb.st_mode) ) {
                if (tree_add_path(new_path,new_path+strlen(n_top)+2,patch) < 0){
                    --status;
                }
            } else {
                if (tree_add_file(new_path,new_path+strlen(n_top)+2,patch) < 0){
                    --status;
                }
            }
        }
    }
    closedir(new);

    /* We're done! */
    return(status);
}

/* Add a set of files and directories from a UNIX tar file
   This is an easy hack - just extract the directory and add it
 */
int tree_tarfile(const char *tarfile, loki_patch *patch)
{
    static const char gzip_magic[2] = { 0037, 0213 };
    FILE *fp;
    char magic[2];
    int is_compressed;
    char tmppath[PATH_MAX];
    char command[4*PATH_MAX];
    int retval;

    /* Make sure we can read the tarfile */
    fp = fopen(tarfile, "rb");
    if ( ! fp ) {
        logme(LOG_ERROR, "Unable to read %s\n", tarfile);
        return(-1);
    }
    if ( fread(magic, sizeof(magic), 1, fp) &&
         (memcmp(magic, gzip_magic, sizeof(magic)) == 0) ) {
        is_compressed = 1;
    } else {
        is_compressed = 0;
    }
    fclose(fp);

    /* Extract the tarfile to the temporary directory */
    sprintf(tmppath, "%s/../tmp", patch->base);
    sprintf(command, "rm -rf %s", tmppath);
    system(command);
    sprintf(command, "mkdir %s", tmppath);
    if ( system(command) != 0 ) {
        return(-1);
    }
    sprintf(command, "tar %sxpf %s -C %s", is_compressed ? "z" : "",
                                           tarfile, tmppath);
    if ( system(command) == 0 ) {
        /* Add the contents of the temporary directory */
        retval = tree_add_path(tmppath, NULL, patch);
    }
    sprintf(command, "rm -rf %s", tmppath);
    system(command);

    return(retval);
}
