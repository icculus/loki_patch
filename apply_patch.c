
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#include <zlib.h>

#include "loki_patch.h"
#include "apply_patch.h"
#include "size_patch.h"
#include "loki_xdelta.h"
#include "mkdirhier.h"
#include "md5.h"
#include "arch.h"
#include "log_output.h"

static void assemble_path(char *dest, const char *base, const char *path)
{
    if ( *path == '/' ) {
        strcpy(dest, path);
    } else {
        sprintf(dest, "%s/%s", base, path);
    }
}

static int apply_add_path(struct op_add_path *op, const char *dst)
{
    char path[PATH_MAX];
    struct stat sb;
    int retval;

    log(LOG_VERBOSE, "-> ADD PATH %s\n", op->dst);

    /* Create a directory, if it doesn't already exist */
    assemble_path(path, dst, op->dst);
    retval = 0;
    if ( stat(path, &sb) == 0 ) {
        if ( S_ISDIR(sb.st_mode) ) {
            if ( (sb.st_mode&0777) != (op->mode&0777) ) {
                retval = chmod(path, op->mode&0777);
            }
        } else {
            log(LOG_ERROR, "Path exists, and isn't directory: %s\n", path);
            retval = -1;
        }
    } else {
        retval = mkdir(path, op->mode&0777);
        if ( retval < 0 ) {
            log(LOG_ERROR, "Unable to make path %s\n", path);
        } else {
            op->added = 1;
        }
    }
    return(retval);
}

static int apply_add_file(const char *base,
                          struct op_add_file *op, const char *dst,
                          size_t disk_done, size_t disk_used)
{
    char src_path[PATH_MAX];
    char dst_path[PATH_MAX];
    gzFile src_zfp;
    int dst_fd;
    int len;
    char data[4096];
    char csum[CHECKSUM_SIZE+1];

    log(LOG_VERBOSE, "-> ADD FILE %s\n", op->dst);

    /* Open the source and destination files */
    sprintf(src_path, "%s/%s", base, op->dst);
    src_zfp = gzopen(src_path, "rb");
    if ( src_zfp == NULL ) {
        log(LOG_ERROR, "Unable to open %s\n", src_path);
        return(-1);
    }
    if ( *op->dst == '/' ) {
        sprintf(dst_path, "%s.new", op->dst);
    } else {
        sprintf(dst_path, "%s/%s.new", dst, op->dst);
    }
    if ( mkdirhier(dst_path) < 0 ) {
        gzclose(src_zfp);
        return(-1);
    }
    unlink(dst_path);
    dst_fd = open(dst_path, O_WRONLY|O_CREAT|O_EXCL, op->mode);
    if ( dst_fd < 0 ) {
        log(LOG_ERROR, "Unable to open %s\n", dst_path);
        gzclose(src_zfp);
        return(-1);
    }
    op->added = 1;

    /* Copy the data */
    disk_done *= 1024;
    while ( (len=gzread(src_zfp, data, sizeof(data))) > 0 ) {
        if ( write(dst_fd, data, len) != len ) {
            log(LOG_ERROR, "Failed writing to %s\n", dst_path);
            return(-1);
        }
        disk_done += len;
        log(LOG_NORMAL, " %0.0f%%%c",
            ((((float)disk_done)/1024.0)/disk_used)*100.0,
            get_logging() <= LOG_VERBOSE ? '\n' : '\r');
    }
    gzclose(src_zfp);
    if ( close(dst_fd) < 0 ) {
        log(LOG_ERROR, "Failed writing to %s\n", dst_path);
        return(-1);
    }

    /* Verify the checksum */
    md5_compute(dst_path, csum, 1);
    if ( strcmp(op->sum, csum) != 0 ) {
        log(LOG_ERROR, "Failed checksum: %s\n", dst_path);
        return(-1);
    }
    return(0);
}

static int apply_symlink_file(const char *base,
                          struct op_symlink_file *op, const char *dst)
{
    char path[PATH_MAX];
    int retval;

    log(LOG_VERBOSE, "-> SYMLINK FILE %s -> %s\n", op->dst, op->link);

    /* Symlink a file, easy */
    assemble_path(path, dst, op->dst);
    if ( mkdirhier(path) < 0 ) {
        return(-1);
    }
    unlink(path);
    retval = symlink(op->link, path);
    if ( retval < 0 ) {
        log(LOG_ERROR, "Unable to create symlink %s\n", path);
    } else {
        op->added = 1;
    }
    return(retval);
}

