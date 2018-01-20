/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   port_platform.h
 * Author: jan
 *
 * Created on January 6, 2017, 1:00 PM
 */

#ifndef PORT_PLATFORM_H
#define PORT_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif
    
    

    void port_platform_deps(void);
    void port_platform_load_identities(wish_core_t* core);
    wish_uid_list_elem_t* port_platform_get_uid_list(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_PLATFORM_H */

