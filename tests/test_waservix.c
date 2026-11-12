/* Offline tests, plus one live call when WASERVIX_LIVE_TEST=true. */
#include <waservix/waservix.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void check(const char *name, int ok) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    if (!ok) failures++;
}

int main(void) {
    const char *live;
    char *s;

    check("matching secret is accepted", waservix_verify_webhook("s3cret-value", "s3cret-value") == 1);
    check("different secret is refused", waservix_verify_webhook("s3cret-valuf", "s3cret-value") == 0);
    check("shorter secret is refused", waservix_verify_webhook("short", "s3cret-value") == 0);
    check("empty secret is refused", waservix_verify_webhook("", "") == 0);
    check("NULL signature is refused", waservix_verify_webhook(NULL, "s3cret-value") == 0);

    s = waservix_json_string("Line 1\nSaid \"hi\" \\ \x02");
    check("JSON escaping", s && strcmp(s, "\"Line 1\\nSaid \\\"hi\\\" \\\\ \\u0002\"") == 0);
    free(s);

    s = waservix_url_encode("+250 788@x");
    check("URL encoding", s && strcmp(s, "%2B250%20788%40x") == 0);
    free(s);
    s = waservix_url_encode("");
    check("URL encoding of empty string", s && strcmp(s, "") == 0);
    free(s);

    check("empty key is refused", waservix_client_new("") == NULL);
    check("NULL key is refused", waservix_client_new(NULL) == NULL);
    {
        waservix_response r;
        waservix_client *c = waservix_client_new("wpk_x");
        check("client is created", c != NULL);
        check("send without a recipient is refused", waservix_send_text(c, "", "hi", &r) == WASERVIX_ERR_ARGS);
        check("bad latitude is refused",
              waservix_send_location(c, "+250788123456", 95.0, 30.0, NULL, &r) == WASERVIX_ERR_ARGS);
        check("group lookup without an ID is refused",
              waservix_get_group_metadata(c, NULL, &r) == WASERVIX_ERR_ARGS);
        waservix_client_free(c);
    }

    live = getenv("WASERVIX_LIVE_TEST");
    if (live && strcmp(live, "true") == 0) {
        waservix_response r;
        waservix_client *c;
        waservix_result res;
        waservix_global_init();
        c = waservix_client_new("wpk_invalid_c_test");
        res = waservix_get_status(c, &r);
        printf("     live: result=%d status=%ld error=%s\n", (int)res, r.status, r.error);
        check("live API rejects an invalid key with 401", res == WASERVIX_ERR_HTTP && r.status == 401);
        waservix_response_free(&r);
        waservix_client_free(c);
        waservix_global_cleanup();
    } else {
        printf("skip live test (set WASERVIX_LIVE_TEST=true)\n");
    }

    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
