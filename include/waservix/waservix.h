/*
 * waservix - WAservix WhatsApp API Gateway client for C and C++, by Tekservix.
 *
 * Send WhatsApp texts, images, documents and locations from your own number.
 * You need a Tekservix account with WAservix and a WhatsApp number connected in
 * the dashboard. Each number has its own device API key (it starts with
 * "wpk_") under WAservix -> Devices. https://tekservix.com/waservix/
 * API reference: https://tekservix.com/waservix/docs/
 *
 * Every call fills a waservix_response with the HTTP status and the raw JSON
 * body, so you can parse it with any JSON library. Free it with
 * waservix_response_free().
 *
 * MIT licensed.
 */
#ifndef WASERVIX_H
#define WASERVIX_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(WASERVIX_SHARED)
#  ifdef WASERVIX_BUILDING
#    define WASERVIX_API __declspec(dllexport)
#  else
#    define WASERVIX_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#  define WASERVIX_API __attribute__((visibility("default")))
#else
#  define WASERVIX_API
#endif

#define WASERVIX_VERSION "1.0.0"
#define WASERVIX_DEFAULT_BASE_URL "https://tekservix.com/waservix/api"

typedef enum waservix_result {
    WASERVIX_OK = 0,          /* HTTP 2xx */
    WASERVIX_ERR_HTTP = 1,    /* the API answered with HTTP 4xx/5xx; see status and body */
    WASERVIX_ERR_NETWORK = 2, /* no answer (DNS, TLS, timeout...); see error */
    WASERVIX_ERR_ARGS = 3,    /* invalid arguments */
    WASERVIX_ERR_MEMORY = 4   /* out of memory */
} waservix_result;

typedef struct waservix_response {
    long status;      /* HTTP status, 0 when there was no answer */
    char *body;       /* raw JSON body, NUL-terminated (may be NULL) */
    size_t body_len;
    char error[256];  /* human-readable error for non-OK results */
} waservix_response;

typedef struct waservix_client waservix_client;

/* Call once at program start / end (wraps curl_global_init/cleanup). Optional
 * if your program already initialises libcurl. */
WASERVIX_API int waservix_global_init(void);
WASERVIX_API void waservix_global_cleanup(void);

/* Creates a client for one WhatsApp number. Returns NULL if api_key is empty. */
WASERVIX_API waservix_client *waservix_client_new(const char *api_key);
/* Same with another base URL (for tests or a proxy). */
WASERVIX_API waservix_client *waservix_client_new_with_base(const char *api_key, const char *base_url);
/* Request timeout in seconds (default 30). */
WASERVIX_API void waservix_client_set_timeout(waservix_client *c, long seconds);
WASERVIX_API void waservix_client_free(waservix_client *c);

WASERVIX_API void waservix_response_free(waservix_response *r);

/* Calls any endpoint. path starts with "/", json_body may be NULL. */
WASERVIX_API waservix_result waservix_request(waservix_client *c, const char *method, const char *path,
                                              const char *json_body, waservix_response *out);

/* Sending. "to" is a number (+250788123456) or a group ID (ending in @g.us).
 * Optional arguments may be NULL. */
WASERVIX_API waservix_result waservix_send_text(waservix_client *c, const char *to, const char *text,
                                                waservix_response *out);
WASERVIX_API waservix_result waservix_send_image(waservix_client *c, const char *to, const char *image_url,
                                                 const char *caption, waservix_response *out);
WASERVIX_API waservix_result waservix_send_document(waservix_client *c, const char *to, const char *document_url,
                                                    const char *file_name, const char *caption,
                                                    waservix_response *out);
WASERVIX_API waservix_result waservix_send_location(waservix_client *c, const char *to, double latitude,
                                                    double longitude, const char *name, waservix_response *out);
/* Any message type: extra_fields_json is the inside of a JSON object with API
 * field names, e.g. "\"videoUrl\":\"https://...\",\"text\":\"Demo\"". */
WASERVIX_API waservix_result waservix_send_message(waservix_client *c, const char *to,
                                                   const char *extra_fields_json, waservix_response *out);

/* Messages. */
WASERVIX_API waservix_result waservix_get_message_info(waservix_client *c, long message_id,
                                                       waservix_response *out);
WASERVIX_API waservix_result waservix_edit_message(waservix_client *c, long message_id, const char *text,
                                                   waservix_response *out);
WASERVIX_API waservix_result waservix_delete_message(waservix_client *c, long message_id, waservix_response *out);
WASERVIX_API waservix_result waservix_resend_message(waservix_client *c, long message_id, waservix_response *out);

/* Numbers, contacts and groups. */
WASERVIX_API waservix_result waservix_on_whatsapp(waservix_client *c, const char *phone, waservix_response *out);
WASERVIX_API waservix_result waservix_get_status(waservix_client *c, waservix_response *out);
WASERVIX_API waservix_result waservix_list_contacts(waservix_client *c, waservix_response *out);
WASERVIX_API waservix_result waservix_get_contact(waservix_client *c, const char *phone, waservix_response *out);
WASERVIX_API waservix_result waservix_list_groups(waservix_client *c, waservix_response *out);
WASERVIX_API waservix_result waservix_get_group_metadata(waservix_client *c, const char *group_id,
                                                         waservix_response *out);

/* Checks the X-Webhook-Signature header against your number's webhook secret,
 * in constant time. Returns 1 if they match. */
WASERVIX_API int waservix_verify_webhook(const char *signature, const char *secret);

/* Escapes s as a JSON string including the quotes. Caller frees. */
WASERVIX_API char *waservix_json_string(const char *s);
/* Percent-encodes s for a URL path segment. Caller frees. */
WASERVIX_API char *waservix_url_encode(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* WASERVIX_H */
