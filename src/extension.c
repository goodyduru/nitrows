#include <stdlib.h>
#include <string.h>

#include "extension.h"

void register_extension(char *key, bool (*parse_offer)(int,char*,uint16_t),
                           uint16_t (*respond_to_offer)(int,char*),
                           bool (*validate_rsv)(int,bool,bool,bool),
                           bool (*process_data)(int,char*,int,char**,int*),
                           void (*close)(int)
                        ) {
    if ( extension_table == NULL ) {
        extension_table = (Extension *)malloc(sizeof(Extension));
        extension_table[0].key = strdup(key);
        extension_table[0].parse_offer = parse_offer;
        extension_table[0].respond_to_offer = respond_to_offer;
        extension_table[0].validate_rsv = validate_rsv;
        extension_table[0].process_data = process_data;
        extension_table[0].close = close;
    }
    else {
        extension_table = (Extension *)realloc(extension_table, sizeof(Extension)*(extension_count+1));
        extension_table[extension_count].key = strdup(key);
        extension_table[extension_count].parse_offer = parse_offer;
        extension_table[extension_count].respond_to_offer = respond_to_offer;
        extension_table[extension_count].validate_rsv = validate_rsv;
        extension_table[extension_count].process_data = process_data;
        extension_table[extension_count].close = close;
        extension_count++;
    }
}