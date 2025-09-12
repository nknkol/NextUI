#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>
#include <msettings.h>
#include <pthread.h>
#include "bt_volume_sync.h"

// BlueZ 音量范围是 0-127
#define BLUEZ_VOLUME_MAX 127

// 全局变量
static GDBusConnection *connection = NULL;
static gchar *active_transport_path = NULL;
static guint prop_changed_watch_id = 0;
static guint object_manager_watch_id = 0;
static GMainLoop *loop = NULL;
static volatile gboolean is_syncing_from_bt = FALSE;

// 将系统音量 (0-20) 转换为 BlueZ 音量 (0-127)
static uint16_t msettings_to_bluez_volume(int vol) {
    if (vol <= VOLUME_MIN) return 0;
    if (vol >= VOLUME_MAX) return BLUEZ_VOLUME_MAX;
    return (uint16_t)(((double)vol / VOLUME_MAX) * BLUEZ_VOLUME_MAX);
}

// 将 BlueZ 音量 (0-127) 转换为系统音量 (0-20)
static int bluez_to_msettings_volume(uint16_t vol) {
    if (vol == 0) return VOLUME_MIN;
    if (vol >= BLUEZ_VOLUME_MAX) return VOLUME_MAX;
    return (int)((((double)vol / BLUEZ_VOLUME_MAX) * VOLUME_MAX) + 0.5);
}

