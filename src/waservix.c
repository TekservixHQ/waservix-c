/* waservix - WAservix WhatsApp API Gateway client for C. MIT licensed. */
#define WASERVIX_BUILDING
#include "waservix/waservix.h"

#include <curl/curl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct waservix_client {
    char *key;
    char *base;
    long timeout;
};

/* ---- small string builder ------------------------------------------------ */

typedef struct sb {
    char *p;
    size_t len, cap;
    int oom;
} sb;

static void sb_add(sb *b, const char *s, size_t n) {
    if (b->oom) return;
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 128;
        char *np;
        while (b->len + n + 1 > cap) cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np) {
            b->oom = 1;
            return;
        }
        b->p = np;
        b->cap = cap;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static void sb_str(sb *b, const char *s) { sb_add(b, s, strlen(s)); }

static void sb_fmt(sb *b, const char *fmt, ...) {
    char tmp[64];
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n > 0) sb_add(b, tmp, (size_t)n < sizeof tmp ? (size_t)n : sizeof tmp - 1);
}

static void sb_json(sb *b, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    sb_add(b, "\"", 1);
    for (; *p; p++) {
        switch (*p) {
            case '"': sb_add(b, "\\\"", 2); break;
            case '\\': sb_add(b, "\\\\", 2); break;
            case '\n': sb_add(b, "\\n", 2); break;
            case '\r': sb_add(b, "\\r", 2); break;
            case '\t': sb_add(b, "\\t", 2); break;
            case '\b': sb_add(b, "\\b", 2); break;
            case '\f': sb_add(b, "\\f", 2); break;
            default:
                if (*p < 0x20) sb_fmt(b, "\\u%04x", *p);
                else sb_add(b, (const char *)p, 1);
        }
    }
    sb_add(b, "\"", 1);
}

static void sb_url(sb *b, const char *s) {
    static const char hex[] = "0123456789ABCDEF";
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    for (; *p; p++) {
        if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-' ||
            *p == '_' || *p == '.' || *p == '~') {
            sb_add(b, (const char *)p, 1);
        } else {
            char e[3];
            e[0] = '%';
            e[1] = hex[*p >> 4];
            e[2] = hex[*p & 15];
            sb_add(b, e, 3);
        }
    }
}

/* trims spaces around s into a new sb */
static void sb_trimmed(sb *b, const char *s, int url) {
    const char *e;
    sb t = {0};
    if (!s) s = "";
    while (*s == ' ' || *s == '\t') s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t')) e--;
    sb_add(&t, s, (size_t)(e - s));
    if (t.oom) {
        b->oom = 1;
    } else if (url) {
        sb_url(b, t.p ? t.p : "");
    } else {
        sb_str(b, t.p ? t.p : "");
    }
    free(t.p);
}

static char *dupstr(const char *s) {
    size_t n = strlen(s);
    char *d = (char *)malloc(n + 1);
    if (d) memcpy(d, s, n + 1);
    return d;
}

static void set_error(waservix_response *r, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->error, sizeof r->error, fmt, ap);
    va_end(ap);
}

/* ---- client ---------------------------------------------------------------- */

int waservix_global_init(void) { return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK ? 0 : -1; }

void waservix_global_cleanup(void) { curl_global_cleanup(); }

waservix_client *waservix_client_new(const char *api_key) {
    return waservix_client_new_with_base(api_key, WASERVIX_DEFAULT_BASE_URL);
}

waservix_client *waservix_client_new_with_base(const char *api_key, const char *base_url) {
    waservix_client *c;
    size_t n;
    if (!api_key || !*api_key || !base_url || !*base_url) return NULL;
    c = (waservix_client *)calloc(1, sizeof *c);
    if (!c) return NULL;
    c->key = dupstr(api_key);
    c->base = dupstr(base_url);
    c->timeout = 30;
    if (!c->key || !c->base) {
        waservix_client_free(c);
        return NULL;
    }
    n = strlen(c->base);
    while (n > 0 && c->base[n - 1] == '/') c->base[--n] = '\0';
    return c;
}

void waservix_client_set_timeout(waservix_client *c, long seconds) {
    if (c && seconds > 0) c->timeout = seconds;
}

void waservix_client_free(waservix_client *c) {
    if (!c) return;
    if (c->key) {
        memset(c->key, 0, strlen(c->key));
        free(c->key);
    }
    free(c->base);
    free(c);
}

void waservix_response_free(waservix_response *r) {
    if (!r) return;
    free(r->body);
    r->body = NULL;
    r->body_len = 0;
}

static size_t on_body(char *data, size_t size, size_t nmemb, void *userp) {
    sb *b = (sb *)userp;
    size_t n = size * nmemb;
    sb_add(b, data, n);
    return b->oom ? 0 : n;
}