static int apply_patch_file(const char *base,
                            struct op_patch_file *op, const char *dst)
{
    char src_path[PATH_MAX];
    char dst_path[PATH_MAX];
    char out_path[PATH_MAX];
    struct stat sb;
    struct delta_option *delta;
    char csum[CHECKSUM_SIZE+1];

    log(LOG_VERBOSE, "-> PATCH FILE %s\n", op->dst);

    /* Make sure the destination file exists */
    if ( *op->dst == '/' ) {
        strcpy(dst_path, op->dst);
    } else {
        sprintf(dst_path, "%s/%s", dst, op->dst);
    }
    if ( stat(dst_path, &sb) < 0 ) {
        if ( op->optional )  {
            return(0);
        }
        log(LOG_ERROR, "Can't find %s\n", dst_path);
        return(-1);
    }
    md5_compute(dst_path, csum, 1);

    /* See if we can find a corresponding delta */
    for ( delta=op->options; delta; delta=delta->next ) {
        if ( strcmp(delta->oldsum, csum) == 0 ) {
            /* Whew!  Found it! */
            break;
        }
    }
    if ( ! delta ) {
        log(LOG_ERROR, "No matching delta for %s\n", dst_path);
        return(-1);
    }

    /* Apply the given delta */
    sprintf(src_path, "%s/%s", base, delta->src);
    if ( stat(src_path, &sb) < 0 ) {
        log(LOG_ERROR, "Can't find %s\n", src_path);
        return(-1);
    }
    sprintf(out_path, "%s.new", dst_path);
    if ( loki_xpatch(src_path, dst_path, out_path) < 0 ) {
        log(LOG_ERROR, "Failed patch delta on %s\n", dst_path);
        return(-1);
    }
    chmod(out_path, op->mode);

    /* Verify the checksum */
    md5_compute(out_path, csum, 1);
    if ( strcmp(delta->newsum, csum) != 0 ) {
        log(LOG_ERROR, "Failed checksum: %s\n", dst_path);
        return(-1);
    }
    delta->installed = 1;

    return(0);
}

static int rename_add_file(struct op_add_file *op, const char *dst)
{
    char o_path[PATH_MAX];
    char n_path[PATH_MAX];
    int retval;

    /* Rename the added file into place */
    if ( *op->dst == '/' ) {
        sprintf(o_path, "%s.new", op->dst);
        sprintf(n_path, "%s", op->dst);
    } else {
        sprintf(o_path, "%s/%s.new", dst, op->dst);
        sprintf(n_path, "%s/%s", dst, op->dst);
    }
    retval = rename(o_path, n_path);
    if ( retval < 0 ) {
        log(LOG_ERROR, "Unable to rename file: %s -> %s\n", o_path, n_path);
    }
    return(retval);
}

static int rename_patch_file(struct op_patch_file *op, const char *dst)
{
    char o_path[PATH_MAX];
    char n_path[PATH_MAX];
    struct stat sb;
    int retval;

    /* Rename the patched file */
    if ( *op->dst == '/' ) {
        sprintf(o_path, "%s.new", op->dst);
        sprintf(n_path, "%s", op->dst);
    } else {
        sprintf(o_path, "%s/%s.new", dst, op->dst);
        sprintf(n_path, "%s/%s", dst, op->dst);
    }
    if ( stat(n_path, &sb) < 0 ) {
        if ( op->optional )  {
            return(0);
        }
    }
    retval = rename(o_path, n_path);
    if ( retval < 0 ) {
        log(LOG_ERROR, "Unable to rename file: %s -> %s\n", o_path, n_path);
    }
    return(retval);
}

static void add_removed_path(const char *path,
                             const char *prefix, struct removed_path **paths)
{
    struct removed_path *newpath;

    newpath = (struct removed_path *)malloc(sizeof *newpath);
    if ( newpath ) {
        if ( strncmp(path, prefix, strlen(prefix)) == 0 ) {
            path += strlen(prefix);
            while ( *path == '/' ) {
                ++path;
            }
        }
        newpath->path = strdup(path);
        if ( newpath->path ) {
            newpath->next = *paths;
            *paths = newpath;
        } else {
            free(newpath);
        }
    }
}

static int apply_del_file(struct op_del_file *op, const char *dst,
                          struct removed_path **paths)
{
    char path[PATH_MAX];
    int retval;

    log(LOG_VERBOSE, "-> DEL FILE %s\n", op->dst);

