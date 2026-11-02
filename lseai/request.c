#include "lseai.h"

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


/* Perform HTTPS POST request using libcurl */
char *send_request(const char *json_payload) {
    const char *api_key = getenv("GEMINI_API_KEY");
    const char *model_name = getenv("GEMINI_MODEL");
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    int retries = 3;
    long http_code = 0;


    if (!api_key) {
        fprintf(stderr, "[Error: GEMINI_API_KEY environment variable is not set]\n");
        return NULL;
    }

    /* Build endpoint URL internally */
    char endpoint[1024];
    snprintf(endpoint, sizeof(endpoint),
             "https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s",
             model_name, api_key);


    headers = curl_slist_append(headers, "Content-Type: application/json");

    while (retries > 0) {
        struct MemoryBuffer chunk = { (char *)malloc(1), 0 };
        if (!chunk.data){
            fprintf( stderr, "Failed to malloc(1)\n" );
            return NULL;
        }


        curl = curl_easy_init();
        if (!curl) {
            fprintf( stderr, "failed curl_easy_init()\n" );
            free(chunk.data);
            return NULL;
        }

        curl_easy_setopt(curl, CURLOPT_URL, endpoint);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (res == CURLE_OK && http_code == 200) {
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
            fprintf( stderr, "Success http_code =200 chunk.data: %s \n", chunk.data);
            return chunk.data;
        }

        /* If 503 Service Unavailable, wait 2 seconds and retry */
        if (http_code == 503 && retries > 1) {
            printf("[503 Server Busy - Retrying in 2s... (%d left)]\n", retries - 1);
            sleep(2);
        } else if (res != CURLE_OK) {
            fprintf(stderr, "\ncurl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            free(chunk.data);
            break;
        } else {
            /* Non-200 HTTP code (e.g., 404, 429) - return raw JSON payload for inspection */
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
            fprintf( stderr, "Non-200 return return %li chunk.data %s\n", http_code, chunk.data );
            return chunk.data;
        }

        curl_easy_cleanup(curl);
        free(chunk.data);
        retries--;
    }

    curl_slist_free_all(headers);
    fprintf( stderr, "unable to obtain success response\n");
    return NULL;
}

