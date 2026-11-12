# waservix: WAservix WhatsApp API Gateway for C and C++

**WAservix WhatsApp API Gateway**: the official C client by Tekservix. Send WhatsApp texts, images, documents and locations from your own number. Works from C and C++, depends only on libcurl.

**You need a Tekservix account with WAservix and a WhatsApp number connected in the dashboard.** Each number has its own API key (it starts with `wpk_`) under WAservix → Devices. [Create an account](https://tekservix.com/waservix/) · [API docs](https://tekservix.com/waservix/docs/)

## Install

With CMake (needs libcurl):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

Then in your project:

```cmake
find_package(waservix 1.0 REQUIRED)
target_link_libraries(your_app PRIVATE waservix::waservix)
```

pkg-config is supported too (`pkg-config --cflags --libs waservix`).

## Use

```c
#include <waservix/waservix.h>
#include <stdio.h>

int main(void) {
    waservix_global_init();
    waservix_client *c = waservix_client_new("wpk_your_device_key");
    waservix_response r;

    if (waservix_send_text(c, "+250788123456", "Your order #1042 has shipped.", &r) == WASERVIX_OK)
        printf("queued: %s\n", r.body);            /* raw JSON, parse with any library */
    else
        printf("failed (HTTP %ld): %s\n", r.status, r.body ? r.body : r.error);
    waservix_response_free(&r);

    waservix_send_image(c, "+250788123456", "https://example.com/catalogue.jpg", "New arrivals", &r);
    waservix_response_free(&r);
    waservix_send_document(c, "+250788123456", "https://example.com/inv-1042.pdf",
                           "Invoice-1042.pdf", "Your invoice", &r);
    waservix_response_free(&r);
    waservix_send_location(c, "+250788123456", -1.9441, 30.0619, "Our shop", &r);
    waservix_response_free(&r);

    /* Groups: IDs end in @g.us */
    waservix_list_groups(c, &r);
    waservix_response_free(&r);
    waservix_get_status(c, &r);
    waservix_response_free(&r);

    waservix_client_free(c);
    waservix_global_cleanup();
    return 0;
}
```

Every call returns `WASERVIX_OK` for HTTP 2xx, `WASERVIX_ERR_HTTP` when the API answered with an error (see `status` and `body`), or `WASERVIX_ERR_NETWORK` when there was no answer (see `error`).

Webhooks: `waservix_verify_webhook(signature_header, secret)` checks `X-Webhook-Signature` in constant time.

A client may be used from one thread at a time; create one per thread if you send in parallel.

## More WAservix integrations

[npm](https://www.npmjs.com/package/waservix) · [PyPI](https://pypi.org/project/waservix/) · [Java/Kotlin](https://central.sonatype.com/artifact/com.tekservix/waservix) · [All docs](https://tekservix.com/waservix/docs/)

support@tekservix.com · MIT licensed.
