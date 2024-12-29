#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <stdlib.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#define BUFFER_SIZE 1024

struct MemoryStruct
{
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t real_size = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + real_size + 1);
    if (!ptr)
    {
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, real_size);
    mem->size += real_size;
    mem->memory[mem->size] = '\0';

    return real_size;
}

char *get_image_url_from_response(const char *json_response)
{
    const char *key = "\"url\":\"";
    char *start = strstr(json_response, key);

    if (start)
    {
        start += strlen(key);
        char *end = strchr(start, '\"');
        if (end)
        {
            size_t url_length = end - start;
            char *url = malloc(url_length + 1);
            if (url)
            {
                strncpy(url, start, url_length);
                url[url_length] = '\0';
                return url;
            }
        }
    }
    return NULL;
}

char *fetch_data_from_url(const char *url)
{
    CURL *http_handle;
    struct MemoryStruct chunk = {0};

    http_handle = curl_easy_init();
    if (!http_handle)
    {
        fprintf(stderr, "Error initializing curl.\n");
        return NULL;
    }

    curl_easy_setopt(http_handle, CURLOPT_URL, url);
    curl_easy_setopt(http_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(http_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(http_handle, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(http_handle);
    if (res != CURLE_OK)
    {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(http_handle);
        return NULL;
    }

    curl_easy_cleanup(http_handle);

    return chunk.memory;
}

void print_usage(const char *prog_name)
{
    printf("Usage: %s [options...]\n", prog_name);
    printf(" -c, --count <n>           Set image fetch count\n");
    printf(" -d, --directory <dir>     Set output directory (default: 'waifus')\n");
    printf(" -h, --help                Get help for commands\n");
}

int create_directory(const char *dir)
{
#ifdef _WIN32
    if (_mkdir(dir) != 0 && errno != 17)
    {
        perror("Error creating directory");
        return 1;
    }
#else
    struct stat st = {0};
    if (stat(dir, &st) == -1 && mkdir(dir, 0700) != 0)
    {
        perror("Error creating directory");
        return 1;
    }
#endif
    return 0;
}

char *extract_filename_from_url(const char *url)
{
    const char *slash = strrchr(url, '/');
    return slash ? strdup(slash + 1) : NULL;
}

size_t write_image_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    return fwrite(ptr, size, nmemb, (FILE *)stream);
}

int download_image(const char *url, const char *file_path)
{
    CURL *curl_handle;
    FILE *image_file;

    curl_handle = curl_easy_init();
    if (!curl_handle)
    {
        fprintf(stderr, "Error initializing curl.\n");
        return 1;
    }

    image_file = fopen(file_path, "wb");
    if (!image_file)
    {
        fprintf(stderr, "Error opening file for writing: %s\n", file_path);
        curl_easy_cleanup(curl_handle);
        return 1;
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_image_data);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, image_file);
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK)
    {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        fclose(image_file);
        curl_easy_cleanup(curl_handle);
        return 1;
    }

    fclose(image_file);
    curl_easy_cleanup(curl_handle);
    return 0;
}

int main(int argc, char *argv[])
{
    int count = 1;
    const char *directory = "waifus";
    const char *url = "https://api.waifu.im/search?is_nsfw=true";

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--count") == 0 || strcmp(argv[i], "-c") == 0)
        {
            if (i + 1 < argc)
            {
                count = atoi(argv[i + 1]);
                i++;
            }
            else
            {
                printf("Error: --count/-c requires a number as argument.\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "--directory") == 0 || strcmp(argv[i], "-d") == 0)
        {
            if (i + 1 < argc)
            {
                directory = argv[i + 1];
                i++;
            }
            else
            {
                printf("Error: --directory/-d requires a directory path.\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (create_directory(directory) != 0)
    {
        return 1;
    }

    for (int i = 0; i < count; i++)
    {
        printf("Fetching data, attempt %d/%d...\n", i + 1, count);

        char *response = fetch_data_from_url(url);
        if (response)
        {
            char *image_url = get_image_url_from_response(response);
            if (image_url)
            {
                printf("Extracted URL: %s\n", image_url);

                char *filename = extract_filename_from_url(image_url);
                if (filename)
                {
                    char file_path[BUFFER_SIZE];
                    snprintf(file_path, sizeof(file_path), "%s/%s", directory, filename);

                    if (download_image(image_url, file_path) == 0)
                    {
                        printf("Saved image to %s\n", file_path);
                    }

                    free(filename);
                }
                free(image_url);
            }
            else
            {
                printf("URL not found in the response.\n");
            }
            free(response);
        }
        else
        {
            printf("Failed to fetch data from the URL.\n");
        }
    }

    return 0;
}
