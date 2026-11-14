#include <stdlib.h>
#include "config.h"

const char *config_get_openalex_key(void)
{
    const char *key = getenv("NnJKTHgw6hrnhMuaiKH7gM");
    if (key == NULL || key[0] == '\0')
        return NULL;
    return key;
}
