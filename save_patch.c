
#include <stdio.h>

#include "loki_patch.h"
#include "save_patch.h"
#include "size_patch.h"

int save_patch(loki_patch *patch, const char *patchfile)
{
    FILE *file;

    /* Open the patch file */
    file = fopen(patchfile, "w");
    if ( ! file ) {
        fprintf(stderr, "Unable to write %s\n", patchfile);
        return(-1);
    }

    /* Print out the patch header */
    fprintf(file, "Product: %s\n", patch->product);    
    if ( patch->component ) {
        fprintf(file, "Component: %s\n", patch->component);    
    }
    fprintf(file, "Version: %s\n", patch->version);    
    fprintf(file, "Description: %s\n", patch->description);    
    if ( patch->prepatch ) {
        fprintf(file, "Prepatch: %s\n", patch->prepatch);    
    }
    if ( patch->postpatch ) {
        fprintf(file, "Postpatch: %s\n", patch->postpatch);    
    }
    fprintf(file, "# Diskspace required: %u K\n", calculate_space(patch));
    fprintf(file, "\n");
    fprintf(file, "%%" LOKI_VERSION " - Do not remove this line!\n");
    fprintf(file, "\n");

    /* Print out the list of new paths */
    { struct op_add_path *op;

        for ( op=patch->add_path_list; op; op=op->next ) {
            fprintf(file, "ADD PATH %s\n", op->dst);
            fprintf(file, "mode=0%lo\n", op->mode);
            fprintf(file, "\n");
        }
    }

    /* Print out the list of new files */
    { struct op_add_file *op;

        for ( op=patch->add_file_list; op; op=op->next ) {
            fprintf(file, "ADD FILE %s\n", op->dst);
            fprintf(file, "src=%s\n", op->dst);
            fprintf(file, "sum=%s\n", op->sum);
            fprintf(file, "mode=0%lo\n", op->mode);
            fprintf(file, "size=%ld\n", op->size);
            fprintf(file, "\n");
        }
    }

    /* Print out the list of patched files */
    { struct op_patch_file *op;
      struct delta_option *option;

        for ( op=patch->patch_file_list; op; op=op->next ) {
            fprintf(file, "PATCH FILE %s\n", op->dst);
            for ( option=op->options; option; option=option->next ) {
                fprintf(file, "oldsum=%s\n", option->oldsum);
                fprintf(file, "src=%s\n", option->src);
                fprintf(file, "newsum=%s\n", option->newsum);
            }
            fprintf(file, "mode=0%lo\n", op->mode);
            fprintf(file, "size=%ld\n", op->size);
            fprintf(file, "\n");
        }
    }

    /* Print out the list of obsolete files */
    { struct op_del_file *op;

        for ( op=patch->del_file_list; op; op=op->next ) {
            fprintf(file, "DEL FILE %s\n", op->dst);
            fprintf(file, "\n");
        }
    }

    /* Print out the list of obsolete paths */
    { struct op_del_path *op;

        for ( op=patch->del_path_list; op; op=op->next ) {
            fprintf(file, "DEL PATH %s\n", op->dst);
            fprintf(file, "\n");
        }
    }

    /* That's it! */
    fclose(file);
	return(0);
}
