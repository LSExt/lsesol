#include "lseai.h"
#include "globals.h"


/* Memory callback for libcurl */
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryBuffer *mem = (struct MemoryBuffer *)userp;

    char *ptr = (char *)realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "Out of memory in realloc()\n");
        return 0;
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';

    return realsize;
}


/* File-scoped persistent handle */
static CURL *s_curl = NULL;

/* Call this during application shutdown (e.g. from File -> Exit callback) */
void cleanup_curl_subsystem(void)
{
    if (s_curl) {
        curl_easy_cleanup(s_curl);
        s_curl = NULL;
    }
}

/* Perform HTTPS POST request reusing persistent TLS connection */
char *send_request(const char *json_payload)
{
    const char *api_key = getenv("GEMINI_API_KEY");
    CURLcode res;
    struct curl_slist *headers = NULL;
    int retries = 3;
    long http_code = 0;
    char endpoint[1024];

    if (!api_key) {
        fprintf(stderr, "[Error: GEMINI_API_KEY environment variable is not set]\n");
        return NULL;
    }

    /* Initialize persistent handle on first use */
    if (!s_curl) {
        s_curl = curl_easy_init();
        if (!s_curl) {
            fprintf(stderr, "[Error: curl_easy_init() failed]\n");
            return NULL;
        }

        /* Set one-time permanent options */
        curl_easy_setopt(s_curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(s_curl, CURLOPT_TCP_KEEPALIVE, 1L);
        /* Send first probe after 60s idle */
        curl_easy_setopt(s_curl, CURLOPT_TCP_KEEPIDLE, 60L);
        /* Subsequent probes every 30s */
        curl_easy_setopt(s_curl, CURLOPT_TCP_KEEPINTVL, 30L);

/* (Avoids sending a POST payload into a half-closed TCP pipe) */
#if LIBCURL_VERSION_NUM >= 0x072400 /* 7.36.0+ */
        curl_easy_setopt(s_curl, CURLOPT_EXPECT_100_TIMEOUT_MS, 0L);
#endif

        /* SPARC TLS Handshake Optimizations */
        curl_easy_setopt(s_curl, CURLOPT_CONNECTTIMEOUT, 60L);
        curl_easy_setopt(s_curl, CURLOPT_TIMEOUT, 120L);
        curl_easy_setopt(s_curl, CURLOPT_SSL_CIPHER_LIST,
            "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:"
            "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:HIGH");
        curl_easy_setopt(s_curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(s_curl, CURLOPT_SSL_VERIFYHOST, 2L);

        /* Use small, Google-specific CA bundle to avoid parsing large root stores */
        /* curl_easy_setopt(s_curl, CURLOPT_CAINFO, "/usr/local/lse/certs/google_roots.pem"); */
    }

    /* Format URL */
    snprintf(endpoint, sizeof(endpoint),
             "https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s",
             g_current_model, api_key);

    headers = curl_slist_append(headers, "Content-Type: application/json");

    while (retries > 0) {
        struct MemoryBuffer chunk;
        chunk.data = (char *)malloc(1);
        if (!chunk.data) {
            fprintf(stderr, "[Error: malloc(1) failed]\n");
            curl_slist_free_all(headers);
            return NULL;
        }
        chunk.data[0] = '\0';
        chunk.size = 0;

        /* Apply per-request options */
        curl_easy_setopt(s_curl, CURLOPT_URL, endpoint);
        curl_easy_setopt(s_curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(s_curl, CURLOPT_POSTFIELDS, json_payload);
        curl_easy_setopt(s_curl, CURLOPT_WRITEDATA, (void *)&chunk);

        res = curl_easy_perform(s_curl);
        curl_easy_getinfo(s_curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (res == CURLE_OK) {
            if (http_code == 200) {
                /* Success */
                curl_slist_free_all(headers);
                return chunk.data;
            } else if (http_code == 503 && retries > 1) {
                fprintf(stderr, "[503 Server Busy - Retrying in 2s... (%d left)]\n", retries - 1);
                free(chunk.data);
                sleep(2);
            } else {
                /* Return JSON error payload for parsing/displaying error messages */
                fprintf(stderr, "API returned HTTP %ld\n", http_code);
                curl_slist_free_all(headers);
                return chunk.data;
            }
        } else {
            fprintf(stderr, "\ncurl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(chunk.data);
            /* If connection reset or closed by peer, breaking or retrying will auto-reconnect */
            break;
        }

        retries--;
    }

    curl_slist_free_all(headers);
    fprintf(stderr, "Unable to obtain success response\n");
    return NULL;
}
