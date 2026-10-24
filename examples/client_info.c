/* SPDX-License-Identifier: LGPL-3.0-only */
/* Link with xdb_client; the server.info envelope is printed for the caller. */
#include "xdb/xdb.h"
#include <stdio.h>

int main(void) {
    xdb_client *client = xdb_client_connect(0, NULL, NULL);
    if (!client) {
        fprintf(stderr, "connect: %s\n", xdb_last_error());
        return 1;
    }
    xdb_response *response = NULL;
    if (!xdb_client_request(client, "server.info", "{}", &response)) {
        fprintf(stderr, "request: %s\n", xdb_last_error());
        xdb_client_destroy(client);
        return 1;
    }
    /* Return 1 from request means response receipt. Parse `ok` for RPC success. */
    puts(xdb_response_json(response));
    xdb_response_destroy(response);
    xdb_client_destroy(client);
    return 0;
}
