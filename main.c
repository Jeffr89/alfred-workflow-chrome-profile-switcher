#include "cjson/cJSON.h"
#include <curl/curl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define PATH_MAX_LEN 4096

char *remove_space(char *str) {
  int i, j = 0;
  for (i = 0; str[i] != '\0'; i++) {
    if (str[i] != ' ') {
      str[j] = str[i];
      j++;
    }
  }
  str[j] = '\0';
  return str;
}

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  return fwrite(ptr, size, nmemb, stream);
}

int download_file(const char *url, const char *filepath) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    // fprintf(stderr, "Can' initialize CURL.\n");

    return 0; // Fallimento
  }

  FILE *fp = fopen(filepath, "wb");
  if (!fp) {
    // fprintf(stderr, "Can't write the following file: %s.\n", filepath);
    curl_easy_cleanup(curl);
    return 0; // Fallimento
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
  // Aggiungi un user-agent per evitare blocchi da alcuni server
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

  CURLcode res = curl_easy_perform(curl);
  fclose(fp);
  curl_easy_cleanup(curl);
  if (res != CURLE_OK) {
    // fprintf(stderr, "curl_easy_perform() failed:
    // %s\n",curl_easy_strerror(res));
    remove(filepath); // Rimuovi il file parziale in caso di errore
    return 0;         // Fallimento
  } else {
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code >= 400) {
      //  fprintf(stderr, "Errore HTTP %ld ricevuto dal server.\n", http_code);
      return 0;
      // Qui puoi decidere di considerare il download fallito
    } else {
      // printf("Successo! Codice HTTP %ld.\n", http_code);

      return 1;
    }
  }
}

int main(void) {

  char *cache_dir = getenv("alfred_workflow_cache");
  if (cache_dir == NULL) {
    cache_dir = "/tmp/alfred";
  }
  const char *home_dir = getenv("HOME");
  if (home_dir == NULL) {
    perror("Can't find HOME env var.\n");
    return 1;
  }

  const char *sub_path =
      "/Library/Application Support/Google/Chrome/Local State";

  size_t full_path_len = strlen(home_dir) + strlen(sub_path);

  char *full_path = malloc(full_path_len);

  strcpy(full_path, home_dir);
  strcat(full_path, sub_path);

  // printf("%s\n", full_path);

  FILE *file = fopen(full_path, "r");
  if (file == NULL) {
    perror("Error opening Chrome Local State file.\n");
    return 1;
  }

  fseek(file, 0, SEEK_END);
  long filelength = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *buffer = (char *)malloc(filelength + 1);
  fread(buffer, 1, filelength, file);
  fclose(file);
  buffer[filelength] = '\0';
  // printf("%s\n", buffer);

  cJSON *json = cJSON_Parse(buffer);
  if (json == NULL) {
    free(buffer);
    return 0;
  }

  const cJSON *profile = cJSON_GetObjectItemCaseSensitive(json, "profile");
  const cJSON *profil = cJSON_GetObjectItemCaseSensitive(profile, "info_cache");
  const cJSON *item = NULL;
  cJSON *formatted_results = cJSON_CreateArray();

  cJSON_ArrayForEach(item, profil) {
    char filename[PATH_MAX_LEN] = "icon.png";
    const char *subtitle = "⏎ to open in this profile";
    // gestione immagine download
    cJSON *pic_url_item =
        cJSON_GetObjectItem(item, "last_downloaded_gaia_picture_url_with_size");
    if (cJSON_IsString(pic_url_item) && pic_url_item->valuestring != NULL &&
        strlen(pic_url_item->valuestring) > 0) {
      const char *url_string = pic_url_item->valuestring;
      char *source = item->string;
      char *pic_filename = malloc(strlen(source) + 1);
      strcpy(pic_filename, source);
      pic_filename = remove_space(pic_filename);
      char filename_base[300];
      snprintf(filename_base, sizeof(filename_base), "%s.png", pic_filename);

      char full_filepath[PATH_MAX_LEN];
      snprintf(full_filepath, sizeof(full_filepath), "%s/%s", cache_dir,
               filename_base);

      // Se il file non esiste, scaricalo
      if (access(full_filepath, F_OK) != 0) {
        // printf("Download di %s in %s\n", url_string, full_filepath);
        if (download_file(url_string, full_filepath)) {
          strncpy(filename, full_filepath, sizeof(filename) - 1);
        }
        // Altrimenti, 'filename' rimane 'icon.png' come fallback
      } else {
        strncpy(filename, full_filepath, sizeof(filename) - 1);
      }
    }

    cJSON *result = cJSON_CreateObject();
    cJSON *name_item = cJSON_GetObjectItemCaseSensitive(item, "name");
    if (cJSON_IsString(name_item)) {
      cJSON_AddStringToObject(result, "title", name_item->valuestring);
    }
    cJSON_AddStringToObject(result, "subtitle", subtitle);
    cJSON_AddStringToObject(result, "arg", item->string);
    cJSON_AddStringToObject(result, "uid", item->string);
    cJSON *icon = cJSON_CreateObject();
    cJSON_AddStringToObject(icon, "path", filename);
    cJSON_AddItemToObject(result, "icon", icon);
    cJSON_AddItemToArray(formatted_results, result);
  }
  cJSON *output_root = cJSON_CreateObject();
  cJSON_AddItemToObject(output_root, "items", formatted_results);
  char *output_string = cJSON_Print(output_root);

  cJSON_Delete(json);
  cJSON_Delete(output_root);
  free(buffer);

  printf("%s\n", output_string);
  return 0;
}
