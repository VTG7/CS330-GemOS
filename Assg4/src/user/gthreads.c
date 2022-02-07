#include <gthread.h>
#include <ulib.h>

static struct process_thread_info tinfo __attribute__((section(".user_data"))) = {};
/*XXX 
      Do not modifiy anything above this line. The global variable tinfo maintains user
      level accounting of threads. Refer gthread.h for definition of related structs.
 */


// Making a dummy function here, jiska address will be stored in stack pointer of the thread
// When the thread returns normally, it will reach here and we will call gthread_ecxit
static void dummy() {
	// still in thread's space
	void * retval;
	asm volatile ("mov %%rax,%0"
	: "=r"(retval));
	////printf("Check2\n");
	gthread_exit(retval);
}
/* Returns 0 on success and -1 on failure */
/* Here you can use helper system call "make_thread_ready" for your implementation */
int gthread_create(int *tid, void *(*fc)(void *), void *arg) {
        
	/* You need to fill in your implementation here*/

	//first checking if the MAX_threads limit has been achieved:
	if(tinfo.num_threads==MAX_THREADS) {	// TODO: initialise the tinfo  
		return -1;
	}
	// allocate stack for the thread:	
	void * stackp;	
	stackp = mmap(NULL, TH_STACK_SIZE, PROT_READ|PROT_WRITE, 0);
	if(!stackp || stackp == MAP_ERR){
		return -1;
	}
	// making stackp point to dummy()
	*((u64 *)(((u64)stackp) + TH_STACK_SIZE-8)) = (u64)dummy;
	////printf("Address of dummy is %x\n", dummy);
	// invoking the clone system call
	int thpid = clone(fc, ((u64)stackp) + TH_STACK_SIZE-8, arg);	// !to check fc or &fc

	// Updating thread related info
	// Begin by handling the tid values:
	for(int i = 0;i<MAX_THREADS;i++) {
		tinfo.threads[i].tid = i;
	}
	for(int i = 0;i<MAX_THREADS;i++) {
		if(tinfo.threads[i].status == TH_STATUS_UNUSED ){	// !TH_STATUS_UNUSED khud se hi kiya hai...mind therepurcussions 
			// ! debug
			//printf("In create giving out tid = %d\n", i);
			// found an unused index
			tinfo.threads[i].pid = thpid; 
			tinfo.threads[i].priv_areas;// TODO 
			//tinfo.threads[i].ret_addr; // TODO
			tinfo.threads[i].stack_addr = stackp; 
			tinfo.threads[i].status = TH_STATUS_USED; 
			tinfo.num_threads++;
			////printf("TID alloted is %d\n", tinfo.threads[i].tid);
			// update the tid parameter passed to this function:
			*tid = tinfo.threads[i].tid;
			break;
		} 
		
	}

	// readying the thread
	make_thread_ready(thpid);
	// // TODO: handling the exit routine 

	return 0;
}

int gthread_exit(void *retval) {

	/* You need to fill in your implementation here*/
	
	// !debug
	//printf("gthread_exit called\n");
	// cleanup process
	u32 pid = getpid();
	int i;
	for( i = 0;i<MAX_THREADS;i++) {
		if(tinfo.threads[i].pid == pid) {
			// got the thread
			// put the return address in ret_addr
			// ! debug
			//printf("Exiting tid %d\n", tinfo.threads[i].tid);
			tinfo.threads[i].ret_addr = retval; // ! use assembly to do that..no ye to direct ho jayega yaha

			tinfo.threads[i].status = TH_STATUS_ALIVE; //! thread exits, but is alive since join is not called yet 
			break;
		}
	}
	// TODO: unmap the private memory areas maybe..?
	////printf("Check 3\n...return value is %d", *(int *)tinfo.threads[i].ret_addr);
	//call exit
	exit(0);
}

