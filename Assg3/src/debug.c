#include <debug.h>
#include <context.h>
#include <entry.h>
#include <lib.h>
#include <memory.h>


/*****************************HELPERS******************************************/

/*
 * allocate the struct which contains information about debugger
 *
 */
struct debug_info *alloc_debug_info()
{
	struct debug_info *info = (struct debug_info *) os_alloc(sizeof(struct debug_info));
	if(info)
		bzero((char *)info, sizeof(struct debug_info));
	return info;
}
/*
 * allocate the struct which contains information about struct fun_on_stack
 *
 */
struct fun_on_stack *alloc_fos_info()
{
	struct fun_on_stack *info = (struct fun_on_stack *) os_alloc(sizeof(struct fun_on_stack));
	if(info)
		bzero((char *)info, sizeof(struct fun_on_stack));
	return info;
}
/*
 * frees a debug_info struct
 */
void free_debug_info(struct debug_info *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct debug_info));
}



/*
 * allocates a page to store registers structure
 */
struct registers *alloc_regs()
{
	struct registers *info = (struct registers*) os_alloc(sizeof(struct registers));
	if(info)
		bzero((char *)info, sizeof(struct registers));
	return info;
}

/*
 * frees an allocated registers struct
 */
void free_regs(struct registers *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct registers));
}

/*
 * allocate a node for breakpoint list
 * which contains information about breakpoint
 */
struct breakpoint_info *alloc_breakpoint_info()
{
	struct breakpoint_info *info = (struct breakpoint_info *)os_alloc(
		sizeof(struct breakpoint_info));
	if(info)
		bzero((char *)info, sizeof(struct breakpoint_info));
	return info;
}

/*
 * frees a node of breakpoint list
 */
void free_breakpoint_info(struct breakpoint_info *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct breakpoint_info));
}

/*
 * Fork handler.
 * The child context doesnt need the debug info
 * Set it to NULL
 * The child must go to sleep( ie move to WAIT state)
 * It will be made ready when the debugger calls wait
 */
void debugger_on_fork(struct exec_context *child_ctx)
{
	// printk("DEBUGGER FORK HANDLER CALLED\n");
	child_ctx->dbg = NULL;
	child_ctx->state = WAITING;
	// putting info of the child in the parent's dbg
	u32 parent_pid = child_ctx->ppid;
	struct exec_context *parent_ctx = get_ctx_by_pid(parent_pid);
	parent_ctx->dbg->cpid = child_ctx->pid;
}


/******************************************************************************/


/* This is the int 0x3 handler
 * Hit from the childs context
 */

