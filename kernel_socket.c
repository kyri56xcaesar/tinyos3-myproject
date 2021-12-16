

#include "tinyos.h"
#include "util.h"
#include "kernel_streams.h"
#include "kernel_socket.h"
#include "kernel_sched.h"
#include "kernel_proc.h"






int socket_write(void* socket_cb, const char* buffer, uint n)
{
	return -1;
}

int socket_read(void* socket_cb, char* buffer, uint n)
{
	return -1;
}

int socket_close(void* socket_cb)
{
	return -1;
}


file_ops socket_file_ops = {

	.Open = NULL,
	.Read = socket_read,
	.Write = socket_write,
	.Close = socket_close
};

/* The port map table */
SCB* PORT_MAP[MAX_PORT+1];


/**
	@brief Return a new socket bound on a port.

	This function returns a file descriptor for a new
	socket object.	If the @c port argument is NOPORT, then the 
	socket will not be bound to a port. Else, the socket
	will be bound to the specified port. 

	@param port the port the new socket will be bound to
	@returns a file id for the new socket, or NOFILE on error. Possible
		reasons for error:
		- the port is iilegal
		- the available file ids for the process are exhausted
*/
Fid_t sys_Socket(port_t port)
{
	
	/*
	Check port validity.
	*/
	if(port < 0 || port > MAX_PORT)
		return NOFILE;

	Fid_t socket_Fid;
	FCB* socket_fcb;

	/* Reserve one FCB for a socket.*/
	if(FCB_reserve(1, &socket_Fid, &socket_fcb)==0)
		return NOFILE; /* no fid available */

	/* Create a Socket Control Block*/
	SCB* scb = (SCB*)xmalloc(sizeof(SCB));

	/* Make connections between the socket and the matching FCB.*/
	socket_fcb->streamfunc=&socket_file_ops;
	socket_fcb->streamobj=scb;

	

	/* Initialization  of the Socket. */ 

	/*Connect to the FCB.*/
	scb->fcb = socket_fcb;
	scb->refcount=0; 

	/* Attach the new socket to the wanted port. If it is 0 then NOPORT.*/
	if(port == 0)
		scb->port = NOPORT;
	else
		scb->port = port;

	/* A new socket is to be unbound. */
	scb->type = SOCKET_UNBOUND;
	rlnode_init(&scb->unbound_s.unbound_socket, NULL); /* propably useless */



	return socket_Fid;
}

/**
	@brief Initialize a socket as a listening socket.

	A listening socket is one which can be passed as an argument to
	@c Accept. Once a socket becomes a listening socket, it is not
	possible to call any other functions on it except @c Accept, @Close
	and @c Dup2().

	The socket must be bound to a port, as a result of calling @c Socket.
	On each port there must be a unique listening socket (although any number
	of non-listening sockets are allowed).

	@param sock the socket to initialize as a listening socket
	@returns 0 on success, -1 on error. Possible reasons for error:
		- the file id is not legal
		- the socket is not bound to a port
		- the port bound to the socket is occupied by another listener
		- the socket has already been initialized
	@see Socket
 */
int sys_Listen(Fid_t sock)
{
	
	if(sock<0 || sock>MAX_FILEID)
		return -1;
	PCB* curp = CURPROC;
	assert(curp!=NULL);
	FCB* curFCB = curp->FIDT[sock];
	if(curFCB==NULL)
		return -1;
	SCB* scb = curFCB->streamobj;
	if(scb==NULL)
		return -1;
	if(scb->port==NOPORT)
		return -1;
	if(PORT_MAP[scb->port]!= NULL)
		return -1;
	if(scb->type==SOCKET_LISTENER)
		return -1;

	scb->type=SOCKET_LISTENER;
	rlnode_init(&scb->listener_s.queue, NULL); 
	scb->listener_s.req_available=COND_INIT;

	return 0;
}


/**
	@brief Wait for a connection.

	With a listening socket as its sole argument, this call will block waiting
	for a single @c Connect() request on the socket's port. 
	one which can be passed as an argument to @c Accept. 

	It is possible (and desirable) to re-use the listening socket in multiple successive
	calls to Accept. This is a typical pattern: a thread blocks at Accept in a tight
	loop, where each iteration creates new a connection, 
	and then some thread takes over the connection for communication with the client.

	@param sock the socket to initialize as a listening socket
	@returns a new socket file id on success, @c NOFILE on error. Possible reasons 
	    for error:
		- the file id is not legal
		- the file id is not initialized by @c Listen()
		- the available file ids for the process are exhausted
		- while waiting, the listening socket @c lsock was closed

	@see Connect
	@see Listen
 */
Fid_t sys_Accept(Fid_t lsock)
{
	return NOFILE;
}


/**
	@brief Create a connection to a listener at a specific port.

	Given a socket @c sock and @c port, this call will attempt to establish
	a connection to a listening socket on that port. If sucessful, the
	@c sock stream is connected to the new stream created by the listener.

	The two connected sockets communicate by virtue of two pipes of opposite directions, 
	but with one file descriptor servicing both pipes at each end.

	The connect call will block for approximately the specified amount of time.
	The resolution of this timeout is implementation specific, but should be
	in the order of 100's of msec. Therefore, a timeout of at least 500 msec is
	reasonable. If a negative timeout is given, it means, "infinite timeout".

	@params sock the socket to connect to the other end
	@params port the port on which to seek a listening socket
	@params timeout the approximate amount of time to wait for a
	        connection.
	@returns 0 on success and -1 on error. Possible reasons for error:
	   - the file id @c sock is not legal (i.e., an unconnected, non-listening socket)
	   - the given port is illegal.
	   - the port does not have a listening socket bound to it by @c Listen.
	   - the timeout has expired without a successful connection.
*/
int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	return -1;
}


/**
   @brief Shut down one direction of socket communication.

   With a socket which is connected to another socket, this call will 
   shut down one or the other direction of communication. The shut down
   of a direction has implications similar to those of a pipe's end shutdown.
   More specifically, assume that this end is socket A, connected to socket
   B at the other end. Then,

   - if `ShutDown(A, SHUTDOWN_READ)` is called, any attempt to call `Write(B,...)`
     will fail with a code of -1.
   - if ShutDown(A, SHUTDOWN_WRITE)` is called, any attempt to call `Read(B,...)`
     will first exhaust the buffered data and then will return 0.
   - if ShutDown(A, SHUTDOWN_BOTH)` is called, it is equivalent to shutting down
     both read and write.

   After shutdown of socket A, the corresponding operation `Read(A,...)` or `Write(A,...)`
   will return -1.

   Shutting down multiple times is not an error.
   
   @param sock the file ID of the socket to shut down.
   @param how the type of shutdown requested
   @returns 0 on success and -1 on error. Possible reasons for error:
       - the file id @c sock is not legal (a connected socket stream).
*/
int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	return -1;
}

