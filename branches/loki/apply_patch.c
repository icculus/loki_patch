
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#include "loki_patch.h"
#include "apply_patch.h"
#include "size_patch.h"
#include "loki_xdelta.h"
#include "mkdirhier.h"
#include "md5.h"
#include "log_output.h"

static int apply_add_path(struct op_add_path *op, const char *dst)
{
    char path[PATH_MAX];
    int retval;

    log(LOG_VERBOSE, "-> ADD PATH %s/%s\n", dst, op->dst);

    /* Create a directory, easy */
    sprintf(path, "%s/%s", dst, op->dst);
    retval = mkdir(path, op->mode&0777);
    if ( retval < 0 ) {
        log(LOG_ERROR, "Unable to make path %s\n", path);
    }
    return(retval);
}

static int apply_add_file(const char *base,
                          struct op_add_file *op, const char *dst)
{
    char src_path[PATH_MAX];
    char dst_path[PATH_MAX];
    int src_fd, dst_fd;
    int len;
    char data[4096];
    char csum[CHECKSUM_SIZE+1];

    log(LOG_VERBOSE, "-> ADD FILE %s/%s\n", dst, op->dst);

    /* Open the source and destination files */
    sprintf(src_path, "%s/%s", base, op->dst);
    src_fd = open(src_path, O_RDONLY);
    if ( src_fd < 0 ) {
        log(LOG_ERROR, "Unable to open %s\n", src_path);
        return(-1);
    }
    sprintf(dst_path, "%s/%s.new", dst, op->dst);
    if ( mkdirhier(dst_path) < 0 ) {
        return(-1);
    }
    unlink(dst_path);
    dst_fd = open(dst_path, O_WRONLY|O_CREAT|O_EXCL, op->mode);
    if ( dst_fd < 0 ) {
        log(LOG_ERROR, "Unable to open %s\n", dst_path);
        close(src_fd);
        return(-1);
    }

    /* Copy the data */
    while ( (len=read(src_fd, data, sizeof(data))) > 0 ) {
        if ( write(dst_fd, data, len) != len ) {
            log(LOG_ERROR, "Failed writing to %s\n", dst_path);
            return(-1);
        }
    }
    close(src_fd);
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

static int apply_patch_file(const char *base,
                            struct op_patch_file *op, const char *dst)
{
    char src_path[PATH_MAX];
    char dst_path[PATH_MAX];
    char out_path[PATH_MAX];
    struct stat sb;
    struct delta_option *delta;
    char csum[CHECKSUM_SIZE+1];

    log(LOG_VERBOSE, "-> PATCH FILE %s/%s\n", dst, op->dst);

    /* Make sure the destination file exists */
    sprintf(dst_path, "%s/%s", dst, op->dst);
    if ( stat(dst_path, &sb) < 0 ) {
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
    sprintf(o_path, "%s/%s.new", dst, op->dst);
    sprintf(n_path, "%s/%s", dst, op->dst);
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
    int retval;

    /* Rename the patched file */
    sprintf(o_path, "%s/%s.new", dst, op->dst);
    sprintf(n_path, "%s/%s", dst, op->dst);
    retval = rename(o_path, n_path);
    if ( retval < 0 ) {
        log(LOG_ERROR, "Unable to rename file: %s -> %s\n", o_path, n_path);
    }
    return(retval);
}

static int apply_del_file(struct op_del_file *op, const char *dst)
{
    char path[PATH_MAX];
    int retval;

    log(LOG_VERBOSE, "-> DEL FILE %s/%s\n", dst, op->dst);

    /* Remove a file, easy */
    sprintf(path, "%s/%s", dst, op->dst);
    retval = unlink(path);
    if ( retval < 0 ) {
        log(LOG_ERROR, "Unable to remove %s\n", path);
    }
    return(retval);
}

static int remove_directory(const char *path)
{
    char child_path[PATH_MAX];
    DIR *dir;
    struct dirent *entry;
    struct stat sb;
    int retval, total;

    /* Make sure we can read the directory */
    dir = opendir(path);
    if ( ! dir ) {
        log(LOG_ERROR, "Unable to list %s\n", path);
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
            retval += remove_directory(child_path);
        } else {
            retval = unlink(child_path);
            if ( retval < 0 ) {
                log(LOG_ERROR, "Unable to remove %s\n", child_path);
                total += retval;
            }
        }
    }
    closedir(dir);

    /* Finally remove the directory and return */
    retval = rmdir(path);
    if ( retval < 0 ) {
        log(LOG_ERROR, "Unable to remove %s\n", path);
        total += retval;
    }
    return(total);
}

static int apply_del_path(struct op_del_path *op, const char *dst)
{
    /* Recursively remove a directory */
    char path[PATH_MAX];

    log(LOG_VERBOSE, "-> DEL PATH %s/%s\n", dst, op->dst);

    /* Remove a file, easy */
    sprintf(path, "%s/%s", dst, op->dst);
    return remove_directory(path);
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

    /* Second stage, apply deltas, create new paths, copy new files */
    { struct op_patch_file *op;

        for ( op = patch->patch_file_list; op; op=op->next ) {
            if ( apply_patch_file(patch->base, op, dst) < 0 ) {
                return(-1);
            }
            disk_done += (op->size + 1023)/1024;
            if ( disk_done ) {
                log(LOG_NORMAL," %0.0f%%\r",((float)disk_done/disk_used)*100.0);
            }
        }
    }
    { struct op_add_path *op;

        for ( op = patch->add_path_list; op; op=op->next ) {
            if ( apply_add_path(op, dst) < 0 ) {
                return(-1);
            }
        }
    }
    { struct op_add_file *op;

        for ( op = patch->add_file_list; op; op=op->next ) {
            if ( apply_add_file(patch->base, op, dst) < 0 ) {
                return(-1);
            }
            disk_done += (op->size + 1023)/1024;
            if ( disk_done ) {
                log(LOG_NORMAL," %0.0f%%\r",((float)disk_done/disk_used)*100.0);
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
            apply_del_file(op, dst);
        }
    }
    { struct op_del_path *op;

        for ( op = patch->del_path_list; op; op=op->next ) {
            /* This is non-fatal */
            apply_del_path(op, dst);
        }
    }

    /* Yay!  The patch succeeded! */
    log(LOG_NORMAL, " %2.2f%%\r", 100.0);

    return(0);
}