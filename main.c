#include "cjson/cJSON.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {

  char *envvar = getenv("alfred_workflow_cache");

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
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      fprintf(stderr, "Error in parsing file: %s\n", error_ptr);
    }
    free(buffer);
  }

  const cJSON *profile = cJSON_GetObjectItemCaseSensitive(json, "profile");
  const cJSON *profil = cJSON_GetObjectItemCaseSensitive(profile, "info_cache");
  const cJSON *item = NULL;
  cJSON *formatted_results = cJSON_CreateArray();

  cJSON_ArrayForEach(item, profil) {
    const char *filename = "icon.png";
    const char *subtitle = "⏎ to open in this profile";

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
