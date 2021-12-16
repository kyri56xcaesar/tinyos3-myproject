#define __KERNEL_SOCKET_H


#include "tinyos.h"
#include "util.h"
#include "kernel_streams.h"
#include "kernel_pipe.h"

typedef enum {
	SOCKET_LISTENER,
	SOCKET_UNBOUND,
	SOCKET_PEER
} socket_type;

typedef struct Socket_Control_Block SCB; 

typedef struct Listener_Socket {
	
	rlnode queue;
	CondVar req_available;

} listener_socket;



typedef struct Unbound_Socket {

	rlnode unbound_socket;

} unbound_socket;



typedef struct Peer_Socket {

	SCB* peer;
	Pipe_CB* write_pipe;
	Pipe_CB* read_pipe;

} peer_socket;



typedef struct Socket_Control_Block {

	uint refcount;
	FCB* fcb;

	socket_type type;

	port_t port;

	union {
		listener_socket listener_s;
		unbound_socket unbound_s;
		peer_socket peer_s;
	};

} SCB;