    /* Remove a file, easy */
    assemble_path(path, dst, op->dst);
    retval = unlink(path);
    if ( retval < 0 ) {
#if 0 /* No worries */
        log(LOG_WARNING, "Unable to remove %s\n", path);
#endif
    } else {
        add_removed_path(path, dst, paths);
    }
    return(retval);
}

static int remove_directory(const char *path,
                            const char *prefix, struct removed_path **paths)
{
    char child_path[PATH_MAX];
    DIR *dir;
    struct dirent *entry;
    struct stat sb;
    int retval, total;

    /* Make sure we can read the directory */
    dir = opendir(path);
    if ( ! dir ) {
#if 0 /* No worries */
        log(LOG_ERROR, "Unable to list %s\n", path);
#endif
        return(-1);
    }

    /* Remove everything in the directory */
    total = 0;
    while ( (entry=readdir(dir)) != NULL ) {
        /* Skip "." and ".." entries */
        if ( (strcmp(entry->d_name, ".") == 0) ||
             (strcmp(entry->d_name, "..") == 0) ) {
            continue;
        }

        /* Remove the child path */
        sprintf(child_path, "%s/%s", path, entry->d_name);
        if ( stat(child_path, &sb) < 0 ) {
            log(LOG_ERROR, "Unable to stat %s\n", child_path);
            total -= 1;
            continue;
        }
        if ( S_ISDIR(sb.st_mode) ) {
            retval += remove_directory(child_path, prefix, paths);
        } else {
            retval = unlink(child_path);
            if ( retval < 0 ) {
                log(LOG_ERROR, "Unable to remove %s\n", child_path);
                total += retval;
            } else {
                add_removed_path(child_path, prefix, paths);
            }
        }
    }
    closedir(dir);

    /* Finally remove the directory and return */
    retval = rmdir(path);
    if ( retval < 0 ) {
        log(LOG_ERROR, "Unable to remove %s\n", path);
        total += retval;
    } else {
        add_removed_path(path, prefix, paths);
    }
    return(total);
}

static int apply_del_path(struct op_del_path *op, const char *dst,
                          struct removed_path **paths)
{
    /* Recursively remove a directory */
    char path[PATH_MAX];

    log(LOG_VERBOSE, "-> DEL PATH %s\n", op->dst);

    /* Remove a directory, easy */
    assemble_path(path, dst, op->dst);
    return remove_directory(path, dst, paths);
}

static int chmod_directory(const char *path)
{
    char child_path[PATH_MAX];
    DIR *dir;
    struct dirent *entry;
    struct stat sb;
    mode_t new_mode;

    /* Make sure that we can read the existing directory mode */
    if ( stat(path, &sb) < 0 ) {
        log(LOG_DEBUG, "Unable to stat %s\n", path);
        return(-1);
    }

    /* We're only affecting directories - files are handled by the apply */
    if ( ! S_ISDIR(sb.st_mode) ) {
        return(0);
    }

    /* Make sure we can read, write and search this directory */
    new_mode = (sb.st_mode | (S_IRUSR|S_IWUSR|S_IXUSR));
    if ( sb.st_mode != new_mode ) {
        chmod(path, new_mode);
    }

    /* Go recursive... */
    dir = opendir(path);
    if ( ! dir ) {
        log(LOG_DEBUG, "Unable to list %s\n", path);
        return(-1);
    }

    /* Change everything in the directory */
    while ( (entry=readdir(dir)) != NULL ) {
        /* Skip "." and ".." entries */
        if ( (strcmp(entry->d_name, ".") == 0) ||
             (strcmp(entry->d_name, "..") == 0) ) {
            continue;
        }

        /* Change the child path */
        sprintf(child_path, "%s/%s", path, entry->d_name);
        chmod_directory(child_path);
    }
    closedir(dir);

    return(0);
}

