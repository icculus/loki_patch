
/* This is the patch version string */
#define LOKI_VERSION    "LOKI_PATCH 1.0"

/* The number of bytes required to hold an MD5 checksum string */
#include "md5.h"

/* These are the operations that can be performed */

typedef enum {
	OP_NONE,
	OP_ADD_FILE,
	OP_ADD_PATH,
	OP_DEL_FILE,
	OP_DEL_PATH,
	OP_PATCH_FILE
} patch_op;

struct op_add_file {
    char *dst;
    char *src;
    char  sum[CHECKSUM_SIZE+1];
    long  mode;
    long  size;
    struct op_add_file *next;
};

struct op_add_path {
    char *dst;
    long  mode;
    struct op_add_path *next;
};

struct op_del_file {
    char *dst;
    struct op_del_file *next;
};

struct op_del_path {
    char *dst;
    struct op_del_path *next;
};

struct op_patch_file {
    char *dst;
    struct delta_option {
        int installed;
        char oldsum[CHECKSUM_SIZE+1];
        char *src;
        char newsum[CHECKSUM_SIZE+1];
        struct delta_option *next;
    } *options;
    long mode;
    long size;
    struct op_patch_file *next;
};

/* The actual patch structure */
typedef struct loki_patch {
    char *base;             /* The patch data base directory */
    char *product;          /* The product as named in install registry */
    char *component;        /* The component modified by this patch */
    char *version;          /* The component version of the patch */
    char *description;      /* The text description of the patch */
    char *prepatch;         /* Command to run before the patch is applied */
    char *postpatch;        /* Command to run after the patch is applied */

    struct op_add_path *add_path_list;
    struct op_add_file *add_file_list;
    struct op_patch_file *patch_file_list;
    struct op_del_file *del_file_list;
    struct op_del_path *del_path_list;

    /* This is needed for unregistering paths that are removed */
    struct removed_path {
        char *path;
        struct removed_path *next;
    } *removed_paths;
} loki_patch;

