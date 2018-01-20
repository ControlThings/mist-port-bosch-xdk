/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

#include <netdb.h> 
#include <time.h>
#include <fcntl.h>
#include <errno.h>


#include "wish_connection.h"
#include "wish_connection_mgr.h"
#include "wish_debug.h"
#include "wish_local_discovery.h"
#include "wish_identity.h"

#include "tcpip_adapter.h"
#include "esp_wifi.h"
#include "esp_system.h"

#include "port_net.h"

wish_core_t core_inst;

wish_core_t* core = &core_inst;

wish_core_t* port_net_get_core(void) {
    return core;
}


#define LOCAL_DISCOVERY_UDP_PORT 9090

/** Set this, if you want the unit to send Wish local discovery messages as unicasts to each associated station, instead of sending broadcasts. 
 This might be necessary to improve compatibility with some Android phones, which exhibit high broadcast packetloss.
 */
#define WLD_SEND_UNICASTS_IN_AP_MODE

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

void socket_set_nonblocking(int sockfd) {
    int status = fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);

    if (status == -1){
        perror("When setting socket to non-blocking mode");
        exit(1);
    }
}


/* When the wish connection "i" is connecting and connect succeeds
 * (socket becomes writable) this function is called */
void connected_cb(wish_connection_t *ctx) {
    //printf("Signaling wish session connected \n");
    wish_core_signal_tcp_event(ctx->core, ctx, TCP_CONNECTED);
}

void connected_cb_relay(wish_connection_t *ctx) {
    //printf("Signaling relayed wish session connected \n");
    wish_core_signal_tcp_event(ctx->core, ctx, TCP_RELAY_SESSION_CONNECTED);
}

void connect_fail_cb(wish_connection_t *ctx) {
    printf("Connect fail... \n");
    if (ctx->send_arg != NULL) {
        int sockfd = *((int *)ctx->send_arg);
        close(sockfd);
        free(ctx->send_arg);
    }
    wish_core_signal_tcp_event(ctx->core, ctx, TCP_DISCONNECTED);
}

int wish_open_connection(wish_core_t* core, wish_connection_t *ctx, wish_ip_addr_t *ip, uint16_t port, bool relaying) {
    ctx->core = core;
    //printf("should start connect\n");
    int *sockfd_ptr = malloc(sizeof(int));
    if (sockfd_ptr == NULL) {
        printf("Malloc fail");
        exit(1);
    }
    *(sockfd_ptr) = socket(AF_INET, SOCK_STREAM, 0);
    

    
    int sockfd = *(sockfd_ptr);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        return 1;
    }
    
    socket_set_nonblocking(sockfd);

    wish_core_register_send(ctx->core, ctx, write_to_socket, sockfd_ptr);

    //printf("Opening connection sockfd %i\n", sockfd);


    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    char ip_str[20];
    snprintf(ip_str, 20, "%d.%d.%d.%d", ip->addr[0], ip->addr[1], ip->addr[2], ip->addr[3]);
    WISHDEBUG(LOG_CRITICAL, "Remote ip is %s port %hu\n", ip_str, port);
    inet_aton(ip_str, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(port);
    int ret = connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr));
    if (ret == -1) {
        if (errno == EINPROGRESS) {
            WISHDEBUG(LOG_DEBUG, "Connect now in progress");
            ctx->curr_transport_state = TRANSPORT_STATE_CONNECTING;
        }
        else {
            perror("Unhandled connect() errno");
        }
    }
    else if (ret == 0) {
        printf("Cool, connect succeeds immediately!\n");
        if (ctx->via_relay) {
            connected_cb_relay(ctx);
        }
        else {
            connected_cb(ctx);
        }
    }
    return 0;
}

void wish_close_connection(wish_core_t* core, wish_connection_t *ctx) {
    /* Note that because we don't get a callback invocation when closing
     * succeeds, we need to excplicitly call TCP_DISCONNECTED so that
     * clean-up will happen */
    ctx->context_state = WISH_CONTEXT_CLOSING;
    int sockfd = *((int *)ctx->send_arg);
    if (sockfd >= 0) {
        close(sockfd);
    }
    free(ctx->send_arg);
    wish_core_signal_tcp_event(ctx->core, ctx, TCP_DISCONNECTED);
}

/* The fd for the socket that will be used for accepting incoming
 * Wish connections */
static int serverfd = 0;

int get_server_fd(void) {
    return serverfd;
}


/* This functions sets things up so that we can accept incoming Wish connections
 * (in "server mode" so to speak)
 * After this, we can start select()ing on the serverfd, and we should
 * detect readable condition immediately when a TCP client connects.
 * */