long int3_handler(struct exec_context *ctx)
{
	

	//Your code

	// Begin
   
	// Determining from where has the INT3 hit
	// probing the instruction pointer...the instruction pointer is now at the second instruction 
	// of the breakpointed function...the first instruction ka address is the function's address

	// ! note: for this to succeed make sure that the first instruction of the do_end_handler raises the INT3 handler
	u64 addr = ctx->regs.entry_rip - 1; // need to check if this address is of the do_end_handler function
	u32 pid_debugger = ctx->ppid;
	struct exec_context * debugger_ctx = get_ctx_by_pid(pid_debugger);



	u64 do_end_handler_addr = (u64)debugger_ctx->dbg->end_handler;// // TODO: find the address of the do_end_handler function via the debugger
	// !debug
	//printk("int 3 called\n");
	//printk("Address in rsp should be of a line in main %d\n", ctx->regs.entry_rsp);
	//printk("%d\n",ctx->regs.entry_rsp>>32);
	if(addr!=do_end_handler_addr){
		// Dealing with the first INT3 hit, idhar hi second call ka kaam nipta diya
	//	// TODO: check here if the end_breakpoint_enable is activated, if so, then time to change the return value of the function:
		// checking if the end_breakpoint_enable is activated
		// first finding which function has triggered INT3
	
		struct breakpoint_info * curr = debugger_ctx->dbg->head;
		while(curr!=NULL) {
				//! debug
		//printk("inside addr!=doendhandler, inside curr!=null, address of breakpointed function is %d\n",addr);
			if(curr->addr == addr) {
				//found
				if(curr->end_breakpoint_enable) {
					// !debug
					// add the function in the stack list
					struct fun_on_stack * next_fun_on_stack = debugger_ctx->dbg->next_fun;
					while(next_fun_on_stack->next!=NULL) {
						next_fun_on_stack = next_fun_on_stack->next;
					}
					struct fun_on_stack * temp = alloc_fos_info();
					if(!temp) return -1;
					temp->addr = addr;
					// !debug
					//printk("temp->addr is %d\n", temp->addr);
					temp->next = NULL;
					temp->original_return_addr = *((u64 *)ctx->regs.entry_rsp); // !saving the original return address for restoring later smh
					next_fun_on_stack->next = temp; 

					//printk("Checkpoint %x\n",*((u64 *)ctx->regs.entry_rsp) );
					*((u64 *)ctx->regs.entry_rsp) = do_end_handler_addr;		
					//printk("Checkpoint %x\n",*((u64 *)ctx->regs.entry_rsp) );
					curr->is_on_stack = 1; // for stack related work
				}
				break;
			}
			curr = curr->next;
		}
		
		ctx->regs.entry_rsp-=8;
		*((u64 *)(ctx->regs.entry_rsp)) = ctx->regs.rbp; // compensating for the loss of push rbp instruction because of the 0xCC

	}
	else {
		// The second INT3 handler
		// determining from which function do_handler has been called...
		// end_breakpoint_enabled is definitely enabled, hence this function is the latest function in the stack
		// ! debug
		//printk("second int3  handler check\n");
		struct fun_on_stack * next_fun_on_stack = debugger_ctx->dbg->next_fun;
		while(next_fun_on_stack->next->next!=NULL) {
			next_fun_on_stack = next_fun_on_stack->next;
		}
		// removing the function from the list and adding the original return address onto the stack
		ctx->regs.entry_rsp -=8;
		*((u64 *)(ctx->regs.entry_rsp)) = next_fun_on_stack->next->original_return_addr; //  this will make the function return to the point where it would have returned, had not it been breakpointed
		ctx->regs.entry_rsp -=8;
		*((u64 *)(ctx->regs.entry_rsp)) = ctx->regs.rbp ;
		next_fun_on_stack->next = NULL;


	}

	// for backtracing: need to do this
	u64* bt = debugger_ctx->dbg->bt;
	for(int i = 0;i<MAX_BACKTRACE;i++) {
		bt[i] = -1;
	}
	// making dummy registers for manipulation
	struct registers * regs = alloc_regs();
	if(!regs) return -1;
	regs->rbp = (ctx->regs.entry_rsp);
	int i = 0;
	// probing the rbp registers, based on whether INT3 has been called from first instruction or do_end_handler
	struct fun_on_stack * next_fun_on_stack = debugger_ctx->dbg->next_fun->next;
	int total = 0;
	while(next_fun_on_stack!=NULL) {
			next_fun_on_stack = next_fun_on_stack->next;
			total++;
	}
	
	if(addr!=do_end_handler_addr) {
		// called from first instruction, function to be included

	// ! debug
	//printk("Checkpoint\n");
		//printk("rbp+8 mein value %x\n", *((u64 *)(regs->rbp+8)));
		while(addr!=END_ADDR) {
			//printk("addr is %x\n", addr);
			bt[i] = addr;
			addr = *((u64 *)(regs->rbp+8));
			if(addr==do_end_handler_addr) {
				next_fun_on_stack = debugger_ctx->dbg->next_fun->next;
				int i = 1;
				while(i!=total) {
					next_fun_on_stack = next_fun_on_stack->next;
					i++;
				}
				total--;
				addr = next_fun_on_stack->original_return_addr;
			}
			regs->rbp= *(u64 *)(regs->rbp);
			i++;
		}
	} 
	else {
		// called from do_end_handler
		//printk("check\n");
		//regs->rbp = ctx->regs.rbp;
		addr = *((u64 *)(regs->rbp+8));
		while(addr!=END_ADDR){
			//printk("addr is %x\n", addr);
			bt[i] = addr;
			regs->rbp = *(u64 *)(regs->rbp);
			addr = *((u64 *)(regs->rbp+8));
			if(addr==do_end_handler_addr) {
				next_fun_on_stack = debugger_ctx->dbg->next_fun->next;
				int i = 1;
				while(i!=total) {
					next_fun_on_stack = next_fun_on_stack->next;
					i++;
				}
				total--;
				addr = next_fun_on_stack->original_return_addr;
			}			
			
			i++;
		}
	}
	// Your code
	free_regs(regs);
	//backtracing procedure over
	ctx->state = WAITING; // setting state of the debugee as waiting
	debugger_ctx->state = READY; // setting state of debugger as ready
	debugger_ctx->regs.rax = addr; // address of either f()/do_end_handler() passed in rax
	schedule(debugger_ctx); 
	return 0;
}

/*
 * Exit handler.
 * Deallocate the debug_info struct if its a debugger.
 * Wake up the debugger if its a child
 */
