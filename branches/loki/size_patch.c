
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

    if ( statfs(path, &sb) < 0 ) {
        return 0;
    }
    return (sb.f_bsize * sb.f_bavail) / 1024;
}