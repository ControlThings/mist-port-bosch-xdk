

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include "socket.h"
#include <time.h>
#include <errno.h>



#include "wish_connection.h"
#include "wish_relay_client.h"

#include "inet_aton.h"


void socket_set_nonblocking(int sockfd);

/* Function used by Wish to send data over the Relay control connection
 * */
int relay_send(int relay_sockfd, unsigned char* buffer, int len) {
    //int n = write(relay_sockfd, buffer, len);
	int n = send(relay_sockfd, buffer, len, 0);
    printf("Wrote %i bytes to relay\n", n);
    if (n < 0) {
        perror("ERROR writing to relay");
    }
    return 0;
}

/* FIXME move the sockfd inside the relay context, so that we can
 * support many relay server connections! */
int relay_sockfd;

void wish_relay_client_open(wish_core_t* core, wish_relay_client_t *relay, 
        uint8_t relay_uid[WISH_ID_LEN]) {
    /* FIXME this has to be split into port-specific and generic
     * components. For example, setting up the RB, next state, expect
     * byte, copying of id is generic to all ports */
    relay->curr_state = WISH_RELAY_CLIENT_OPEN;
    ring_buffer_init(&(relay->rx_ringbuf), relay->rx_ringbuf_storage, 
        RELAY_CLIENT_RX_RB_LEN);
    memcpy(relay->uid, relay_uid, WISH_ID_LEN);

    /* Linux/Unix-specific from now on */ 

    struct sockaddr_in relay_serv_addr;

    printf("Open relay connection");
    relay->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    
    if (relay->sockfd < 0) {
        perror("ERROR opening socket");
        relay->curr_state = WISH_RELAY_CLIENT_WAIT_RECONNECT;
        return;
    }
    socket_set_nonblocking(relay->sockfd);

    relay_serv_addr.sin_family = AF_INET;
    char ip_str[12+3+1] = { 0 };
    sprintf(ip_str, "%i.%i.%i.%i", 
        relay->ip.addr[0], relay->ip.addr[1], 
        relay->ip.addr[2], relay->ip.addr[3]);
    
    printf("Relay server ip is %s port %d\n", ip_str, relay->port);
    inet_aton(ip_str, &relay_serv_addr.sin_addr);
    relay_serv_addr.sin_port = htons(relay->port);
    int connect_ret = connect(relay->sockfd, (struct sockaddr *) &relay_serv_addr,
            sizeof(relay_serv_addr));
	if (connect_ret == SL_EALREADY) {
		printf("Started connecting to relay server\n");
		relay->sendXXX = relay_send;
	}
	else {
		perror("relay server connect()");
		relay->curr_state = WISH_RELAY_CLIENT_WAIT_RECONNECT;
	}

}

void wish_relay_client_close(wish_core_t* core, wish_relay_client_t *relay) {
    close(relay->sockfd);
    relay_ctrl_disconnect_cb(core, core->relay_db);
}


