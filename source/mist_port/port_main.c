/*
 * port_main.c
 *
 *  Created on: Jan 14, 2018
 *      Author: jan
 *
 *
 * References
 * Simple link networking API: http://xdk.bosch-connectivity.com/xdk_docs/html/group__ti__networking.html
 * TI Simple link API User's guide : http://www.ti.com/lit/ug/swru368a/swru368a.pdf
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <malloc.h>

#include "socket.h" /* interface provided by TI Simplelink API */
#include "BCDS_NetworkConfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "PAL_initialize_ih.h"

#include "wish_core.h"
#include "wish_connection.h"
#include "wish_connection_mgr.h"
#include "wish_local_discovery.h"
#include "wish_identity.h"
#include "wish_event.h"
#include "wish_port_config.h"
#include "utlist.h"

#include "port_platform.h"
#include "port_net.h"
#include "spiffs_integration.h"
#include "port_service_ipc.h"
#include "port_relay_client.h"

int max_fd = 0;

static void update_max_fd(int fd) {
    if (fd >= max_fd) {
        max_fd = fd + 1;
    }
}

static bool listen_to_adverts = true;
static bool as_server = true;
static bool as_relay_client = true;

void port_main(void) {
	my_spiffs_mount();
    port_platform_deps();

    /* Start initialising Wish core */
    wish_core_t *core = port_net_get_core();


    wish_core_init(core);

    port_platform_load_identities(core);

    if (as_server) {
        setup_wish_server(core);
    }

    if (listen_to_adverts) {
        setup_wish_local_discovery();
    }

    //mist_initted = true;

    int wld_fd = get_wld_fd();
    int server_fd = get_server_fd();

    mist_config_init();

    printf("Entering main loop!\n");
    while (true) {
        fd_set rfds;
        fd_set wfds;
        struct timeval tv;

        FD_ZERO(&rfds);
        FD_ZERO(&wfds);

        if (as_server) {
            FD_SET(server_fd, &rfds);
            update_max_fd(server_fd);
        }

        if (listen_to_adverts) {
            FD_SET(wld_fd, &rfds);
            update_max_fd(wld_fd);
        }

        if (as_relay_client) {
            wish_relay_client_t* relay;

            LL_FOREACH(core->relay_db, relay) {
                if (relay->curr_state == WISH_RELAY_CLIENT_CONNECTING) {
                	if (relay->sockfd != -1) {
                		FD_SET(relay->sockfd, &wfds);
                		update_max_fd(relay->sockfd);
                	}
                }
                else if (relay->curr_state == WISH_RELAY_CLIENT_WAIT_RECONNECT) {
                    /* connect to relay server has failed or disconnected and we wait some time before retrying */
                }
                else if (relay->curr_state == WISH_RELAY_CLIENT_RESOLVING) {
                	/* Don't do anything as the resolver is resolving. relay->sockfd is not valid as it has not yet been initted! */
                }
                else if (relay->curr_state != WISH_RELAY_CLIENT_INITIAL) {
                    if (relay->sockfd != -1) {
                    	FD_SET(relay->sockfd, &rfds);
                    	update_max_fd(relay->sockfd);
                    }
                }
            }
        }

        int i = -1;
        for (i = 0; i < WISH_PORT_CONTEXT_POOL_SZ; i++) {
            wish_connection_t* ctx = &(core->connection_pool[i]);
            if (ctx->context_state == WISH_CONTEXT_FREE) {
                continue;
            }
            if (ctx->send_arg == NULL) {
                printf("context in state %i but send_arg is NULL\n", ctx->context_state);
                continue;
            }
            int sockfd = ((struct xdk_send_arg *) ctx->send_arg)->sock_fd;
            if (ctx->curr_transport_state == TRANSPORT_STATE_CONNECTING) {
                /* If the socket has currently a pending connect(), set
                 * the socket in the set of writable FDs so that we can
                 * detect when connect() is ready */
                FD_SET(sockfd, &wfds);
            }
            else {
                FD_SET(sockfd, &rfds);
            }
            update_max_fd(sockfd);
        }


        tv.tv_sec = 0;
        tv.tv_usec = 100*1000;
        int select_ret = select( max_fd, &rfds, &wfds, NULL, &tv );
        taskYIELD();

        /* Zero fds ready means we timed out */
        if ( select_ret > 0 ) {
            //printf("there is a fd ready\n");
            if (FD_ISSET(wld_fd, &rfds)) {

                read_wish_local_discovery();
            }

            if (as_relay_client) {
                wish_relay_client_t* relay;

                LL_FOREACH(core->relay_db, relay) {

                    if (relay->curr_state ==  WISH_RELAY_CLIENT_CONNECTING && relay->sockfd != -1 && FD_ISSET(relay->sockfd, &wfds)) {
                    	/* On the XDK, we are to call connect again! */
                    	wish_ip_addr_t* relay_ip = port_get_relay_ip();
                    	 printf("relay connection sockfd id writable\n");

                    	struct sockaddr_in relay_serv_addr;
                    	relay_serv_addr.sin_family = AF_INET;
                    	relay_serv_addr.sin_addr.s_addr = htons(NetworkConfig_Ipv4Value(relay_ip->addr[0], relay_ip->addr[1], relay_ip->addr[2], relay_ip->addr[3]));
                    	relay_serv_addr.sin_port = htons(relay->port);
                        int connect_error =
                        		connect(relay->sockfd, (struct sockaddr *) &relay_serv_addr, sizeof(relay_serv_addr));

                        if (connect_error >= 0) {
                            /* connect() succeeded, the connection is open */
                            //printf("Relay client connected\n");
                            relay_ctrl_connected_cb(core, relay);
                            wish_relay_client_periodic(core, relay);
                        }
                        else if (connect_error == SL_EALREADY) {
                        	//just continue
                        }
                        else {
                            /* connect fails. Note that perror() or the
                             * global errno is not valid now */
                            printf("relay control connect() failed: %i\n", connect_error);

                            // FIXME only one relay context assumed!
                            relay_ctrl_connect_fail_cb(core, relay);
                            close(relay->sockfd);
                        }
                    }

                    else if (relay->curr_state == WISH_RELAY_CLIENT_WAIT_RECONNECT) {
						/* connect to relay server has failed or disconnected and we wait some time before retrying  */
					}
					else if (relay->curr_state == WISH_RELAY_CLIENT_RESOLVING) {
						/* Don't do anything as the resolver is resolving. relay->sockfd is not valid as it has not yet been initted! */
					}
					else if (relay->curr_state != WISH_RELAY_CLIENT_INITIAL && relay->sockfd != -1 && FD_ISSET(relay->sockfd, &rfds)) {
                    	//printf("Reading is possible from relay.\n");
                        uint8_t byte;   /* That's right, we read just one
                            byte at a time! */
                        int read_len = recv(relay->sockfd, &byte, 1, 0);
                        if (read_len > 0) {
                            wish_relay_client_feed(core, relay, &byte, 1);
                            wish_relay_client_periodic(core, relay);
                        }
                        else if (read_len == 0) {
                            printf("Relay control connection disconnected\n");
                            relay_ctrl_disconnect_cb(core, relay);
                            close(relay->sockfd);
                        }
                        else {
                            perror("relay control read()");
                        }
                    }
                }
            }


            int i = 0;
             /* Check for Wish connections status changes */
            for (i = 0; i < WISH_PORT_CONTEXT_POOL_SZ; i++) {
                wish_connection_t* ctx = &(core->connection_pool[i]);
                if (ctx->context_state == WISH_CONTEXT_FREE) {
                    continue;
                }
                if ((struct xdk_send_arg *) ctx->send_arg == NULL) {
                    continue;
                }
                int sockfd = ((struct xdk_send_arg *)ctx->send_arg)->sock_fd;
                if (FD_ISSET(sockfd, &rfds)) {
                    /* The Wish connection socket is now readable. Data
                     * can be read without blocking */
                    int rb_free = wish_core_get_rx_buffer_free(core, ctx);
                    if (rb_free == 0) {
                        /* Cannot read at this time because ring buffer
                         * is full */
                        printf("ring buffer full\n");
                        continue;
                    }
                    if (rb_free < 0) {
                        printf("Error getting ring buffer free sz\n");
                        //exit(1);
                    }
                    const size_t max_read = 512; //We must limit the reading, else recv (sl_Recv) can sometimes fail and return truely funky values (> 0 though)
                    const size_t read_buf_len = rb_free > max_read?max_read:rb_free;
                    uint8_t buffer[read_buf_len];
                    taskYIELD();
                    int read_len = recv(sockfd, buffer, read_buf_len, 0);
                    if (read_len > 0) {
                        //printf("Read some data, len = %i, requested = %i, fd = %i\n", read_len, read_buf_len, sockfd);

                        wish_core_feed(core, ctx, buffer, read_len);
                        struct wish_event ev = {
                            .event_type = WISH_EVENT_NEW_DATA,
                            .context = ctx };
                        wish_message_processor_notify(&ev);
                    }
                    else if (read_len == 0) {
                        printf("Connection closed, fd = %i\n", sockfd);
                        close(sockfd);
                        free(ctx->send_arg);
                        wish_core_signal_tcp_event(core, ctx, TCP_DISCONNECTED);
                        continue;
                    }
                }
                if (FD_ISSET(sockfd, &wfds)) {
                    /* The Wish connection socket is now writable. This
                     * means that a previous connect succeeded. (because
                     * normally we don't select for socket writability!)
                     * */
                	struct sockaddr_in *remote_addr = &((struct xdk_send_arg *)ctx->send_arg)->remote_addr;
                    int connect_ret = connect(sockfd, (struct sockaddr *) remote_addr, sizeof(struct sockaddr_in));

                    if (connect_ret >= 0) {
                        /* connect() succeeded, the connection is open
                         * */
                        if (ctx->curr_transport_state
                                == TRANSPORT_STATE_CONNECTING) {
                            if (ctx->via_relay) {
                                connected_cb_relay(ctx);
                            }
                            else {
                                connected_cb(ctx);
                            }
                        }
                        else {
                            printf("There is somekind of state inconsistency\n");
                            //exit(1);
                        }
                    }
                    else {
                        /* connect fails. Note that perror() or the
                         * global errno is not valid now */
                        printf("wish connection connect() failed: %i\n",
                            connect_ret);
                        connect_fail_cb(ctx);
                        close(sockfd);
                    }
                }

            }


            /* Check for incoming Wish connections to our server */
            if (as_server) {
                if (FD_ISSET(server_fd, &rfds)) {
                    printf("Detected incoming connection!\n");
                    int newsockfd = accept(server_fd, NULL, NULL);
                    //printf("accept returns %i\n", newsockfd);
                    if (newsockfd < 0) {
                        perror("on accept");
                        //exit(1);
                    }
                    socket_set_nonblocking(newsockfd);
                    /* Start the wish core with null IDs.
                     * The actual IDs will be established during handshake
                     * */
                    uint8_t null_id[WISH_ID_LEN] = { 0 };
                    wish_connection_t *ctx = wish_connection_init(core, null_id, null_id);
                    if (ctx == NULL) {
                        /* Fail... no more contexts in our pool */
                        printf("No new Wish connections can be accepted!\n");
                        close(newsockfd);
                    }
                    else {
                        struct xdk_send_arg* send_arg = malloc(sizeof(struct xdk_send_arg));
                        send_arg->sock_fd = newsockfd;
                        /* New wish connection can be accepted */
                        wish_core_register_send(core, ctx, write_to_socket, send_arg);
                        wish_core_signal_tcp_event(core, ctx, TCP_CLIENT_CONNECTED);
                    }
                }
            }



        }
        else if (select_ret == 0) {
            //printf("select timeout\n");
        }
        else {
            printf("select error!\n");
            //exit(0);
        }


        static time_t timestamp = 0;
        int sec_cnt =  (xTaskGetTickCount() / portTICK_RATE_MS) / 1000;
        if (sec_cnt > timestamp + 10) {
            timestamp = sec_cnt;
            /* Perform periodic action 10s interval
             * Note: define INCLUDE_uxTaskGetStackHighWaterMark to 1 in FreeRTOSConfig.h
             */

            struct mallinfo minfo = mallinfo();
            printf("Stack high water mark (bytes): %i, System heap arena: %i fordblks %i uordblks %i size %i min ever %i\n",  uxTaskGetStackHighWaterMark( NULL ) * (sizeof (portSTACK_TYPE)),
            		minfo.arena, minfo.fordblks, minfo.uordblks, xPortGetFreeHeapSize(),  xPortGetMinimumEverFreeHeapSize());;

        }

        while (1) {
            /* FIXME this loop is bad! Think of something safer */
            /* Call wish core's connection handler task */
            struct wish_event *ev = wish_get_next_event();
            if (ev != NULL) {
                wish_message_processor_task(core, ev);
            }
            else {
                /* There is nothing more to do, exit the loop */
                break;
            }
        }

        static int periodic_timestamp = 0;
        if (sec_cnt > periodic_timestamp) {
            /* 1-second periodic interval */
            periodic_timestamp = sec_cnt;
            wish_time_report_periodic(core);
            //mist_config_periodic();
        }
        port_service_ipc_task();
        //process_application_events();

    }


}
