/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "wish_fs.h"
#include "wish_platform.h"
#include "wish_identity.h"
#include "wish_debug.h"
#include "wish_core.h"
#include "bson_visit.h"
#include "spiffs_integration.h"



#include "port_platform.h"

static long esp_random_wrapper(void) {
    //return (long) esp_random();
	return (long) 4; //chosen by a fair roll of dice
}

void port_platform_deps(void) {
    wish_platform_set_malloc(malloc);
    wish_platform_set_realloc(realloc);
    wish_platform_set_free(free);
    wish_platform_set_rng(esp_random_wrapper);
    wish_platform_set_vprintf(vprintf);
    wish_platform_set_vsprintf(vsprintf);

    wish_fs_set_open(my_fs_open);
    wish_fs_set_read(my_fs_read);
    wish_fs_set_write(my_fs_write);
    wish_fs_set_lseek(my_fs_lseek);
    wish_fs_set_close(my_fs_close);
    wish_fs_set_rename(my_fs_rename);
    wish_fs_set_remove(my_fs_remove);
}


void port_platform_load_identities(wish_core_t *core) {
    
    wish_core_update_identities(core);
    if (core->num_ids == 0) {
        printf("Creating new identity.\n");
        /* Create new identity */
        wish_identity_t id;
        wish_create_local_identity(core, &id, "Bosch XDK");
        wish_save_identity_entry(&id);
        wish_core_update_identities(core);
        if (core->num_ids == 1) {
            printf("Created new identity, we are all set.\n");
        }
        core->config_skip_connection_acl = true;
    }
    else if (core->num_ids == 1) {
        core->config_skip_connection_acl = true;
    }
}

void _gettimeofday(void) {

}
