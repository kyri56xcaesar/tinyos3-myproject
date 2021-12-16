
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_streams.h"

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
  TCB* tcb=NULL;
  //PTCB* ptcb=NULL;

  if(task==NULL)
    return NOTHREAD;

  // Create a new Thread.
  tcb=spawn_thread(CURPROC, start_main_process_thread);
  assert(tcb!=NULL);




//------------------------- INSTEAD OF SPAWN THREAD ---------------------------
  // PCB pointer to the currect PCB(owner of the fresh Thread)
  PCB* pcb = tcb->owner_pcb;

  // Create a new PTCB block.
  PTCB* ptcb;
  ptcb = (PTCB*)xmalloc(sizeof(PTCB));


  // Connect PTCB with TCB and vice versa
  tcb->ptcb=ptcb;
  ptcb->tcb=tcb;

  // Input arguments
  ptcb->task=task;
  ptcb->argl=argl;
  ptcb->args=args;


  // Initialize PTCB 
  ptcb->exited=0;
  ptcb->detached=0;
  ptcb->exit_cv=COND_INIT;
  ptcb->refcount=0;

  // Initialize Intrusive list node.
  rlnode_init(&ptcb->ptcb_list_node, ptcb);

  // Insert ptcb to the PCB list of PTCBs
  rlist_push_back(&pcb->PTCB_list, &ptcb->ptcb_list_node);
  // Increment the PCB thread counter.
  pcb->thread_count++;

//------------------------- INSTEAD OF SPAWN THREAD ---------------------------

  //ptcb=spawn_process_thread(tcb, task, argl, args);
  

  // Check if everything went smoothly.
  assert(tcb!=NULL);
  assert(ptcb!=NULL);
  assert(CURPROC==tcb->owner_pcb);
  assert(ptcb==tcb->ptcb);
  assert(ptcb->task==task);
  assert(ptcb->argl==argl);
  assert(ptcb->exited==0);
  assert(ptcb->detached==0);
  assert(ptcb->refcount==0);


  // Wake up the fresh baken Thread!
  wakeup(ptcb->tcb);


	return (Tid_t)ptcb;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{

	return (Tid_t) (cur_thread()->ptcb);
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
  //Cast Tid_t to a PTCB pointer.
  PTCB* ptcb = (PTCB*)tid;
 

  /*
   * Make necessary checks before joining.
   */

  // Cannot join a Thread that does not exist.
  if(rlist_find(&CURPROC->PTCB_list, ptcb, NULL)==NULL)
    return -1;

  // Cannot join itself.
  if(tid ==sys_ThreadSelf())
    return -1;

  // Cannot join a Thread that has been detached.
  if(ptcb->detached==1)
	   return -1;

  if(ptcb==NULL)
    return -1;


  // Join the Thread via kernel_wait func + the CondVar argument given.
  // Increment the refcount of the Thread that has been joined at in order to keep track of its waiting queue.
  ptcb->refcount++;
  while(!(ptcb->exited || ptcb->detached))
  {
    kernel_wait(&ptcb->exit_cv, SCHED_USER);
  }
  // Decrement the refcount after it is done.
  ptcb->refcount--;

 
  // Better safe than sorry!
  if(ptcb->detached==1)
  {
    return -1;    
  }
  
  // Be sure to save the exitval if there is one.
  if(exitval != NULL)
  {
    *exitval=ptcb->exitval;
  }

  // ???? refcount is 0. It can be deleted. A signal must be broadcast.
  if(ptcb->refcount==0)
  {
    rlist_remove(&ptcb->ptcb_list_node);
    free(ptcb);
  }

  return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
  //Cast Tid_t to a PTCB pointer.
  PTCB* ptcb = (PTCB*)tid;

  // Making necessary checks before continuing

  // Cannot detach a Thread that doesn't exist!
  if(rlist_find(&CURPROC->PTCB_list, ptcb, NULL)==NULL)return -1;

  // Cannot detach an exited Thread.
  if(ptcb->exited==1)
  {
    return -1;
  }

 
  // If Thread exists and is not exited, then detach it!
  
  // Raise detach flag
  ptcb->detached=1;

  // Relief any responsibilities
  ptcb->refcount=0;

  // Signal that freedom has arrived!
  kernel_broadcast(&ptcb->exit_cv);
  

  
 
  return 0;

  

  
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
  // Hold the current PCB
  PCB* curproc = CURPROC;

  // Hold the current PTCB
  PTCB* ptcb = (PTCB*)sys_ThreadSelf();

  // Set the Thread as exited. --> Raise exited flag.
  ptcb->exited=1;
  ptcb->exitval=exitval;

  // Decrement PCB's thread count.
  ptcb->tcb->owner_pcb->thread_count--;


  // Signal that this Thread has exited in order to wake up its waiting list.
  kernel_broadcast(&ptcb->exit_cv);


  // IF must be: if its the last thread alive.
  if(curproc->thread_count==0)
  {

    if(get_pid(curproc)!=1)
    {
      /* Reparent any children of the exiting process to the 
       initial task */
    PCB* initpcb = get_pcb(1);
    while(!is_rlist_empty(& curproc->children_list)) {
      rlnode* child = rlist_pop_front(& curproc->children_list);
      child->pcb->parent = initpcb;
      rlist_push_front(& initpcb->children_list, child);
    }

    /* Add exited children to the initial task's exited list 
       and signal the initial task */
    if(!is_rlist_empty(& curproc->exited_list)) {
      rlist_append(& initpcb->exited_list, &curproc->exited_list);
      kernel_broadcast(& initpcb->child_exit);
    }

    /* Put me into my parent's exited list */
    rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
    kernel_broadcast(& curproc->parent->child_exit);

    //
    }
    
    assert(is_rlist_empty(& curproc->children_list));
    assert(is_rlist_empty(& curproc->exited_list));

    /* 
    Do all the other cleanup we want here, close files etc. 
   */

    /* Release the args data */
    if(curproc->args) 
    {
      free(curproc->args);
      curproc->args = NULL;
    }

    /* Clean up FIDT */
    for(int i=0;i<MAX_FILEID;i++) {
      if(curproc->FIDT[i] != NULL) {
        FCB_decref(curproc->FIDT[i]);
        curproc->FIDT[i] = NULL;
      }
    }

    /* Disconnect my main_thread */
    //curproc->main_thread = NULL;

    /* Now, mark the process as exited. */
    //curproc->pstate = ZOMBIE;



    // CLEAN PTCBs
    while(!is_rlist_empty(&curproc->PTCB_list))
    {
      if(ptcb->refcount<1)
      {
        PTCB* rem_ptcb;
        rem_ptcb = rlist_pop_front(&curproc->PTCB_list)->ptcb;
        free(rem_ptcb);
      }
        
    }
    assert(is_rlist_empty(&curproc->PTCB_list));
  }

  /* Disconnect my main_thread */
    curproc->main_thread = NULL;

    /* Now, mark the process as exited. */
    curproc->pstate = ZOMBIE;



  // "Delete the thread"
  kernel_sleep(EXITED, SCHED_USER);


}

