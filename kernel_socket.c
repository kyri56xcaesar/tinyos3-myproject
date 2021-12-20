

#include "tinyos.h"
#include "util.h"
#include "kernel_streams.h"
#include "kernel_socket.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"



/* The port map table */
SCB* PORT_MAP[MAX_PORT+1];


int socket_write(void* socket_cb, const char* buffer, uint n)
{
	SCB* scb = (SCB*)socket_cb;

	if(scb==NULL)
		return -1;
	if(n<1)
		return -1;
	if(buffer==NULL)
		return -1;
	if(scb->fcb==NULL)
		return -1;
	if(scb->peer_s.write_pipe==NULL || scb->peer_s.peer->peer_s.read_pipe==NULL)
		return -1;

	int r;

	r = pipe_write(scb->peer_s.write_pipe, buffer, n);
	if(r==-1)
		return -1;

	return r;
}

int socket_read(void* socket_cb, char* buffer, uint n)
{
	SCB* scb = (SCB*)socket_cb;

	if(scb==NULL)
		return -1;
	if(n<1)
		return -1;
	if(buffer==NULL)
		return -1;
	if(scb->fcb==NULL)
		return -1;
	if(scb->peer_s.read_pipe==NULL)
		return -1;

	int r;

	r = pipe_read(scb->peer_s.read_pipe, buffer, n);
	if(r ==-1)
		return -1;

	return r;
}

int socket_close(void* socket_cb)
{
	SCB* scb = (SCB*)socket_cb;
	if(scb==NULL)
		return -1;

	if(scb->type == SOCKET_PEER)
	{
		pipe_writer_close(scb->peer_s.write_pipe);
		pipe_reader_close(scb->peer_s.read_pipe);
	}

	if(scb->type == SOCKET_LISTENER)
	{


		while(!is_rlist_empty(&scb->listener_s.queue))
		{
			rlnode* node = rlist_pop_front(&scb->listener_s.queue);
			free(node->cr);
		}


		PORT_MAP[scb->port]=NULL;
		kernel_broadcast(&scb->listener_s.req_available);

	}


	if(scb->refcount<=0)
		free(scb);

	return 0;
}


file_ops socket_file_ops = {

	.Open = NULL,
	.Read = socket_read,
	.Write = socket_write,
	.Close = socket_close
};




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
	if(scb->type==SOCKET_LISTENER || scb->type==SOCKET_PEER)
		return -1;

	scb->type=SOCKET_LISTENER;
	rlnode_init(&scb->listener_s.queue, NULL); 
	scb->listener_s.req_available=COND_INIT;

	PORT_MAP[scb->port] = scb;

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

	if(lsock<0 || lsock>MAX_FILEID)
		return -1;

	if(CURPROC->FIDT[lsock]==NULL || CURPROC->FIDT[lsock]->streamobj==NULL)
		return -1;

	SCB* lscb = CURPROC->FIDT[lsock]->streamobj;

	if(PORT_MAP[lscb->port]==NULL)
		return -1;

	if(lscb==NULL || lscb->type != SOCKET_LISTENER)
		return -1;

	for(int i=0; i< MAX_FILEID; i++)
	{
		if(CURPROC->FIDT[i]==NULL)
			break;
		if(i==MAX_FILEID-1 && CURPROC->FIDT[i]!=NULL)
			return -1;
	}

	lscb->refcount++;

	while(is_rlist_empty(&lscb->listener_s.queue) && PORT_MAP[lscb->port]!=NULL)
	{
		kernel_wait(&lscb->listener_s.req_available, SCHED_PIPE);
	}

	if(PORT_MAP[lscb->port]==NULL)
		return -1;



	rlnode* popped_req;
	popped_req = rlist_pop_front(&lscb->listener_s.queue);

	con_req* req;
	req = popped_req->cr;



	req->admitted=1;

	kernel_signal(&req->connected_cv);


	SCB* scb1 = req->peer;
	Fid_t fid2 = sys_Socket(lscb->port);
	SCB* scb2 = CURPROC->FIDT[fid2]->streamobj;

	scb2->refcount++;

	scb1->peer_s.peer = scb2;
	scb2->peer_s.peer = scb1;

	Pipe_CB* pipe1;
	Pipe_CB* pipe2;

	pipe1 = xmalloc(sizeof(Pipe_CB));
	pipe2 = xmalloc(sizeof(Pipe_CB));

	pipe1->reader=scb1->fcb;
	pipe1->writer=scb2->fcb;
	pipe2->reader=scb2->fcb;
	pipe2->writer=scb1->fcb;

	scb1->type = SOCKET_PEER;
	scb2->type = SOCKET_PEER;

	scb1->peer_s.write_pipe = pipe2;
	scb1->peer_s.read_pipe = pipe1;
	scb2->peer_s.write_pipe = pipe1;
	scb2->peer_s.read_pipe = pipe2;



	lscb->refcount--;
	scb2->refcount--;


	return fid2;
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
	if(sock<0 || sock>MAX_FILEID)
		return -1;

	if(PORT_MAP[port] == NULL || port==NOPORT || port<= 0 || port > MAX_PORT)
		return -1;

	SCB* scb = CURPROC->FIDT[sock]->streamobj;

	if(scb==NULL)
		return -1;

	if(scb->type != SOCKET_UNBOUND)
		return -1;

	scb->refcount++;

	con_req* req = (con_req*)xmalloc(sizeof(con_req));
	req->admitted=0;
	req->peer=scb;
	req->connected_cv=COND_INIT;

	rlnode_init(&req->queue_node, req);

	rlist_push_back(&PORT_MAP[port]->listener_s.queue, &req->queue_node);
	assert(is_rlist_empty(&PORT_MAP[port]->listener_s.queue)==0);


	kernel_signal(&PORT_MAP[port]->listener_s.req_available);

	while(req->admitted==0)
	{
		if(kernel_timedwait(&req->connected_cv, SCHED_PIPE, timeout)==0)
			return -1;
	}

	scb->refcount--;



	return 0;
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
	if(sock<0 || sock>MAX_FILEID)
		return -1;

	SCB* scb = CURPROC->FIDT[sock]->streamobj;

	if(scb->type != SOCKET_PEER)
		return -1;

	switch(how)
	{
		case SHUTDOWN_READ:
			pipe_reader_close(scb->peer_s.read_pipe);
			scb->peer_s.read_pipe=NULL;

			break;
		case SHUTDOWN_WRITE:
			pipe_writer_close(scb->peer_s.write_pipe);
			scb->peer_s.write_pipe=NULL;
			break;
		case SHUTDOWN_BOTH:
			pipe_reader_close(scb->peer_s.read_pipe);
			scb->peer_s.read_pipe=NULL;
			pipe_writer_close(scb->peer_s.write_pipe);
			scb->peer_s.write_pipe=NULL;
			scb->fcb=NULL;
			break;
		default:
			break;
	}

	return 0;
}

