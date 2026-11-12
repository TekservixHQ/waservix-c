/* Usage: WASERVIX_API_KEY=wpk_... ./waservix_send +250788123456 "Hello from C" */
#include <waservix/waservix.h>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    const char *key = getenv("WASERVIX_API_KEY");
    waservix_client *c;
    waservix_response r;
    waservix_result res;

    if (argc < 3 || !key) {
        fprintf(stderr, "usage: WASERVIX_API_KEY=wpk_... %s <number> <message>\n", argv[0]);
        return 2;
    }
    waservix_global_init();
    c = waservix_client_new(key);
    res = waservix_send_text(c, argv[1], argv[2], &r);
    printf("HTTP %ld\n%s\n", r.status, r.body ? r.body : r.error);
    waservix_response_free(&r);
    waservix_client_free(c);
    waservix_global_cleanup();
    return res == WASERVIX_OK ? 0 : 1;
}