void debugger_on_exit(struct exec_context *ctx)
{
	// Your code
	if(ctx->dbg) {
		// not null, hence this is a debugger...deallocating debug_info
		free_debug_info(ctx->dbg);
		ctx->state = EXITING;
	}
	else {
		// child(debugee)
		u32 ppid = ctx->ppid;
		struct exec_context * debugger_ctx = get_ctx_by_pid(ppid);
		debugger_ctx->regs.rax = CHILD_EXIT;
		ctx->state = EXITING;
		debugger_ctx->state = READY;
		// //! ye kayde se hoga yaha: schedule(debugger_ctx);
	}
}


/*
 * called from debuggers context
 * initializes debugger state
 */
int do_become_debugger(struct exec_context *ctx, void *addr)
{
	// Your code
	
	// Begin 

	// addr calling INT3
	*((int*)addr) = (*((int*)addr) & 0xFFFFFF00) | 0xCC;

	// debug_info struct of the parent being initialised

	// ! debug
	//printk("do_become_debugger working 1\n");	
	struct debug_info * dbg = alloc_debug_info();
	if(!dbg) return -1;
	dbg->breakpoint_count = 0;
	dbg->end_handler = addr;
	dbg->head = NULL;
	dbg->breakpoint_mastercount = 0;
	dbg->cpid = 0;


	struct fun_on_stack * dummy = alloc_fos_info();
	if(!dummy) return -1;
	dummy->next = NULL;
	dummy->addr = 0;
	
	dummy->original_return_addr = 0;
	dbg->next_fun = dummy;

	// bt array:
	for(int i = 0; i<MAX_BACKTRACE;i++) {
		dbg->bt[i] = -1;
	}
	// Now associate the above initialised struct with the parent's context
	ctx->dbg = dbg;
	return 0;
}

/*
 * called from debuggers context
 */
int do_set_breakpoint(struct exec_context *ctx, void *addr, int flag)
{

	// ! debug
	//printk("set breakpoint called\n");
	// Your code

	// Begin
	
	// // TODO: the flag related work..passing the control to the do_end_handler function
	// // TODO: have to manipulate debuggee’s address space so that whenever the instruction at addr is executed, INT3 would be triggered. 
	// Attempting to set a breakpoint on debuggee process at address addr
    // Firstly, checking through all breakpoint_info structs to see if the addr is already breakpointed

	struct breakpoint_info * curr = ctx->dbg->head;
	if(curr==NULL) {
		// ! debug
		// this is the first ever breakpoint to be set
		if(ctx->dbg->breakpoint_count < MAX_BREAKPOINTS) {
			// creating a new breakpoint_info struct and filling the values

			struct breakpoint_info* new_breakpoint = alloc_breakpoint_info();
			if(!new_breakpoint) return -1;
			new_breakpoint->addr = (u64)addr;
			new_breakpoint->end_breakpoint_enable = flag;
			new_breakpoint->next = NULL;
			new_breakpoint->num =  ctx->dbg->breakpoint_mastercount+1;
			// making the auxilliary changes
			ctx->dbg->breakpoint_count+=1;
			ctx->dbg->breakpoint_mastercount+=1;
			ctx->dbg->head = new_breakpoint;
			
			// now manipulating debugge's address space
			// * printk("Value before setting 0xCC is 0x%x\n",*((int*)addr) );
			*((int*)addr) = (*((int*)addr) & 0xFFFFFF00) | 0xCC; // adding the INT3 instruction, would need to appropriately handle int 3 handler
			// !debug...passed
			// printk("Checkpoint 1\n");
			return 0; 
		}
		else {
			return -1;
		}		
	}
	// breakpoint functions present:
	while(curr!=NULL) {
		// ! debug
		//printk("This should not have printed\n");
		if(curr->addr == (u64)addr) {
			// addr present, update the end_breakpoint_enable flag
			// if the function is in the call stack...need to check ki flag ka 1 se 0 nhi ho rha ho.
			// 0 se 1/0 ok
			// !debug
			struct fun_on_stack * next_fun = ctx->dbg->next_fun;
			while(next_fun->next!=NULL) {
				if(next_fun->next->addr == curr->addr) {
					if(curr->end_breakpoint_enable == 1) {
						if(flag) return 0;
						else return -1;
					}
				}
				next_fun = next_fun->next;
			}
			curr->end_breakpoint_enable = flag;
			return 0;
						
		}
		else if(curr->next==NULL) {
			// search is finished, the addr was not found
			if(ctx->dbg->breakpoint_count < MAX_BREAKPOINTS) {
				// creating a new breakpoint_info struct and filling the values
				// ! debug
				struct breakpoint_info * new_breakpoint = alloc_breakpoint_info();
				if(!new_breakpoint) return -1;
				new_breakpoint->addr = (u64)addr;
				new_breakpoint->end_breakpoint_enable = flag;
				new_breakpoint->next = NULL;
				new_breakpoint->num =  ctx->dbg->breakpoint_mastercount+1;

				// making the auxilliary changes
				ctx->dbg->breakpoint_count+=1;
				ctx->dbg->breakpoint_mastercount+=1;
				curr->next = new_breakpoint;

				// now manipulate the debugee's address space
				*((int*)addr) = (*((int*)addr) & 0xFFFFFF00) | 0xCC; // adding the INT3 instruction, would need to appropriately handle int 3 handler
				
				//! no return value added because of the todo[or use the return 0 at the end]...
				//! once done, the break may be removed.
				break; // this break is essential else the curr=curr->next later will not be NULL when required, since additional breakpoint has been added.
			}
			else {
				return -1;
			}
		}
		else {
			return -1;
		}
		
		curr = curr->next;
	} 
	return 0;
}

