
#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif

#include "loki_patch.h"
#include "size_patch.h"


/* Calculate the maximum disk space required for patch */
size_t calculate_space(loki_patch *patch)
{
    unsigned long used = 0;

    /* First check the space used by new files */
    { struct op_add_file *op;

        for ( op = patch->add_file_list; op; op=op->next ) {
            used += (op->size + 1023)/1024;
        }
    }

    /* Now check the space used by patched files */
    { struct op_patch_file *op;

        for ( op = patch->patch_file_list; op; op=op->next ) {
            used += (op->size + 1023)/1024;
        }
    }

    return(used);
}

/* Calculate the amount of free space on the given path */
size_t available_space(const char *path)
{
    struct statfs sb;
    unsigned long long avail;

    if ( statfs(path, &sb) < 0 ) {
        return 0;
    }
    avail = sb.f_bsize;
    avail *= sb.f_bavail;
    return  (size_t) (avail / 1024LL);
}

#ifdef TEST_MAIN

int main(int argc, char *argv[])
{
    const char *path;

    path = argv[1];
    if ( path ) {
        printf("%ld K available on %s\n", available_space(path), path);
    }
}

#endif /* TEST_MAIN */
