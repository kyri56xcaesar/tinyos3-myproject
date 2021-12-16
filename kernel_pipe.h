#define __KERNEL_PIPE_H



#include "kernel_streams.h"

/* BUFFER SIZE OF THE PIPE*/
#define PIPE_BUFFER_SIZE 4000




/*
 *	Pipe implementation. 
 *	A structure that hold the information about a pipe. Control block of a pipe.
 *	
 *	A pipe refers to two seperate File Control Blocks. A Reader and a Writer and links them.
 *	Condition Variable mechanicm is used in order to control any synchronization problems that might occur.
 *
 *	Integer variables @c w_position and @c r_position are accountable for keeping track of where reading and writing is happening 
 *	every moment at the Buffer.
 */

typedef struct pipe_control_block {

	FCB *reader, *writer;


	CondVar has_space;

	CondVar has_data;


	int w_position, r_position;

	char BUFFER[PIPE_BUFFER_SIZE];

} Pipe_CB;

int pipe_write(void* pipe_cb, const char* buffer, uint n);
int pipe_read(void* pipe_cb, char* buffer, uint n);
int pipe_reader_close(void* pipe_cb);
int pipe_writer_close(void* pipe_cb);




