#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-journal.h>
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

bool stopService(const char *service_name) {
    sd_bus *bus = NULL;
    sd_bus_message *msg = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int ret;

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
        "StopUnit",          
        &error,
        &msg,
        "ss",
        service_name,
        "replace"
    );

    if (ret < 0) {
        fprintf(stderr, "failed to stop service: %s\n", strerror(-ret));
        if (error.message) {
            fprintf(stderr, "Error message: %s\n", error.message);
        }
        sd_bus_error_free(&error);
        sd_bus_message_unref(msg);
        sd_bus_unref(bus);
        return false;
    }

    sd_bus_error_free(&error);
    sd_bus_message_unref(msg);
    sd_bus_unref(bus);

    return true;
}