void setup_wish_server(wish_core_t* core) {
    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0) {
        perror("server socket creation");
        exit(1);
    }
    int option = 1;
    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    socket_set_nonblocking(serverfd);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof (server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(wish_get_host_port(core));
    if (bind(serverfd, (struct sockaddr *) &server_addr, 
            sizeof(server_addr)) < 0) {
        perror("ERROR on binding wish server socket");
        exit(1);
    }
    int connection_backlog = 1;
    if (listen(serverfd, connection_backlog) < 0) {
        perror("listen()");
    }
}
    
/* The UDP Wish local discovery socket */
static int wld_fd = 0;
static struct sockaddr_in sockaddr_wld;
/* The broadcast socket */
int wld_bcast_sock;

int get_wld_fd(void) {
    return wld_fd;
}

/* This function sets up a UDP socket for listening to UDP local
 * discovery broadcasts */
void setup_wish_local_discovery(void) {
    wld_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (wld_fd == -1) {
        WISHDEBUG(LOG_CRITICAL, "error: udp socket");
    }

#if 1
    /* Set socketoption REUSEADDR on the UDP local discovery socket so
     * that we can have several programs listening on the one and same
     * local discovery port 9090 */
    int option = 1;
    setsockopt(wld_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
#endif

    socket_set_nonblocking(wld_fd);

    memset((char *) &sockaddr_wld, 0, sizeof(struct sockaddr_in));
    sockaddr_wld.sin_family = AF_INET;
    sockaddr_wld.sin_port = htons(LOCAL_DISCOVERY_UDP_PORT);
    sockaddr_wld.sin_addr.s_addr = INADDR_ANY;

    if (bind(wld_fd, (struct sockaddr*) &sockaddr_wld, 
            sizeof(struct sockaddr_in))==-1) {
        WISHDEBUG(LOG_CRITICAL, "error: local discovery bind()");
    }

    /* Setup wld broadcasting socket for sending out adverts */
    wld_bcast_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (wld_bcast_sock < 0) {
        perror("Could not create socket for broadcasting");
        exit(1);
    }

#ifdef WLD_SEND_UNICASTS_IN_AP_MODE
    /* Set socket's broadcast enabled bit on only if we are in STA mode */
    wifi_mode_t current_mode = WIFI_MODE_NULL;
    ESP_ERROR_CHECK( esp_wifi_get_mode(&current_mode) );
    /* Set socket's broadcast bit on only if we are in STA mode */
    if (current_mode == WIFI_MODE_STA) { 
        int broadcast = 1;
        if (setsockopt(wld_bcast_sock, SOL_SOCKET, SO_BROADCAST, 
                &broadcast, sizeof(broadcast))) {
            error("set sock opt");
        }
    }
#else 
    /* Set broadcast enabled bit ON */
    int broadcast = 1;
    if (setsockopt(wld_bcast_sock, SOL_SOCKET, SO_BROADCAST, 
            &broadcast, sizeof(broadcast))) {
        error("set sock opt");
    }
#endif


    struct sockaddr_in sockaddr_src;
    memset(&sockaddr_src, 0, sizeof (struct sockaddr_in));
    sockaddr_src.sin_family = AF_INET;
    sockaddr_src.sin_port = 0;
    if (bind(wld_bcast_sock, (struct sockaddr *)&sockaddr_src, sizeof(struct sockaddr_in)) != 0) {
        error("Send local discovery: bind()");
    }



}

/* This function reads data from the local discovery socket. This
 * function should be called when select() indicates that the local
 * discovery socket has data available */
void read_wish_local_discovery(void) {
    const int buf_len = 1024;
    uint8_t buf[buf_len];
    int blen;
    socklen_t slen = sizeof(struct sockaddr_in);

    blen = recvfrom(wld_fd, buf, sizeof(buf), 0, (struct sockaddr*) &sockaddr_wld, &slen);
    if (blen == -1) {
      error("recvfrom()");
    }

    if (blen > 0) {
        //printf("Received from %s:%hu\n\n",inet_ntoa(sockaddr_wld.sin_addr), ntohs(sockaddr_wld.sin_port));
        union ip {
           uint32_t as_long;
           uint8_t as_bytes[4];
        } ip;
        /* XXX Don't convert to host byte order here. Wish ip addresses
         * have network byte order */
        //ip.as_long = ntohl(sockaddr_wld.sin_addr.s_addr);
        ip.as_long = sockaddr_wld.sin_addr.s_addr;
        wish_ip_addr_t ip_addr;
        memcpy(&ip_addr.addr, ip.as_bytes, 4);
        //printf("UDP data from: %i, %i, %i, %i\n", ip_addr.addr[0],
        //    ip_addr.addr[1], ip_addr.addr[2], ip_addr.addr[3]);

        wish_ldiscover_feed(core, &ip_addr, 
           ntohs(sockaddr_wld.sin_port), buf, blen);
    }
}

void cleanup_local_discovery(void) {
    close(wld_fd);

    close(wld_bcast_sock);
}

int wish_send_advertizement(wish_core_t* core, uint8_t *ad_msg, size_t ad_len) {
    struct sockaddr_in si_other;
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(LOCAL_DISCOVERY_UDP_PORT);
    
#ifdef WLD_SEND_UNICASTS_IN_AP_MODE
    /* Get the list of each associated APs, and at each iteration send wld message as unicast to that particular station. This improves reliability if you have stations that exhibit
     * high packet loss (such as Samsung J5) */
    wifi_mode_t current_mode = WIFI_MODE_NULL;
    ESP_ERROR_CHECK( esp_wifi_get_mode(&current_mode) );
    if (current_mode == WIFI_MODE_AP) {      
        wifi_sta_list_t ap_sta_list;
        esp_wifi_ap_get_sta_list(&ap_sta_list);
        tcpip_adapter_sta_list_t tcpip_sta_list;
        tcpip_adapter_get_sta_list(&ap_sta_list, &tcpip_sta_list);
        
        if (tcpip_sta_list.num > 0) {
            static int i = 0;
            ip4_addr_t sta_ip = tcpip_sta_list.sta[i].ip;
            printf("Unicasting instead to %s, %i\n", ip4addr_ntoa(&sta_ip), i);
            inet_aton(ip4addr_ntoa(&sta_ip), &si_other.sin_addr);
            i++;
            if (i >= tcpip_sta_list.num) {
                i = 0;
            }
        }
        else {
            inet_aton("255.255.255.255", &si_other.sin_addr);
        } 
    }
    else {
        /* Fail safe: there are no associated stations, but broadcast something anyway */
        inet_aton("255.255.255.255", &si_other.sin_addr);
    }
#else
    /* Form broadcast address */
    inet_aton("255.255.255.255", &si_other.sin_addr);
#endif
    socklen_t addrlen = sizeof(struct sockaddr_in);
    
    if (sendto(wld_bcast_sock, ad_msg, ad_len, 0, 
            (struct sockaddr*) &si_other, addrlen) == -1) {
        if (errno == ENETUNREACH || errno == ENETDOWN) {
            printf("wld: Network currently unreachable, or down. Retrying later.\n");
        }
        else if (errno == EHOSTUNREACH) {
            printf("wld: sendto returned EHOSTUNREACH\n");
        } else {
            char buffer[80] = { 0 };
            snprintf(buffer, 80, "sendto() errno = %i", errno);
            error(buffer);
        }
    }

    return 0;
}



/**
 * Get the local host IP addr formatted as a C string. The retuned
 * address should be the one which is the subnet having the host's
 * default route
 *
 * @param addr_str the pointer where the address should be stored
 * @param addr_str_len the maximum allowed length of the address
 * @return Returns value 0 if all went well.
 */
int wish_get_host_ip_str(wish_core_t* core, char* addr_str, size_t addr_str_len) {
    tcpip_adapter_ip_info_t ip_info;
    tcpip_adapter_if_t tcpip_if = TCPIP_ADAPTER_IF_STA;
    
    wifi_mode_t mode;
    ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));
    switch (mode) {
        case WIFI_MODE_STA:
            tcpip_if = TCPIP_ADAPTER_IF_STA;
            break;
        case WIFI_MODE_AP:
            tcpip_if = TCPIP_ADAPTER_IF_AP;
            break;
        case WIFI_MODE_APSTA:
            WISHDEBUG(LOG_CRITICAL, "wifi mode APSTA not handled");
            break;
        case WIFI_MODE_NULL:
            WISHDEBUG(LOG_CRITICAL, "wifi is not enabled.");
            break;
        default:
            WISHDEBUG(LOG_CRITICAL, "wifi get mode fail");
            break;
    }
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(tcpip_if, &ip_info));
    
    snprintf(addr_str, addr_str_len, "%i.%i.%i.%i", ip4_addr1(&ip_info.ip), ip4_addr2(&ip_info.ip), ip4_addr3(&ip_info.ip), ip4_addr4(&ip_info.ip) );
    
    return 0;
}

/** Get the local TCP port where the Wish core accepts incoming connections 
 * @return the local TCP server port
 */
int wish_get_host_port(wish_core_t* core) {
    return core->wish_server_port;
}

void wish_set_host_port(wish_core_t* core, uint16_t port) {
    core->wish_server_port = port;
}

int write_to_socket(void *sockfd_ptr, unsigned char* buffer, int len) {
    int retval = 0;
    int sockfd = *((int *) sockfd_ptr);
    int n = write(sockfd,buffer,len);
    
    if (n < 0) {
         printf("ERROR writing to socket: %s", strerror(errno));
         retval = 1;
    }

    return retval;
}

