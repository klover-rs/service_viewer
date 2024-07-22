#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#define DESTINATION "org.freedesktop.systemd1"


bool isServiceRunning(const char* service_name) {

    sd_bus *bus = NULL;
    sd_bus_message *msg = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int ret;
    bool is_running = false;

    ret = sd_bus_open_system(&bus);
    if (ret < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-ret));
        return false;
    }

    ret = sd_bus_call_method(
        bus,
        "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager",
        "GetUnit",
        &error,
        &msg,
        "s",
        service_name
    );

    if (ret < 0) {
        fprintf(stderr, "failed to connect to system bus: %s\n", strerror(-ret));
        sd_bus_unref(bus);
        return false;
    }

    const char *unit_path;
    ret = sd_bus_message_read(msg, "o", &unit_path);
    if (ret < 0) {
        fprintf(stderr, "Failed to read unit object path: %s\n", strerror(-ret));
        sd_bus_message_unref(msg);
        sd_bus_unref(bus);
        return false;
    }

    sd_bus_message *status_msg = NULL;
    ret = sd_bus_call_method(
        bus,
        "org.freedesktop.systemd1",
        unit_path,
        "org.freedesktop.DBus.Properties",
        "Get",
        &error,
        &status_msg,
        "ss",
        "org.freedesktop.systemd1.Unit",
        "ActiveState"
    );


    if (ret < 0) {
        fprintf(stderr, "Failed to get ActiveState property: %s\n", strerror(-ret));
        sd_bus_error_free(&error);
        sd_bus_message_unref(msg);
        sd_bus_unref(bus);
        return false;
    }

    const char *state;
    ret = sd_bus_message_read(status_msg, "v", "s", &state);
    if (ret < 0) {
        fprintf(stderr, "failed to read ActiveState: %s\n", strerror(-ret));
        sd_bus_message_unref(status_msg);
        sd_bus_message_unref(msg);
        sd_bus_unref(bus);
        return false;
    }

    if (strcmp(state, "active") == 0) {
        is_running = true;
    }

    sd_bus_message_unref(status_msg);
    sd_bus_message_unref(msg);
    sd_bus_unref(bus);

    return is_running;
}

bool doesServiceExist(const char* service_name) {

    sd_bus *bus = NULL;
    sd_bus_message *msg = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int ret;
    bool exists = false;

    ret = sd_bus_open_system(&bus);
    if (ret < 0) {
        fprintf(stderr, "failed to connect to system bus: %s\n", strerror(-ret));
        return false;
    }

    ret = sd_bus_call_method(
        bus,
        DESTINATION,
        "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager",
        "GetUnit",
        &error,
        &msg,
        "s",
        service_name
    );

    if (ret < 0) {
        fprintf(stderr, "Failed to call method: %s\n", strerror(-ret));
        sd_bus_unref(bus);
        return false;
    }

    if (msg) {
        exists = true;
        sd_bus_message_unref(msg);
    } else {
        exists = false;
    }

    sd_bus_unref(bus);
    return exists;
}

char** serviceNamesArray(size_t* count) {

    sd_bus *bus = NULL;
    sd_bus_message *reply = NULL;
    int r;

    r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "failed to connect to bus: %s\n", strerror(-r));
        sd_bus_unref(bus);
        return NULL;
    }

    r = sd_bus_call_method(
        bus, 
        "org.freedesktop.systemd1",     
        "/org/freedesktop/systemd1",    
        "org.freedesktop.systemd1.Manager", 
        "ListUnits",                     
        NULL,                            
        &reply,
        NULL
    );

    if (r < 0) {
        fprintf(stderr, "Failed to call ListUnits method: %s\n", strerror(-r));
        sd_bus_unref(bus);
        return NULL;
    }

    r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(ssssssouso)");
    if (r < 0) {
        fprintf(stderr, "Failed to enter container: %s\n", strerror(-r));
        sd_bus_message_unref(reply);
        sd_bus_unref(bus);
        return NULL;
    }

    const char *name;
    const char *description;
    const char *load_state;
    const char *active_state;
    const char *sub_state;
    const char *following;
    uint32_t unit_id;
    const char *object_path;
    const char *job_type;
    const char *job_path;

    size_t capacity = 10;
    char** array = (char**)malloc(capacity * sizeof(char*));
    if (array == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        sd_bus_message_exit_container(reply);
        sd_bus_message_unref(reply);
        sd_bus_unref(bus);
        return NULL;
    }

    *count = 0;
    
    while ((r = sd_bus_message_read(reply, "(ssssssouso)", &name, &description, &load_state, &active_state, &sub_state, &following, &unit_id, &object_path, &job_type, &job_path)) > 0) {
        if (strstr(name, ".service") != NULL) {
            if (*count >= capacity) {
                capacity *= 2;
                char **temp = (char**)realloc(array, capacity * sizeof(char*));
                if (temp == NULL) {
                    fprintf(stderr, "Failed to reallocate memory for service names\n");
                    for (size_t i = 0; i < *count; i++) {
                        free(array[i]);
                    }
                    free(array);
                    sd_bus_message_exit_container(reply);
                    sd_bus_message_unref(reply);
                    sd_bus_unref(bus);
                    return NULL;
                }
                array = temp;
            }
            array[*count] = strdup(name);
            if (array[*count] == NULL) {
                fprintf(stderr, "Failed to deuplicate service name \n");
                for (size_t i = 0; i < *count; i++) {
                    free(array[i]);
                }
                free(array);
                sd_bus_message_exit_container(reply);
                sd_bus_message_unref(reply);
                sd_bus_unref(bus);
                return NULL;
            }
            (*count)++;
        }
    }

    if (r < 0) {
        fprintf(stderr, "Failed to read message: %s\n", strerror(-r));
        for (size_t i = 0; i < *count; i++) {
            free(array[i]);
        }
        free(array);
        sd_bus_message_exit_container(reply);
        sd_bus_message_unref(reply);
        sd_bus_unref(bus);
        return NULL;
    }

    sd_bus_message_exit_container(reply);
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);

    return array;
}

