
#include <sys/types.h>

/* Calculate the size of the patch data files */
extern size_t patch_size(loki_patch *patch);

/* Calculate the maximum disk space required for patch */
extern size_t calculate_space(loki_patch *patch);

/* Calculate the amount of free space on the given path */
extern size_t available_space(const char *path);
