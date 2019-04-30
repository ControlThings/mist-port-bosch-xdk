

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

/* Un-define the macro 'send' defined in the SimpleLink-provided socket.h */
#undef send

#include "wish_connection.h"
#include "wish_relay_client.h"

#include "inet_aton.h"

#include "port_relay_client.h"



void socket_set_nonblocking(int sockfd);

/* Function used by Wish to send data over the Relay control connection
 * */
int relay_send(int relay_sockfd, unsigned char* buffer, int len) {
	int n = sl_Send(relay_sockfd, buffer, len, 0);
    printf("Wrote %i bytes to relay\n", n);
    if (n < 0) {
        perror("ERROR writing to relay");
    }
    return 0;
}

/* FIXME move the sockfd inside the relay context, so that we can
 * support many relay server connections! */
int relay_sockfd;

int port_dns_start_resolving_relay_client(wish_relay_client_t *rc, char *qname) {


}

static wish_ip_addr_t port_relay_ip;

wish_ip_addr_t* port_get_relay_ip(void) {
	return &port_relay_ip;
}

void port_relay_client_open(wish_relay_client_t* relay, wish_ip_addr_t *relay_ip) {
    relay->curr_state = WISH_RELAY_CLIENT_CONNECTING;
    struct sockaddr_in relay_serv_addr;

	printf("Open relay connection\n");
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
		relay_ip->addr[0], relay_ip->addr[1],
		relay_ip->addr[2], relay_ip->addr[3]);

	printf("Relay server ip is %s port %d\n", ip_str, relay->port);
	inet_aton(ip_str, &relay_serv_addr.sin_addr);
	relay_serv_addr.sin_port = htons(relay->port);
	int connect_ret = connect(relay->sockfd, (struct sockaddr *) &relay_serv_addr,
			sizeof(relay_serv_addr));
	if (connect_ret == SL_EALREADY) {
		printf("Started connecting to relay server\n");
		relay->send = relay_send;
		memcpy(&port_relay_ip, relay_ip, sizeof(wish_ip_addr_t));
	}
	else {
		perror("relay server connect()");
		relay->curr_state = WISH_RELAY_CLIENT_WAIT_RECONNECT;
	}

}

void wish_relay_client_open(wish_core_t* core, wish_relay_client_t* relay, uint8_t uid[WISH_ID_LEN]) {
    /* FIXME this has to be split into port-specific and generic
     * components. For example, setting up the RB, next state, expect
     * byte, copying of id is generic to all ports */
    relay->curr_state = WISH_RELAY_CLIENT_OPEN;
    ring_buffer_init(&(relay->rx_ringbuf), relay->rx_ringbuf_storage, 
        RELAY_CLIENT_RX_RB_LEN);
    memcpy(relay->uid, uid, WISH_ID_LEN);

    wish_ip_addr_t relay_ip;
    if (wish_parse_transport_ip(relay->host, 0, &relay_ip) == RET_FAIL) {
        /* The relay's host was not an IP address. DNS Resolve first. */
        relay->curr_state = WISH_RELAY_CLIENT_RESOLVING;

        port_dns_start_resolving_relay_client(relay, relay->host);

	}
	else {
		port_relay_client_open(relay, &relay_ip);
	}
#if 0
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
		relay->send = relay_send;
	}
	else {
		perror("relay server connect()");
		relay->curr_state = WISH_RELAY_CLIENT_WAIT_RECONNECT;
	}
#endif
}

void wish_relay_client_close(wish_core_t* core, wish_relay_client_t *relay) {
    close(relay->sockfd);
    relay_ctrl_disconnect_cb(core, relay);
}


