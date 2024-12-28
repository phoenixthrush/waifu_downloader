#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <stdlib.h>

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t real_size = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + real_size + 1);
    if (ptr == NULL) {
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, real_size);
    mem->size += real_size;
    mem->memory[mem->size] = 0;

    return real_size;
}

char *get_image_url_from_response(const char *json_response) {
    const char *key = "\"url\":\"";
    char *start = strstr(json_response, key);

    if (start) {
        start += strlen(key);
        char *end = strchr(start, '\"');
        if (end) {
            size_t url_length = end - start;
            char *url = (char *)malloc(url_length + 1);
            if (url) {
                strncpy(url, start, url_length);
                url[url_length] = '\0';
                return url;
            }
        }
    }
    return NULL;
}

int main(void) {
    CURL *http_handle;
    CURLM *multi_handle;
    int still_running = 1;
    struct MemoryStruct chunk;

    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    http_handle = curl_easy_init();

    curl_easy_setopt(http_handle, CURLOPT_URL, "https://api.waifu.im/search");
    curl_easy_setopt(http_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(http_handle, CURLOPT_WRITEDATA, (void *)&chunk);

    multi_handle = curl_multi_init();

    curl_multi_add_handle(multi_handle, http_handle);

    do {
        CURLMcode mc = curl_multi_perform(multi_handle, &still_running);

        if (!mc)
            mc = curl_multi_poll(multi_handle, NULL, 0, 1000, NULL);

        if (mc) {
            fprintf(stderr, "curl_multi_poll() failed, code %d.\n", (int)mc);
            break;
        }

    } while (still_running);

    curl_multi_remove_handle(multi_handle, http_handle);

    curl_easy_cleanup(http_handle);

    curl_multi_cleanup(multi_handle);

    if (chunk.memory) {
        char *image_url = get_image_url_from_response(chunk.memory);
        if (image_url) {
            printf("Extracted URL: %s\n", image_url);
            free(image_url);
        } else {
            printf("URL not found in the response.\n");
        }
        free(chunk.memory);
    }

    curl_global_cleanup();

    return 0;
}
