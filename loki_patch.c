
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "loki_patch.h"
#include "load_patch.h"
#include "apply_patch.h"
#include "log_output.h"

#include "setupdb.h"

static void print_usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s patch-file [install-path]\n", argv0);
}

static char *get_product_root(const char *product, char *path, int maxpath)
{
    product_t *the_product;
    product_info_t *product_info;

    /* Check the registry for this product */
    the_product = loki_openproduct(product);
    if ( the_product ) {
        product_info = loki_getinfo_product(the_product);
        strncpy(path, product_info->root, maxpath);
        loki_closeproduct(the_product);
    } else {
        path = NULL;
    }
    return path;
}

static void update_registry(loki_patch *patch)
{
    product_t *product;
    product_option_t *default_option;
    product_file_t *file_info;
    product_option_t *install_option;
    product_component_t *component;

    /* Open the product for updating */
    product = loki_openproduct(patch->product);
    if ( ! product ) {
        return;
    }

    /* The patch component defaults to the default product component */
    if ( patch->component ) {
        component = loki_find_component(product, patch->component);
    } else {
        component = loki_getdefault_component(product);
    }
    /* Get the first install option for the component */
    if ( ! component ) { /* Then this is an add-on (new component) */
        component = loki_create_component(product, patch->component, patch->version);

        /* Create a default install option for this add-on */
        loki_create_option(component, "Base Install");
    }
    default_option = loki_getfirst_option(component);

    /* Now update all the added, symlinked, removed and patched files */
    { struct op_add_path *op;
        for ( op = patch->add_path_list; op; op=op->next ) {
            file_info = loki_findpath(op->dst, product);
            if ( file_info ) {
                install_option = loki_getoption_file(file_info);
                loki_unregister_path(install_option, op->dst);
            } else {
                install_option = default_option;
            }
            if ( op->added ) {
                loki_register_file(install_option, op->dst, NULL);
            }
        }
    }
    { struct op_add_file *op;
        for ( op = patch->add_file_list; op; op=op->next ) {
            file_info = loki_findpath(op->dst, product);
            if ( file_info ) {
                install_option = loki_getoption_file(file_info);
                loki_unregister_path(install_option, op->dst);
            } else {
                install_option = default_option;
            }
            if ( op->added ) {
                loki_register_file(install_option, op->dst, op->sum);
            }
        }
    }
    { struct op_symlink_file *op;
        for ( op = patch->symlink_file_list; op; op=op->next ) {
            file_info = loki_findpath(op->dst, product);
            if ( file_info ) {
                install_option = loki_getoption_file(file_info);
                loki_unregister_path(install_option, op->dst);
            } else {
                install_option = default_option;
            }
            if ( op->added ) {
                loki_register_file(install_option, op->dst, NULL);
            }
        }
    }
    { struct op_patch_file *op;
        for ( op = patch->patch_file_list; op; op=op->next ) {
            struct delta_option *option;
            for ( option=op->options; option; option=option->next ) {
                if ( option->installed ) {
                    file_info = loki_findpath(op->dst, product);
                    if ( file_info ) {
                        install_option = loki_getoption_file(file_info);
                    } else {
                        install_option = default_option;
                    }
                    loki_register_file(install_option, op->dst, option->newsum);
                    break;
                }
            }
        }
    }
    { struct removed_path *op;
        for ( op = patch->removed_paths; op; op=op->next ) {
            file_info = loki_findpath(op->path, product);
            if ( file_info ) {
                install_option = loki_getoption_file(file_info);
                loki_unregister_path(install_option, op->path);
            }
        }
    }

    /* Update the component version for this patch.
       Don't override the version extension, if there already is one
     */
    { char new_version[256];
      char old_versionbase[128], old_versionext[128];
      char new_versionbase[128], new_versionext[128];

      loki_split_version(loki_getversion_component(component),
                         old_versionbase, sizeof(old_versionbase),
                         old_versionext, sizeof(old_versionext));
      loki_split_version(patch->version,
                         new_versionbase, sizeof(new_versionbase),
                         new_versionext, sizeof(new_versionext));
      if ( *old_versionext ) {
          strcpy(new_versionext, old_versionext);
      }
      sprintf(new_version, "%s%s", new_versionbase, new_versionext);
      loki_setversion_component(component, new_version);
    }

    /* We're done! */
    loki_closeproduct(product);
}

int main(int argc, char *argv[])
{
    char path[PATH_MAX];
    char *product_root;
	loki_patch *patch;
    const char *install;

    /* Quick hack to check command-line arguments */
    if ( argv[1] && (strcmp(argv[1], "-v") == 0) ) {
        --argc;
        ++argv;
        set_logging(LOG_VERBOSE);
    }

    /* Make sure we have the correct command line arguments */
	if ( argc < 2 ) {
        print_usage(argv[0]);
		exit(1);
	}

    /* Load the patch */
	patch = load_patch(argv[1]);
	if ( ! patch ) {
		exit(2);
	}

    /* See if we're just verifying the patch */
    if ( argv[2] && (strcmp(argv[2], "--verify") == 0) ) {
        /* Patch is okay by this point */
        exit(0);
    }

    /* Apply the patch */
    printf("%s\n", patch->description);
    if ( argv[2] ) {
        install = argv[2];
    } else {
        install = get_product_root(patch->product, path, (sizeof path));
    }
    if ( ! install ) {
        printf("Unable to find install path for %s\n", patch->product);
        print_usage(argv[0]);
        exit(3);
    }
	if ( apply_patch(patch, install) ) {
		exit(3);
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
