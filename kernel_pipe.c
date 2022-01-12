
#include "tinyos.h"
#include "kernel_pipe.h"


#include "kernel_streams.h"
#include "kernel_cc.h"






int pipe_write(void* pipe_cb, const char* buffer, uint n)
{
	Pipe_CB* pipCB = (Pipe_CB*)pipe_cb;

	/* Make necessary checks before continuing*/
	if(pipCB==NULL)
		return -1;
	if(n < 1)
		return -1;
	if(buffer==NULL)
		return -1;
	if(pipCB->writer==NULL)
		return -1;
	if(pipCB->reader==NULL)
		return -1;


	/* Good to go*/

	


 	/* Writing.. */
	preempt_off;

	uint count=0;
		// count==0   n==6000 --- PIPE_BUFFER_SIZE == 4000
	while(count < n && pipCB->reader!=NULL)
	{

		
		pipCB->BUFFER[pipCB->w_position%PIPE_BUFFER_SIZE] = buffer[count];

		pipCB->w_position++;
		count++;

		if(pipCB->w_position-pipCB->r_position == n%PIPE_BUFFER_SIZE)
			kernel_broadcast(&pipCB->has_data);


		while(pipCB->w_position - pipCB->r_position == n%PIPE_BUFFER_SIZE && pipCB->reader!=NULL && count!=n)
		{
			kernel_wait(&pipCB->has_space, SCHED_PIPE);
		}

		if(pipCB->reader==NULL)
			{
				preempt_on;
				return -1;
			}
		

	

		
		

	}

	preempt_on;
	/* End of writing.. */




	return count;
}

int pipe_read(void* pipe_cb, char* buffer, uint n)
{
	Pipe_CB* pipCB = (Pipe_CB*)pipe_cb;

	/* Make necessary checks before continuing*/
	if(pipCB==NULL)
		return -1;
	if(n < 1 )
		return -1;
	if(buffer==NULL)
		return -1;
	if(pipCB->reader==NULL)
		return -1;


	/* Good to go*/


	// Disable preemption
	preempt_off;

	uint count=0;

	
	while(count<n)
	{
		
		
		if(pipCB->w_position==pipCB->r_position && pipCB->writer!=NULL)
			kernel_broadcast(&pipCB->has_space);

		while(pipCB->w_position==pipCB->r_position && pipCB->writer!=NULL && count!=n)
		{
			kernel_wait(&pipCB->has_data, SCHED_PIPE);
		}

		if(pipCB->writer==NULL && pipCB->r_position==pipCB->w_position)
		{
			// Read what leftover data is left.
			//return count;
			while(pipCB->r_position!=pipCB->w_position)
			{
				buffer[count] = pipCB->BUFFER[pipCB->r_position++];
				count++;

			}
			return count;
		}

		
		
		buffer[count] = pipCB->BUFFER[pipCB->r_position%PIPE_BUFFER_SIZE];

		pipCB->r_position++;
		

		
		


		count++;

	}

	preempt_on;


	return count;
}

int pipe_writer_close(void* pipe_cb)
{
	Pipe_CB* pipCB = (Pipe_CB*)pipe_cb;
	
	if(pipCB==NULL)
		return -1;
	/* Its already closed. */
	if(pipCB->writer==NULL)
		return -1;

	pipCB->writer=NULL;


	if(pipCB->reader==NULL)
		free(pipCB);
	else
		kernel_broadcast(&pipCB->has_data);



	return 0;
}

int pipe_reader_close(void* pipe_cb)
{
	Pipe_CB* pipCB = (Pipe_CB*)pipe_cb;
	
	if(pipCB==NULL)
		return -1;
	/* Its already closed. */
	if(pipCB->reader==NULL)
		return -1;

	pipCB->reader=NULL;

	if(pipCB->writer==NULL)
		free(pipCB);
	else
		kernel_broadcast(&pipCB->has_space);

	

	return 0;
}


/*
 *	Dummy function that will simply return -1 indicating that it should not have been called.
 */
int foo_func()
{
	/* Trap function, Should not be here! */
	return -1;
}

/*
 * Stream implementation methods for the Fid that is meant to write.
 *
 * @note Writer is not meant to read, therefore the Read method is a foo dummy method.
 * 
 * @note file Opening has already taken place.
 */
file_ops pipe_writer_fops = {
	
	.Open = NULL,
	.Read = foo_func,
	.Write = pipe_write,
	.Close = pipe_writer_close
};

/*
 * Stream implementation methods for the Fid that is meant to read.
 *
 * @note Reader is not meant to write, therefore the Write method is a foo dummy method.
 * 
 * @note file Opening has already taken place.
 *
 */
file_ops pipe_reader_fops = {

	.Open = NULL,
	.Read = pipe_read,
	.Write = foo_func,
	.Close = pipe_reader_close
};

/* System call to assemble a new Pipe. Pipe Control Block*/
int sys_Pipe(pipe_t* pipe)
{
	/* A Pipe requires two 'file descriptors'. One to the read end and one to the write end.*/
	Fid_t pipe_Fid[2];
	/* Each of those file descriptors is associated to a specific FCB (it is going to be reserved).*/
	FCB* pipe_fcb[2];

	/* Make the reservation! If it does not succeed return error -1. */
	if(FCB_reserve(2, pipe_Fid, pipe_fcb)==0)
		return -1;


	/* Setup of the pipe argument - Initialization. @c pipe_t* Structure keeps track of two file ids. Read and Write ends of the pipe*/
	pipe->read = pipe_Fid[0];
	pipe->write = pipe_Fid[1];




	/* Give birth to the Pipe. */
	Pipe_CB* pipe_control = xmalloc(sizeof(Pipe_CB));


	/* Initialization tactics. */
	/*--------------------------*/
	pipe_control->reader = pipe_fcb[0];
	pipe_control->writer = pipe_fcb[1];

	pipe_control->w_position=0;
	pipe_control->r_position=0;

	pipe_control-> has_data = COND_INIT;
	pipe_control-> has_space = COND_INIT;
	/*--------------------------*/






	/* Establish the Pipe_Control_Block as the Streaming Object of the the files reserved(before) */
	pipe_fcb[0]->streamobj = pipe_control;
	pipe_fcb[1]->streamobj = pipe_control;

	/* Set stream_func Stream Functions referred to each file accordingly. Distinguish Reader from Writer. Associate the first FCB as a read and the second FCB as a Writer.
	 * file_ops set of functions will be linked to.
	 */
	pipe_fcb[0]->streamfunc = &pipe_reader_fops;
	pipe_fcb[1]->streamfunc = &pipe_writer_fops;




	return 0;
}


