
#include <stdio.h>

#include "loki_patch.h"
#include "print_patch.h"

void print_info(loki_patch *patch, FILE *output)
{
    struct optional_field *field;

    fprintf(output, "Product: %s\n", patch->product);    
    if ( patch->component ) {
        fprintf(output, "Component: %s\n", patch->component);    
    }
    fprintf(output, "Version: %s\n", patch->version);    
    for ( field=patch->optional_fields; field; field=field->next ) {
        fprintf(output, "%s: %s\n", field->key, field->val);
    }
}
