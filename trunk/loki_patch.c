
#include <stdio.h>
#include <unistd.h>

#include "loki_patch.h"
#include "load_patch.h"
#include "apply_patch.h"
#include "log_output.h"

#include "setupdb.h"

static void print_usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s patch-file [install-path]\n", argv0);
}

static void update_registry(product_t *product, loki_patch *patch)
{
    product_option_t *default_option;
    product_file_t *file_info;
    product_option_t *install_option;
    product_component_t *component;

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

    /* Now update all the added, removed and patched files */
    { struct op_add_path *op;
        for ( op = patch->add_path_list; op; op=op->next ) {
            file_info = loki_findpath(op->dst, product);
            if ( file_info ) {
                install_option = loki_getoption_file(file_info);
            } else {
                install_option = default_option;
            }
            loki_register_file(install_option, op->dst, 0);
        }
    }
    { struct op_add_file *op;
        for ( op = patch->add_file_list; op; op=op->next ) {
            file_info = loki_findpath(op->dst, product);
            if ( file_info ) {
                install_option = loki_getoption_file(file_info);
            } else {
                install_option = default_option;
            }
            loki_register_file(install_option, op->dst, op->sum);
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
fprintf(stderr, "Unregistering %s\n", op->path);
                loki_unregister_path(install_option, op->path);
            }
        }
    }
    return;
}

int main(int argc, char *argv[])
{
    product_t *product;
    product_info_t *product_info;
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

    /* Check the registry for this product */
    product = loki_openproduct(patch->product);
    if ( product ) {
        product_info = loki_getinfo_product(product);
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
        if ( ! product ) {
            /* FIXME: Prompt for the install path? */
            printf("Unable to find install path for %s\n", patch->product);
            print_usage(argv[0]);
            exit(3);
        }
        install = product_info->root;
    }
    if ( product ) {
        /* Double check the install path */
        if ( strcmp(install, product_info->root) != 0 ) {
            log(LOG_ERROR, "Warning: product installed in %s, patching %s\n",
                product_info->root, install);
            loki_closeproduct(product);
            product = (product_t *)0;
        }
    }
	if ( apply_patch(patch, install) ) {
		exit(3);
	}

    /* Update the registry */
    if ( product ) {
        update_registry(product, patch);

        /* Write it all out */
        loki_closeproduct(product);
        product = (product_t *)0;
    }

    /* We're done! */
	free_patch(patch);
	return(0);
}