void* gthread_join(int tid) {
        
     /* Here you can use helper system call "wait_for_thread" for your implementation */
       
     /* You need to fill in your implementation here*/

	// checking if the thread corresponding to the tid is finished
	// correlate with gthread_exit() above
	// !debug
	////printf("CP 1\n");
	void * retval = NULL;
	int i;
	int found  = 0;
	for(i = 0;i<MAX_THREADS;i++) {
		if(tinfo.threads[i].tid == tid) {

			// check if it has finished
			////printf("Check 4\n");// ...return address is %x\n", (int* )retval);
			found = 1;
			// ! debug
			//printf("In join, tid = %d\n",tid);
			while(tinfo.threads[i].status != TH_STATUS_ALIVE) {	// since gthread_exit mein we have updated
				if(wait_for_thread(tinfo.threads[i].pid)<0) return NULL;
				
	// !debug
	////printf("tid stuck is %d\n", tid);
				
			}
			retval = tinfo.threads[i].ret_addr; 
			tinfo.threads[i].status = TH_STATUS_UNUSED;
			tinfo.threads[i].pid = -1;
			// finished
			break;
		}
	}
	// !debug
	////printf("CP 2\n");
	if(!found) return NULL;
	// thread finished
	// destroy all reference to the thread
	////printf("!!!!Check!!!!...return value is %d\n", *(int *)retval);

	// destroying stack
	munmap(tinfo.threads[i].stack_addr, TH_STACK_SIZE);
	return retval;
}


/*Only threads will invoke this. No need to check if its a process
 * The allocation size is always < GALLOC_MAX and flags can be one
 * of the alloc flags (GALLOC_*) defined in gthread.h. Need to 
 * invoke mmap using the proper protection flags (for prot param to mmap)
 * and MAP_TH_PRIVATE as the flag param of mmap. The mmap call will be 
 * handled by handle_thread_private_map in the OS.
 * */

void* gmalloc(u32 size, u8 alloc_flag)
{
   
	/* You need to fill in your implementation here*/
	void * mem = NULL;
	int prot_par = PROT_READ|PROT_WRITE;
	// determining the third parameter to bitwise OR:
	if(alloc_flag==GALLOC_OWNONLY){
		prot_par|=TP_SIBLINGS_NOACCESS;
	}
	else if(alloc_flag==GALLOC_OTRDONLY) {
		prot_par|=TP_SIBLINGS_RDONLY;
	}
	else if(alloc_flag==GALLOC_OTRDWR) {
		prot_par|=TP_SIBLINGS_RDWR;
	}
	else {
		return NULL;
	}
	
	//maintaining user space info for private mapping
	int found = 0;
	u32 pid = getpid();
	for(int i = 0;i<MAX_THREADS;i++) {
		if(tinfo.threads[i].pid == pid) {
			if(tinfo.threads[i].status==TH_STATUS_UNUSED) 
				return NULL; //error case
			
			// the current thread
			// searching through priv areas to find if there is space to accomodate:
			for(int j = 0;j<MAX_GALLOC_AREAS;j++) {
				if(tinfo.threads[i].priv_areas[j].owner == NULL) {//! is it enough to only check owner NULL hai to its unused
					
					found = 1;
					tinfo.threads[i].priv_areas[j].owner = &(tinfo.threads[i]);
					// allocate memory:
					mem = mmap(NULL,size, prot_par,MAP_TH_PRIVATE);
					//printf("gmalloc called, inside the loop\n");
					tinfo.threads[i].priv_areas[j].start = (u64)mem;
					tinfo.threads[i].priv_areas[j].length = size;
					tinfo.threads[i].priv_areas[j].flags = alloc_flag; // !Check, the datatypes are u8 and u32 
					break;
				}
			}

		}
	}
	if(!found) return NULL;
	return mem;
}
/*
   Only threads will invoke this. No need to check if the caller is a process.
*/
int gfree(void *ptr)
{
   
    /* You need to fill in your implementation here*/
	u32 pid = getpid();
	int found = 0;
	for(int i = 0;i<MAX_THREADS;i++) {
		if(tinfo.threads[i].pid == pid) {
			// calling threads
			// check if the ptr is valid:
			for(int j = 0;j<MAX_GALLOC_AREAS;j++) {
				if(tinfo.threads[i].priv_areas[j].start == (u64)ptr) {
					//printf("Pahucha in gfree inside loop\n");
					found = 1;
					munmap(ptr, tinfo.threads[i].priv_areas[j].length);
					// make the owner NULL:
					tinfo.threads[i].priv_areas[j].owner = NULL;
				} 

				
			}
		}
	}
	if(!found) return -1;

	return 0;
}