void sync_volume_to_bt(int msettings_volume) {
    if (!active_transport_path || is_syncing_from_bt) {
        return;
    }
    uint16_t bluez_volume = msettings_to_bluez_volume(msettings_volume);
    g_dbus_connection_call(connection,
                           "org.bluez",
                           active_transport_path,
                           "org.freedesktop.DBus.Properties",
                           "Set",
                           g_variant_new("(ssv)", "org.bluez.MediaTransport1", "Volume", g_variant_new_uint16(bluez_volume)),
                           NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
    g_print("SYNC: System volume %d -> BT volume %u\n", msettings_volume, bluez_volume);
}

static void on_media_properties_changed(GDBusConnection *conn,
                                        const gchar *sender_name, const gchar *object_path,
                                        const gchar *interface_name, const gchar *signal_name,
                                        GVariant *parameters, gpointer user_data) {
    if (g_strcmp0(object_path, active_transport_path) != 0) return;

    GVariantIter *properties;
    const gchar *key;
    GVariant *value;

    g_variant_get(parameters, "(sa{sv}as)", NULL, &properties, NULL);
    while (g_variant_iter_next(properties, "{sv}", &key, &value)) {
        if (g_strcmp0(key, "Volume") == 0) {
            uint16_t bluez_volume = g_variant_get_uint16(value);
            int new_msettings_volume = bluez_to_msettings_volume(bluez_volume);
            
            if (GetVolume() != new_msettings_volume) {
                is_syncing_from_bt = TRUE;
                SetVolume(new_msettings_volume);
                g_print("SYNC: BT volume %u -> System volume %d\n", bluez_volume, new_msettings_volume);
                is_syncing_from_bt = FALSE;
            }
        }
        g_variant_unref(value);
    }
    g_variant_iter_free(properties);
}

static void setup_existing_transport(const gchar *path) {
    if (active_transport_path) g_free(active_transport_path);
    active_transport_path = g_strdup(path);

    prop_changed_watch_id = g_dbus_connection_signal_subscribe(connection,
                                                              "org.bluez",
                                                              "org.freedesktop.DBus.Properties",
                                                              "PropertiesChanged",
                                                              path,
                                                              "org.bluez.MediaTransport1",
                                                              G_DBUS_SIGNAL_FLAGS_NONE,
                                                              on_media_properties_changed,
                                                              NULL, NULL);
    g_print("Watching MediaTransport1: %s\n", path);
    sync_volume_to_bt(GetVolume());
}

static void on_object_manager_signal(GDBusConnection *conn,
                                     const gchar *sender_name, const gchar *object_path,
                                     const gchar *interface_name, const gchar *signal_name,
                                     GVariant *parameters, gpointer user_data) {
    if (g_strcmp0(signal_name, "InterfacesAdded") == 0) {
        const gchar *added_path;
        GVariantIter *interfaces;
        g_variant_get(parameters, "(&oa{sa{sv}})", &added_path, &interfaces);
        
        const gchar *iface_name;
        GVariant *props;
        while (g_variant_iter_next(interfaces, "{sa{sv}}", &iface_name, &props)) {
            if (g_strcmp0(iface_name, "org.bluez.MediaTransport1") == 0) {
                 if (active_transport_path && prop_changed_watch_id > 0) {
                     g_dbus_connection_signal_unsubscribe(connection, prop_changed_watch_id);
                 }
                 setup_existing_transport(added_path);
            }
            g_variant_unref(props);
        }
        g_variant_iter_free(interfaces);
    } else if (g_strcmp0(signal_name, "InterfacesRemoved") == 0) {
        const gchar *removed_path;
        GVariantIter *interfaces;
        g_variant_get(parameters, "(&oas)", &removed_path, &interfaces);

        const gchar *iface_name;
        while (g_variant_iter_next(interfaces, "s", &iface_name)) {
            if (g_strcmp0(iface_name, "org.bluez.MediaTransport1") == 0) {
                if (active_transport_path && g_strcmp0(removed_path, active_transport_path) == 0) {
                    g_print("MediaTransport1 removed: %s\n", active_transport_path);
                    g_dbus_connection_signal_unsubscribe(connection, prop_changed_watch_id);
                    prop_changed_watch_id = 0;
                    g_free(active_transport_path);
                    active_transport_path = NULL;
                }
            }
        }
        g_variant_iter_free(interfaces);
    }
}

static void* bt_volume_thread_func(void *arg) {
    GError *error = NULL;
    loop = g_main_loop_new(NULL, FALSE);
    connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!connection) {
        g_printerr("Failed to connect to D-Bus: %s\n", error->message);
        g_error_free(error);
        return NULL;
    }

    object_manager_watch_id = g_dbus_connection_signal_subscribe(connection,
                                      "org.bluez", "org.freedesktop.DBus.ObjectManager",
                                      NULL, "/", NULL, G_DBUS_SIGNAL_FLAGS_NONE,
                                      on_object_manager_signal, NULL, NULL);

    GVariant *result = g_dbus_connection_call_sync(connection, "org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects", NULL, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    if (result) {
        GVariantIter *objects;
        g_variant_get(result, "(a{oa{sa{sv}}})", &objects);
        
        const gchar *path;
        GVariant *interfaces;
        while(g_variant_iter_next(objects, "{oa{sa{sv}}}", &path, &interfaces)) {
            if (g_variant_lookup_value(interfaces, "org.bluez.MediaTransport1", NULL)) {
                setup_existing_transport(path);
            }
            g_variant_unref(interfaces);
        }
        g_variant_iter_free(objects);
        g_variant_unref(result);
    } else if (error) {
        g_printerr("Failed to get managed objects: %s\n", error->message);
        g_error_free(error);
    }
    
    g_main_loop_run(loop);
    return NULL;
}

static pthread_t bt_volume_tid;

void start_bt_volume_sync_thread(void) {
    if (pthread_create(&bt_volume_tid, NULL, &bt_volume_thread_func, NULL) != 0) {
        perror("pthread_create for bt_volume_sync failed");
    }
}

void stop_bt_volume_sync_thread(void) {
    if (loop) {
        g_main_loop_quit(loop);
    }
    pthread_join(bt_volume_tid, NULL);
    if (prop_changed_watch_id > 0) g_dbus_connection_signal_unsubscribe(connection, prop_changed_watch_id);
    if (object_manager_watch_id > 0) g_dbus_connection_signal_unsubscribe(connection, object_manager_watch_id);
    if (connection) g_object_unref(connection);
}