waservix_result waservix_request(waservix_client *c, const char *method, const char *path, const char *json_body,
                                 waservix_response *out) {
    CURL *h;
    CURLcode rc;
    struct curl_slist *hdrs = NULL;
    sb url = {0}, auth = {0}, body = {0};
    waservix_result res;

    if (!out) return WASERVIX_ERR_ARGS;
    memset(out, 0, sizeof *out);
    if (!c || !method || !path) {
        set_error(out, "invalid arguments");
        return WASERVIX_ERR_ARGS;
    }
    h = curl_easy_init();
    if (!h) {
        set_error(out, "curl_easy_init failed");
        return WASERVIX_ERR_NETWORK;
    }
    sb_str(&url, c->base);
    sb_str(&url, path);
    sb_str(&auth, "Authorization: Bearer ");
    sb_str(&auth, c->key);
    if (url.oom || auth.oom) {
        res = WASERVIX_ERR_MEMORY;
        set_error(out, "out of memory");
        goto done;
    }
    hdrs = curl_slist_append(hdrs, auth.p);
    hdrs = curl_slist_append(hdrs, "Accept: application/json");
    if (json_body) hdrs = curl_slist_append(hdrs, "Content-Type: application/json");

    curl_easy_setopt(h, CURLOPT_URL, url.p);
    curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, method);
    curl_easy_setopt(h, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(h, CURLOPT_USERAGENT, "waservix-c/" WASERVIX_VERSION);
    curl_easy_setopt(h, CURLOPT_TIMEOUT, c->timeout);
    curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, on_body);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &body);
    if (json_body) {
        curl_easy_setopt(h, CURLOPT_POSTFIELDS, json_body);
        curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE, (long)strlen(json_body));
    }

    rc = curl_easy_perform(h);
    if (body.oom) {
        res = WASERVIX_ERR_MEMORY;
        set_error(out, "out of memory");
        goto done;
    }
    out->body = body.p;
    out->body_len = body.len;
    body.p = NULL;
    if (rc != CURLE_OK) {
        res = WASERVIX_ERR_NETWORK;
        set_error(out, "%s", curl_easy_strerror(rc));
        goto done;
    }
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &out->status);
    if (out->status >= 200 && out->status < 300) {
        res = WASERVIX_OK;
    } else {
        res = WASERVIX_ERR_HTTP;
        set_error(out, "WAservix API error %ld", out->status);
    }

done:
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(h);
    if (auth.p) {
        memset(auth.p, 0, auth.len);
        free(auth.p);
    }
    free(url.p);
    free(body.p);
    return res;
}

/* ---- helpers ---------------------------------------------------------------- */

static waservix_result run(waservix_client *c, const char *method, sb *path, sb *body, waservix_response *out) {
    waservix_result r;
    if (path->oom || (body && body->oom)) {
        if (out) {
            memset(out, 0, sizeof *out);
            set_error(out, "out of memory");
        }
        r = WASERVIX_ERR_MEMORY;
    } else {
        r = waservix_request(c, method, path->p, body ? body->p : NULL, out);
    }
    free(path->p);
    if (body) free(body->p);
    return r;
}

static waservix_result args_error(waservix_response *out) {
    if (out) {
        memset(out, 0, sizeof *out);
        set_error(out, "invalid arguments");
    }
    return WASERVIX_ERR_ARGS;
}

static void field(sb *b, const char *name, const char *value) {
    if (!value) return;
    sb_add(b, ",", 1);
    sb_json(b, name);
    sb_add(b, ":", 1);
    sb_json(b, value);
}

static waservix_result send_built(waservix_client *c, sb *body, waservix_response *out) {
    sb path = {0};
    sb_str(&path, "/send-message");
    sb_add(body, "}", 1);
    return run(c, "POST", &path, body, out);
}

static void start_send(sb *body, const char *to) {
    sb_str(body, "{\"to\":");
    sb_json(body, to);
}

waservix_result waservix_send_text(waservix_client *c, const char *to, const char *text, waservix_response *out) {
    sb body = {0};
    if (!to || !*to || !text) return args_error(out);
    start_send(&body, to);
    field(&body, "text", text);
    return send_built(c, &body, out);
}

waservix_result waservix_send_image(waservix_client *c, const char *to, const char *image_url, const char *caption,
                                    waservix_response *out) {
    sb body = {0};
    if (!to || !*to || !image_url) return args_error(out);
    start_send(&body, to);
    field(&body, "imageUrl", image_url);
    field(&body, "text", caption);
    return send_built(c, &body, out);
}

