
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#include "loki_patch.h"
#include "load_patch.h"
#include "print_patch.h"
#include "apply_patch.h"
#include "registry.h"
#include "log_output.h"


static void print_usage(const char *argv0)
{
    fprintf(stderr, "Loki Patch Tools " VERSION "\n");
    fprintf(stderr, "Usage: %s [--info] patch-file [install-path]\n", argv0);
}

int main(int argc, char *argv[])
{
    char path[PATH_MAX];
    char *product_root;
    loki_patch *patch;
    int i;
    int show_info;
    int just_verify;
    const char *patchfile;
    const char *install;

    /* Quick hack to check command-line arguments */
    show_info = 0;
    just_verify = 0;
    for ( i=1; argv[i] && (argv[i][0] == '-'); ++i ) {
        if ( (strcmp(argv[i], "--verbose") == 0) ||
             (strcmp(argv[i], "-v") == 0) ) {
            set_logging(LOG_VERBOSE);
        } else
        if ( strcmp(argv[i], "--verify") == 0 ) {
            just_verify = 1;
        } else
        if ( strcmp(argv[i], "--info") == 0 ) {
            show_info = 1;
        } else {
            print_usage(argv[0]);
            return(1);
        }
    }
    /* Allow environment variable override */
    if ( getenv("PATCH_LOGGING") ) {
        set_logging(atoi(getenv("PATCH_LOGGING")));
    }

    /* Make sure we have the correct command line arguments */
    patchfile = argv[i];
    if ( ! patchfile ) {
        print_usage(argv[0]);
        return(1);
    }
    install = argv[i+1];

    /* Load the patch */
    patch = load_patch(patchfile);
    if ( ! patch ) {
        return(2);
    }

    /* Figure out where the product is installed (if at all) */
    if ( ! install || !*install ) {
        install = get_product_root(patch->product, path, (sizeof path));
    }

    /* Print out information about the patch and install */
    if ( show_info ) {
        print_info(patch, stdout);
        if ( install ) {
            printf("Installed: %s\n", install);
        }
        free_patch(patch);
        return(0);
    }

    /* See if we're just verifying the patch */
    if ( just_verify ) {
        /* Patch is okay by this point */
        free_patch(patch);
        return(0);
    }

    /* Apply the patch */
    if ( ! install ) {
        printf("Unable to find install path for %s\n", patch->product);
        print_usage(argv[0]);
        free_patch(patch);
        return(3);
    }
    if ( apply_patch(patch, install) ) {
        free_patch(patch);
        return(3);
    }

    /* Update the registry, if we're updating the proper path */
    product_root = get_product_root(patch->product, path, sizeof(path));
    if ( product_root && (strcmp(install, product_root) == 0) ) {
        update_registry(patch);
    }

    /* We're done! */
    free_patch(patch);
    return(0);
}
