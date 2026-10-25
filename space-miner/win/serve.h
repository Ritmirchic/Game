/* =========================================================================
   SPACE MINER — game file server (platform independent core)

   Serves a single-folder game build over HTTP/1.1 with strict path handling,
   correct MIME types, HEAD support and a /__ping heartbeat endpoint.

   Host hooks (provided by the platform layer):
       extern const char *srv_root;                     game folder (no slash)
       int  srv_file_exists(const char *path);
       int  srv_read_file(const char *path, unsigned char **out, unsigned int *len);
       void srv_free_file(unsigned char *buf);
       void srv_note_request(void);                     heartbeat bookkeeping
   ========================================================================= */
#ifndef SPACE_MINER_SERVE_H
#define SPACE_MINER_SERVE_H

#include <string.h>
#include <stdio.h>
#include "http_util.h"

typedef struct {
    int (*write)(void *ctx, const char *data, int len);
    void *ctx;
} srv_out;

/* host hooks — the platform layer defines these */
extern const char *srv_root;
int  srv_file_exists(const char *path);
int  srv_read_file(const char *path, unsigned char **out, unsigned int *len);
void srv_free_file(unsigned char *buf);
void srv_note_request(void);

static const char *srv_reason(int code)
{
    switch (code) {
    case 200: return "200 OK";
    case 400: return "400 Bad Request";
    case 403: return "403 Forbidden";
    case 404: return "404 Not Found";
    case 405: return "405 Method Not Allowed";
    case 500: return "500 Internal Server Error";
    default:  return "200 OK";
    }
}

static int srv_emit_head(srv_out *out, int code, const char *type, unsigned int len,
                         int cache, int headOnly)
{
    char head[512];
    int hl = _snprintf(head, sizeof(head),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %u\r\n"
        "Cache-Control: %s\r\n"
        "Connection: close\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "\r\n",
        srv_reason(code), type, len, cache ? "max-age=3600" : "no-store");
    if (hl <= 0) return 0;
    if (hl > (int)sizeof(head)) hl = (int)sizeof(head);
    out->write(out->ctx, head, hl);
    return headOnly ? 0 : 1;
}

static void srv_text(srv_out *out, int code, const char *msg, int headOnly)
{
    if (srv_emit_head(out, code, "text/plain; charset=utf-8", (unsigned int)strlen(msg), 0, headOnly))
        out->write(out->ctx, msg, (int)strlen(msg));
}

/* Returns the HTTP status code that was sent (useful for logging/tests). */
static int serve_request(const char *method, const char *target, int headOnly, srv_out *out)
{
    char rel[512];
    char full[1024];
    unsigned char *body = NULL;
    unsigned int blen = 0;
    int cache;

    if (!method || !target || !out || !out->write) return 0;

    if (!strcmp(target, "/__ping") || !strncmp(target, "/__ping?", 8)) {
        srv_note_request();
        srv_text(out, 200, "pong", headOnly);
        return 200;
    }
    if (strcmp(method, "GET") && strcmp(method, "HEAD")) {
        srv_text(out, 405, "method not allowed", headOnly);
        return 405;
    }
    if (!url_to_rel(target, rel, sizeof(rel))) {
        srv_text(out, 400, "bad path", headOnly);
        return 400;
    }

    path_join(full, sizeof(full), srv_root, rel);
    if (!srv_file_exists(full)) {
        path_join(full, sizeof(full), srv_root, "index.html");
        if (!srv_file_exists(full)) {
            srv_text(out, 404, "not found", headOnly);
            return 404;
        }
    }
    if (!srv_read_file(full, &body, &blen)) {
        srv_text(out, 500, "read failed", headOnly);
        return 500;
    }
    srv_note_request();
    cache = !strstr(rel, ".html");
    if (srv_emit_head(out, 200, mime_for(full), blen, cache, headOnly) && blen) {
        out->write(out->ctx, (const char *)body, (int)blen);
    }
    srv_free_file(body);
    return 200;
}

#endif /* SPACE_MINER_SERVE_H */