void freeServiceNameArray(char** array, size_t count) {
    if (array) {
        for (size_t i = 0; i < count; i++) {
            free(array[i]);
        }
        free(array);
    }
}



#define COMMAND_BUFFER_SIZE 256

int execute_command(const char *command, char *output, size_t output_size) {
    FILE *fp;
    char buffer[COMMAND_BUFFER_SIZE];

    fp = popen(command, "r");
    if (fp == NULL) {
        perror("popen");
        return -1;
    }

    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strncpy(output, buffer, output_size - 1);
        output[output_size - 1] = '\0';
    } else {
        output[0] = '\0';
    }

    if (pclose(fp) == -1) {
        perror("pclose");
        return -1;
    }

    return 0;
}


#define MAX_LINE_LENGTH 256

typedef struct {
    char type[1024];
    char exec_start[1024];
    char description[4192];
    char user[MAX_LINE_LENGTH];
} ServiceFileData;

void parse_service_file(const char* file_path, ServiceFileData *data) {
    FILE *file;
    char line[MAX_LINE_LENGTH];

    memset(data, 0, sizeof(ServiceFileData));

    file = fopen(file_path, "r");
    if (file == NULL) {
        return;
    } 

    while (fgets(line, sizeof(line), file)) {
        char *key;
        char *value;

        line[strcspn(line, "\r\n")] = 0;

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        key = strtok(line, "=");
        value = strtok(NULL, "=");

        if (value) {
            if (strcmp(key, "Type") == 0) {
                strncpy(data->type, value, sizeof(data->type) - 1);
            } else if (strcmp(key, "ExecStart") == 0) {
                strncpy(data->exec_start, value, sizeof(data->exec_start) - 1);
            } else if (strcmp(key, "Description") == 0) {
                strncpy(data->description, value, sizeof(data->description) - 1);
            } else if (strcmp(key, "User") == 0) {
                strncpy(data->user, value, sizeof(data->user) - 1);
            }
        }
    }
}

void remove_extension(char *str) {
    char *last_dot = strrchr(str, '.');
    if (last_dot != NULL) {
        *last_dot = '\0';
    }
}

typedef struct {
    char service_name[256];
    char service_display_name[256];
    char executable_path[1024];
    char description[4192];
    char service_type[1024];
    char service_account[256];
} ServiceDetails;

void getServiceDetails(const char* service_name, ServiceDetails* details) {
    memset(details, 0, sizeof(ServiceDetails));
    snprintf(details->service_name, sizeof(details->service_name), "%s", service_name);

    char* service_name_copy = strdup(service_name);
    if (service_name_copy == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }
    
    remove_extension(service_name_copy);
    snprintf(details->service_display_name, sizeof(details->service_display_name), "%s", service_name_copy);

    free(service_name_copy);

    char command[COMMAND_BUFFER_SIZE];
    char output[COMMAND_BUFFER_SIZE];
    char *path_start;
    snprintf(command, sizeof(command), "systemctl show -p FragmentPath %s", service_name);

    if (execute_command(command, output, sizeof(output)) != 0) {
        fprintf(stderr, "Failed to execute command.\n");
        return;
    }

    if ((path_start = strstr(output, "FragmentPath=")) != NULL) {
        path_start += strlen("FragmentPath=");
        path_start[strcspn(path_start, "\r\n")] = '\0';

        ServiceFileData data;

        parse_service_file(path_start, &data);

        if (strcmp(data.type, "") == 0) {
            snprintf(details->service_type, sizeof(details->service_type), "Not specified");
        } else {
            snprintf(details->service_type, sizeof(details->service_type), "%s", data.type);
        }

        if (strcmp(data.description, "") == 0) {
            snprintf(details->description, sizeof(details->description), "Not specified");
        } else {
            snprintf(details->description, sizeof(details->description), "%s", data.description);
        }

        if (strcmp(data.exec_start, "") == 0) {
            snprintf(details->executable_path, sizeof(details->executable_path), "Not specified");
        } else {
            snprintf(details->executable_path, sizeof(details->executable_path), "%s", data.exec_start);
        }

        if (strcmp(data.user, "") == 0) {
            snprintf(details->service_account, sizeof(details->service_account), "Not specified");
        } else {
            snprintf(details->service_account, sizeof(details->service_account), "%s", data.user);
        }
    }

    
}

