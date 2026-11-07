#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "config.h"

/* Simple memory chunk struct for curl write callback */
struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

/*
 * Queries Google API for models supporting generateContent.
 * Updates g_config.models and g_config.model_count.
 * Returns 0 on success, -1 on failure.
 */
int fetch_remote_models(const char *api_key)
{
    CURL *curl;
    CURLcode res;
    char url[1024];
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    if (!api_key || strlen(api_key) == 0) {
        free(chunk.memory);
        return -1;
    }

    snprintf(url, sizeof(url),
             "https://generativelanguage.googleapis.com/v1beta/models?key=%s", api_key);

    curl = curl_easy_init();
    if (!curl) {
        free(chunk.memory);
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "lseai-solaris/1.0");

    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || chunk.size == 0) {
        if (chunk.memory) free(chunk.memory);
        return -1;
    }

    /*
     * Lightweight stream extraction:
     * Look for: "name": "models/..."
     * Followed closely by: "generateContent"
     */
    char *p = chunk.memory;
    int count = 0;

    while ((p = strstr(p, "\"name\": \"models/")) != NULL && count < MAX_MODELS) {
        p += 16; /* Skip past \"name\": \"models/ */
        char *end_name = strchr(p, '"');
        if (!end_name) break;

        size_t name_len = end_name - p;
        char temp_name[64];
        if (name_len < sizeof(temp_name)) {
            strncpy(temp_name, p, name_len);
            temp_name[name_len] = '\0';

            /* Verify model supports generateContent */
            char *next_model = strstr(end_name, "\"name\": \"models/");
            char *gen_support = strstr(end_name, "\"generateContent\"");

            /* Only add if "generateContent" appears before the next model entry */
            if (gen_support && (!next_model || gen_support < next_model)) {
                strncpy(g_config.models[count], temp_name, sizeof(g_config.models[count]) - 1);
                g_config.models[count][sizeof(g_config.models[count]) - 1] = '\0';
                count++;
            }
        }
        p = end_name;
    }

    free(chunk.memory);

    if (count > 0) {
        g_config.model_count = count;
        return 0;
    }

    return -1;
}
