#ifndef WISH_PORT_CONFIG_H
#define WISH_PORT_CONFIG_H

/** Port-specific config variables */

/** This specifies the size of the receive ring buffer */
#define WISH_PORT_RX_RB_SZ 1500

/** This specifies the maximum number of simultaneous Wish connections
 * */
#define WISH_PORT_CONTEXT_POOL_SZ   4 // "There are a total of eight regular (non-secured) sockets", and we reserve one for relay, refer to ch 6.7 http://www.ti.com/lit/ug/swru368a/swru368a.pdf

/** If this is defined, include support for the App TCP server */
//#define WITH_APP_TCP_SERVER

/** This specifies the maximum number of simultaneous app requests to core */
#define WISH_PORT_APP_RPC_POOL_SZ ( 10 )

/** This specifies the maximum size of the buffer where some RPC handlers build the reply (1400) */
#define WISH_PORT_RPC_BUFFER_SZ ( 1400 )

/** This defines the maximum number of entries in the Wish local discovery table (4).
 * You should make sure that in the worst case any message will fit into WISH_PORT_RPC_BUFFFER_SZ  */
#define WISH_LOCAL_DISCOVERY_MAX ( 8 ) /* wld.list: 64 local discoveries should fit in 16k RPC buffer size */

/** This defines the maximum number of uids in database (max number of identities + contacts) (4) 
     You should make sure that in the worst case any message will fit into WISH_PORT_RPC_BUFFFER_SZ */
#define WISH_PORT_MAX_UIDS ( 8 ) /* identity.list: 128 uid entries should fit into 16k RPC buffer */

/** The app is hard-coded to the core binary */
#define WITH_APP_INTERNAL

/** If this is defined, then managing the core's identities from a
 * remote node is possible */
#define WISH_REMOTE_MANAGEMENT

#endif
