#include <stdio.h>

#include "header.h"

int tests() {
    char tokens[] = "foo,bar;baz,foo;baz";
    char tokens1[] = "foo;bar;baz=1;bar=2";
    ExtensionList *list = get_extension_list(1);
    parse_extensions(1, tokens, list);
    parse_extensions(1, tokens1, list);
    printf("Testing tokens: %s\n", tokens);
    print_list(list);
}