waservix_result waservix_send_document(waservix_client *c, const char *to, const char *document_url,
                                       const char *file_name, const char *caption, waservix_response *out) {
    sb body = {0};
    if (!to || !*to || !document_url) return args_error(out);
    start_send(&body, to);
    field(&body, "documentUrl", document_url);
    field(&body, "fileName", file_name);
    field(&body, "text", caption);
    return send_built(c, &body, out);
}

waservix_result waservix_send_location(waservix_client *c, const char *to, double latitude, double longitude,
                                       const char *name, waservix_response *out) {
    sb body = {0};
    char num[64];
    if (!to || !*to || latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180)
        return args_error(out);
    start_send(&body, to);
    snprintf(num, sizeof num, ",\"location\":{\"latitude\":%.7f", latitude);
    sb_str(&body, num);
    snprintf(num, sizeof num, ",\"longitude\":%.7f", longitude);
    sb_str(&body, num);
    field(&body, "name", name);
    sb_add(&body, "}", 1);
    return send_built(c, &body, out);
}

waservix_result waservix_send_message(waservix_client *c, const char *to, const char *extra_fields_json,
                                      waservix_response *out) {
    sb body = {0};
    if (!to || !*to) return args_error(out);
    start_send(&body, to);
    if (extra_fields_json && *extra_fields_json) {
        sb_add(&body, ",", 1);
        sb_str(&body, extra_fields_json);
    }
    return send_built(c, &body, out);
}

static waservix_result by_id(waservix_client *c, const char *method, const char *fmt, long id, sb *body,
                             waservix_response *out) {
    sb path = {0};
    sb_fmt(&path, fmt, id);
    return run(c, method, &path, body, out);
}

waservix_result waservix_get_message_info(waservix_client *c, long id, waservix_response *out) {
    return by_id(c, "GET", "/messages/%ld/info", id, NULL, out);
}

waservix_result waservix_edit_message(waservix_client *c, long id, const char *text, waservix_response *out) {
    sb body = {0};
    if (!text) return args_error(out);
    sb_str(&body, "{\"text\":");
    sb_json(&body, text);
    sb_add(&body, "}", 1);
    return by_id(c, "PUT", "/messages/%ld", id, &body, out);
}

waservix_result waservix_delete_message(waservix_client *c, long id, waservix_response *out) {
    return by_id(c, "DELETE", "/messages/%ld", id, NULL, out);
}

waservix_result waservix_resend_message(waservix_client *c, long id, waservix_response *out) {
    return by_id(c, "POST", "/messages/%ld/resend", id, NULL, out);
}

static waservix_result get_with(waservix_client *c, const char *prefix, const char *value, const char *suffix,
                                int encode, waservix_response *out) {
    sb path = {0};
    if (!value || !*value) return args_error(out);
    sb_str(&path, prefix);
    sb_trimmed(&path, value, encode);
    if (suffix) sb_str(&path, suffix);
    return run(c, "GET", &path, NULL, out);
}

waservix_result waservix_on_whatsapp(waservix_client *c, const char *phone, waservix_response *out) {
    return get_with(c, "/on-whatsapp/", phone, NULL, 1, out);
}

waservix_result waservix_get_status(waservix_client *c, waservix_response *out) {
    return waservix_request(c, "GET", "/status", NULL, out);
}

waservix_result waservix_list_contacts(waservix_client *c, waservix_response *out) {
    return waservix_request(c, "GET", "/contacts", NULL, out);
}

waservix_result waservix_get_contact(waservix_client *c, const char *phone, waservix_response *out) {
    return get_with(c, "/contacts/", phone, NULL, 1, out);
}

waservix_result waservix_list_groups(waservix_client *c, waservix_response *out) {
    return waservix_request(c, "GET", "/groups", NULL, out);
}

waservix_result waservix_get_group_metadata(waservix_client *c, const char *group_id, waservix_response *out) {
    /* the group ID (…@g.us) is sent as is */
    return get_with(c, "/groups/", group_id, "/metadata", 0, out);
}

/* ---- webhooks and utilities ---------------------------------------------------- */

int waservix_verify_webhook(const char *signature, const char *secret) {
    size_t n, i;
    unsigned char diff = 0;
    if (!signature || !secret || !*secret) return 0;
    n = strlen(secret);
    if (strlen(signature) != n) return 0;
    for (i = 0; i < n; i++) diff |= (unsigned char)(signature[i] ^ secret[i]);
    return diff == 0;
}

char *waservix_json_string(const char *s) {
    sb b = {0};
    sb_json(&b, s);
    if (b.oom) {
        free(b.p);
        return NULL;
    }
    return b.p;
}

char *waservix_url_encode(const char *s) {
    sb b = {0};
    sb_add(&b, "", 0);
    sb_url(&b, s);
    if (b.oom) {
        free(b.p);
        return NULL;
    }
    return b.p;
}