int apply_patch(loki_patch *patch, const char *dst)
{
    size_t disk_done;
    size_t disk_used;
    size_t disk_free;

    /* First stage, check ownership and disk space requirements */
    chmod_directory(dst);
    if ( access(dst, W_OK) < 0 ) {
        log(LOG_ERROR, "Unable to write to %s\n", dst);
        return(-1);
    }

    disk_done = 0;
    disk_used = calculate_space(patch);
    disk_free = available_space(dst);
    if ( disk_used > disk_free ) {
        log(LOG_ERROR,
        "Not enough diskspace available, %uMB needed, %uMB free\n",
                (disk_used+1023)/1024, disk_free/1024);
        return(-1);
    }

    /* Second stage, set environment and run pre-patch script */
    { char env[2*PATH_MAX], *bufp, *key;
      struct optional_field *field;
        /* Set the environment for the patch scripts */
        /* Ack, I use strdup() because on LinuxPPC, putenv() just adds
           the pointer to the environ array, rather than duplicating it.
         */
        sprintf(env, "PATCH_PRODUCT=%s", patch->product);
        putenv(strdup(env));
        sprintf(env, "PATCH_COMPONENT=%s", patch->component);
        putenv(strdup(env));
        sprintf(env, "PATCH_VERSION=%s", patch->version);
        putenv(strdup(env));
        for ( field=patch->optional_fields; field; field=field->next ) {
            strcpy(env, "PATCH_");
            bufp = env+strlen(env);
            for ( key=field->key; *key; ++key ) {
                *bufp++ = toupper(*key);
            }
            *bufp = '\0';
            strcat(env, "=");
            strncat(env, field->val, sizeof(env)-strlen(env));
            putenv(strdup(env));
        }
        sprintf(env, "PATCH_PATH=%s", dst);
        putenv(strdup(env));
        sprintf(env, "PATCH_OS=%s", detect_os());
        putenv(strdup(env));
        sprintf(env, "PATCH_ARCH=%s", detect_arch());
        putenv(strdup(env));
    }
    if ( patch->prepatch ) {
        if ( system(patch->prepatch) != 0 ) {
            log(LOG_ERROR, "Prepatch script returned non-zero status - Aborting\n");
            return(-1);
        }
    }

    /* Fire it up! */
    log(LOG_NORMAL, " 0%%%c", get_logging() <= LOG_VERBOSE ? '\n' : '\r');

    /* Third stage, apply deltas, create new paths, copy new files */
    { struct op_patch_file *op;

        for ( op = patch->patch_file_list; op; op=op->next ) {
            if ( apply_patch_file(patch->base, op, dst) < 0 ) {
                return(-1);
            }
            disk_done += (op->size + 1023)/1024;
            if ( disk_done ) {
                log(LOG_NORMAL," %0.0f%%%c",
                    ((float)disk_done/disk_used)*100.0,
                    get_logging() <= LOG_VERBOSE ? '\n' : '\r');
            }
        }
    }
    { struct op_add_path *op;

        for ( op = patch->add_path_list; op; op=op->next ) {
            op->added = 0;
            if ( apply_add_path(op, dst) < 0 ) {
                return(-1);
            }
        }
    }
    { struct op_add_file *op;

        for ( op = patch->add_file_list; op; op=op->next ) {
            op->added = 0;
            if (apply_add_file(patch->base, op, dst, disk_done, disk_used) < 0){
                return(-1);
            }
            disk_done += (op->size + 1023)/1024;
            if ( disk_done ) {
                log(LOG_NORMAL," %0.0f%%%c",
                    ((float)disk_done/disk_used)*100.0,
                    get_logging() <= LOG_VERBOSE ? '\n' : '\r');
            }
        }
    }
    { struct op_symlink_file *op;

        for ( op = patch->symlink_file_list; op; op=op->next ) {
            op->added = 0;
            if ( apply_symlink_file(patch->base, op, dst) < 0 ) {
                return(-1);
            }
        }
    }

    /* Third stage, rename patched/added files, remove obsolete files */
    { struct op_patch_file *op;

        for ( op = patch->patch_file_list; op; op=op->next ) {
            if ( rename_patch_file(op, dst) < 0 ) {
                return(-1);
            }
        }
    }
    { struct op_add_file *op;

        for ( op = patch->add_file_list; op; op=op->next ) {
            if ( rename_add_file(op, dst) < 0 ) {
                return(-1);
            }
        }
    }
    { struct op_del_file *op;

        for ( op = patch->del_file_list; op; op=op->next ) {
            /* This is non-fatal */
            apply_del_file(op, dst, &patch->removed_paths);
        }
    }
    { struct op_del_path *op;

        for ( op = patch->del_path_list; op; op=op->next ) {
            /* This is non-fatal */
            apply_del_path(op, dst, &patch->removed_paths);
        }
    }

    /* Final stage, run post-patch script */
    if ( patch->postpatch ) {
        if ( system(patch->postpatch) != 0 ) {
            log(LOG_WARNING, "Postpatch script returned non-zero status\n");
        }
    }

    /* Yay!  The patch succeeded! */
    log(LOG_NORMAL, " 100%%%c", get_logging() <= LOG_VERBOSE ? '\n' : '\r');

    return(0);
}