/*
 * called from debuggers context
 */
int do_remove_breakpoint(struct exec_context *ctx, void *addr)
{
	//Your code

	// Begin
	int found = 0;
	struct breakpoint_info * curr = ctx->dbg->head;
	if(curr==NULL) {
		// no address to remove=> error
		return -1;
	}
	struct breakpoint_info * pichla = NULL;
	while(curr!=NULL) {
		if(curr->addr ==(u64)addr) {
			//! debug
			// check if its on the call stack
			struct fun_on_stack * next_fun = ctx->dbg->next_fun;
			while(next_fun->next!=NULL) {
										// !debug...passed
				if(next_fun->next->addr == curr->addr) {
					if(curr->end_breakpoint_enable == 1) {
						// !debug...passed
						//printk("shit\n");
						return -1;
					}
				}
				next_fun = next_fun->next;
			}			
			found = 1;
			break;
		}
		pichla = curr;
		curr = curr->next;
		
	}
	if(!found) return -1;
	// modifications:
	if(pichla==NULL) {
		// basically the first function is the one which is to be removed
		// ! debug
		ctx->dbg->head = curr->next;

	}
	else {
		// ! debug
		pichla->next = curr->next;
	}
	// now remove curr, sort the int3 thing:
	*((int*)(curr->addr)) = (*((int*)(curr->addr)) & 0xFFFFFF00) | 0x55; // ! for later: update the opcodes as pushrbp_opcode and int3opcode
	free_breakpoint_info(curr);
	return 0;
}


/*
 * called from debuggers context
 */

int do_info_breakpoints(struct exec_context *ctx, struct breakpoint *ubp)
{
	
	// Your code
	struct breakpoint_info * curr = ctx->dbg->head;
	int i = 0;
	while(curr!=NULL) {
		ubp[i].addr = curr->addr;
		ubp[i].end_breakpoint_enable = curr->end_breakpoint_enable;
		ubp[i].num = curr->num;
		i++;
		curr= curr->next;
	}
	return i;
}


/*
 * called from debuggers context
 */
int do_info_registers(struct exec_context *ctx, struct registers *regs)
{
	// Your code
	u32 cpid = ctx->dbg->cpid;
	struct exec_context * child_ctx = get_ctx_by_pid(cpid);
	regs->entry_rip = child_ctx->regs.entry_rip -1;	// -1 since rip advances by one instruction
	regs->entry_rsp = child_ctx->regs.entry_rsp + 8;  // +8 since INT3 ke ekdum end mein call ho rha ye
	regs->rbp= child_ctx->regs.rbp;
	regs->rax= child_ctx->regs.rax;
	regs->rdi= child_ctx->regs.rdi;
	regs->rsi= child_ctx->regs.rsi;
	regs->rdx= child_ctx->regs.rdx;
	regs->rcx= child_ctx->regs.rcx;
	regs->r8= child_ctx->regs.r8;
	regs->r9= child_ctx->regs.r9;



	return 0;
}

/*
 * Called from debuggers context
 */
int do_backtrace(struct exec_context *ctx, u64 bt_buf)
{
	u64 * bt = (u64 *) bt_buf;
	int i ;
	for(i = 0; i<MAX_BACKTRACE;i++) {
		if(ctx->dbg->bt[i] != -1) bt[i] = ctx->dbg->bt[i];
		else break;
	}
	return i;
	
}

/*
 * When the debugger calls wait
 * it must move to WAITING state
 * and its child must move to READY state
 */

s64 do_wait_and_continue(struct exec_context *ctx)
{
	ctx->state = WAITING; // debugger to the waiting state
	u32 child_pid = ctx->dbg->cpid;
	struct exec_context * child_ctx = get_ctx_by_pid(child_pid);
	child_ctx->state = READY; // debugee ready
	ctx->regs.rax = -1; // return register setting to -1.
	schedule(child_ctx);	// scheduled the child

	// Your code
	return 0;
}

