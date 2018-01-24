/*
 * A Service IPC layer implementation for Bosch XDK, based on the one found on the ESP8266 port.
 * 
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "wish_core.h"
#include "wish_app.h"
#include "app_service_ipc.h"
#include "core_service_ipc.h"
#include "bson_visit.h"
#include "wish_debug.h"
#include "wish_dispatcher.h"
#include "wish_port_config.h"
#include "port_net.h"

#include "utlist.h"

static wish_core_t* core;

enum ipc_event_type { EVENT_UNKNOWN, EVENT_APP_TO_CORE, EVENT_CORE_TO_APP };

struct ipc_event {
    enum ipc_event_type type;
    wish_app_t *app;
    uint8_t *data;
    size_t len;
    struct ipc_event *next;
};

struct ipc_event *ipc_event_queue = NULL;

void port_service_ipc_task(void) {
    
    /* Take first eleent in queue*/
    struct ipc_event *event = ipc_event_queue;
   
    if (event == NULL) {
        //PORT_PRINTF("Unexpected: Event queue is empty!");
        return;
    }
    
    switch (event->type) {
        case EVENT_APP_TO_CORE:
            /* Feed the message to core */
            receive_app_to_core(core, event->app->wsid, event->data, event->len);
            break;
        case EVENT_CORE_TO_APP: {
            
            receive_core_to_app(event->app, event->data, event->len);
           
            break;
        }
        default:
            printf("Bad ipc event!");
    }
    
    LL_DELETE(ipc_event_queue, event);
    free(event->data);
    free(event);
    
    struct ipc_event *elt;
    int queue_len = 0;
    LL_COUNT(ipc_event_queue, elt, queue_len);
    
    if ( queue_len > 0) {
        /* Continue processing the event queue */
        //system_os_post(SERVICE_IPC_TASK_ID, 0, 0);
    }
    else {
        printf("IPC event queue is empty!\n  ");
    }
}

void core_service_ipc_init(wish_core_t* wish_core) {
    core = wish_core;
}

void send_app_to_core(uint8_t *wsid, const uint8_t *data, size_t len) {
    /* Handle the following situations:
     *      -login message 
     *      -normal situation */
    
    
    
    struct ipc_event *event = malloc(sizeof(struct ipc_event));
    if (event == NULL) {
        printf("Could not allocate ipc event!\n");
        return;
    }
    else {
        event->data = malloc(len);
        if (event->data == NULL) {
            printf("Could not allocate ipc event data buffer: type %d\n", EVENT_APP_TO_CORE);
            return;
        }
        memcpy(event->data, data, len);
        event->len = len;
        event->app = wish_app_find_by_wsid(wsid);
        event->type = EVENT_APP_TO_CORE;
        LL_APPEND(ipc_event_queue, event);
    }
}


void receive_app_to_core(wish_core_t* core, const uint8_t wsid[WISH_ID_LEN], const uint8_t *data, size_t len) {
    wish_core_handle_app_to_core(core, wsid, data, len);
}

void receive_core_to_app(wish_app_t *app, const uint8_t *data, size_t len) {
    /* Parse the message:
     * -peer
     * -frame
     * -ready signal
     *
     * Then feed it to the proper wish_app_* callback depending on type
     */
    wish_app_determine_handler(app, (uint8_t*) data, len);
}


void send_core_to_app(wish_core_t* core, const uint8_t wsid[WISH_ID_LEN], const uint8_t *data, size_t len) {
    struct ipc_event *event = malloc(sizeof(struct ipc_event));
    if (event == NULL) {
        printf("Could not allocate ipc event!\n");
        return;
    }
    else {
        event->data = malloc(len);
        if (event->data == NULL) {
            printf("Could not allocate ipc event data buffer: type %d\n", EVENT_CORE_TO_APP);
            return;
        }
        memcpy(event->data, data, len);
        event->len = len;
        event->app = wish_app_find_by_wsid((uint8_t*) wsid);
        event->type = EVENT_CORE_TO_APP;
        LL_APPEND(ipc_event_queue, event);
    }
}
