
/* The number of bytes required to hold an MD5 checksum string */
#include "md5.h"

/* This is the patch version string */
#define LOKI_VERSION    "LOKI_PATCH 1.0"

#define SAMPLE_HEADER \
"# This is the name of the product as listed in the installation registry\n" \
"Product: product\n" \
"# This is an optional component name, used for adding or patching add-ons\n" \
"Component:\n" \
"# This is the version of the product/component after patching\n" \
"Version: 1.1\n" \
"# This is a description of the patch, printed out at patch time\n" \
"Description: Product 1.0 Update\n" \
"# This is an optional comma separated list of valid architectures\n" \
"Architecture: any\n" \
"# This is an optional comma separated list of valid C library versions\n" \
"Libc: any\n" \
"# This is the version of product or component to which the update applies\n" \
"Applies: 1.0\n" \
"# This is the name of the update archive on the mirror sites\n" \
"File: product-1.1.run\n" \
"# This is a command line run before the patch process\n" \
"Prepatch: sh pre-patch.sh $PATCH_PRODUCT $PATCH_PATH\n" \
"# This is a command line run after the patch process\n" \
"Postpatch: sh post-patch.sh $PATCH_PRODUCT $PATCH_PATH\n" \
"\n" \
"%%" LOKI_VERSION " - Do not remove this line!\n"

/* These are the operations that can be performed */

typedef enum {
	OP_NONE,
	OP_ADD_FILE,
	OP_ADD_PATH,
	OP_DEL_FILE,
	OP_DEL_PATH,
	OP_PATCH_FILE,
	OP_SYMLINK_FILE
} patch_op;

struct op_add_file {
    char *dst;
    char *src;
    char  sum[CHECKSUM_SIZE+1];
    long  mode;
    long  size;
    int   performed;
    struct op_add_file *next;
};

struct op_add_path {
    char *dst;
    long  mode;
    int   performed;
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
    int optional;
    int performed;
    struct op_patch_file *next;
};

struct op_symlink_file {
    char *dst;
    char *link;
    int   performed;
    struct op_symlink_file *next;
};

/* The actual patch structure */
typedef struct loki_patch {
    char *base;             /* The patch data base directory */
    char *product;          /* The product as named in install registry */
    char *component;        /* The component modified by this patch */
    char *version;          /* The component version of the patch */
    struct optional_field { /* Untouched fields in the header */
        char *key;
        char *val;
        struct optional_field *next;
    } *optional_fields;
    char *prepatch;         /* Command to run before the patch is applied */
    char *postpatch;        /* Command to run after the patch is applied */

    struct op_add_path *add_path_list;
    struct op_add_file *add_file_list;
    struct op_patch_file *patch_file_list;
    struct op_symlink_file *symlink_file_list;
    struct op_del_file *del_file_list;
    struct op_del_path *del_path_list;

    /* This is needed for unregistering paths that are removed */
    struct removed_path {
        char *path;
        struct removed_path *next;
    } *removed_paths;
} loki_patch;

