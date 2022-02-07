#include<clone_threads.h>
#include<entry.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<mmap.h>

/*
  system call handler for clone, create thread like 
  execution contexts. Returns pid of the new context to the caller. 
  The new context starts execution from the 'th_func' and 
  use 'user_stack' for its stack
*/

long do_clone(void *th_func, void *user_stack, void *user_arg) 
{
  


  struct exec_context *new_ctx = get_new_ctx();
  struct exec_context *ctx = get_current_ctx();

  u32 pid = new_ctx->pid;
  
  if(!ctx->ctx_threads){  // This is the first thread
          ctx->ctx_threads = os_alloc(sizeof(struct ctx_thread_info));
          bzero((char *)ctx->ctx_threads, sizeof(struct ctx_thread_info));
          ctx->ctx_threads->pid = ctx->pid;
  }
     
 /* XXX Do not change anything above. Your implementation goes here*/
  
  
  // allocate page for os stack in kernel part of process's VAS
  // doing it:
  ctx->os_stack_pfn = os_pfn_alloc(OS_PT_REG); // ! is this all? 
  //////printk("For parent , type is %d\n", ctx->type);
    // The following two lines should be there. The order can be 
  // decided depending on your logic.
   setup_child_context(new_ctx);
   new_ctx->type = EXEC_CTX_USER_TH;    // Make sure the context type is thread
  
   new_ctx->state = READY;            //! check
  // copying most of the parent's content to the thread:
  new_ctx->alarm_config_time = ctx->alarm_config_time;
  //new_ctx->files = ctx->files;
  for(int i = 0;i<MAX_OPEN_FILES;i++) {
    new_ctx->files[i] = ctx->files[i];
  }
  //new_ctx->mms = ctx->mms;
  for(int i = 0;i<MAX_MM_SEGS;i++) {
    new_ctx->mms[i] = ctx->mms[i];
  }
  //new_ctx->name = ctx->name;
  for(int i = 0;i<CNAME_MAX;i++) {
    new_ctx->name[i] = ctx->name[i];
  }
  new_ctx->os_rsp; // every process(here a thread) has a separate stack...
  new_ctx->pending_signal_bitmap = ctx->pending_signal_bitmap;
  new_ctx->pgd = ctx->pgd;
  new_ctx->ppid = ctx->pid; // first change from the parent
  new_ctx->regs = ctx->regs; // thread will have separate regs
  //new_ctx->sighandlers = ctx->sighandlers;
  for(int i = 0;i<MAX_SIGNALS;i++) {
    new_ctx->sighandlers[i] = ctx->sighandlers[i];
  }
  new_ctx->ticks_to_alarm = ctx->ticks_to_alarm;
  new_ctx->ticks_to_sleep = ctx->ticks_to_sleep;
  new_ctx->used_mem = ctx->used_mem;
  new_ctx->vm_area = ctx->vm_area;

     // modifying rip value of the thread so that it starts executing with th_func 
   new_ctx->regs.entry_rip = (u64)th_func; 
  

   // initialising the thread with its own stack:
   new_ctx->regs.entry_rsp = (u64)user_stack; 

   // putting the argument in the rdi register:
   new_ctx->regs.rdi = ((u64)user_arg);// !check
  
   new_ctx->regs.rbp = (u64)user_stack;

//  ////printk("Address inside rsp is %x\n", *(u64*)(new_ctx->regs.entry_rsp));
// ////printk("Address in rip register is %x\n", new_ctx->regs.entry_rip);

  // add the thread to parent's ctx_threads;
  ctx->ctx_threads->pid = ctx->pid; // storing the parent's pid
  
  int found = 0;
  for(int i = 0;i<MAX_THREADS;i++) {
        if(ctx->ctx_threads->threads[i].status ==TH_UNUSED )  // unused thread found, fill it in
    {
      found = 1;
      ctx->ctx_threads->threads[i].parent_ctx = ctx;
      ctx->ctx_threads->threads[i].pid = new_ctx->pid;
      ctx->ctx_threads->threads[i].private_mappings;// !TODO
      ctx->ctx_threads->threads[i].status = TH_USED;
      break;
    }
  }   
  if(!found) return -1;
  return pid;


	
  return -1;

}

/*This is the page fault handler for thread private memory area (allocated using 
 * gmalloc from user space). This should fix the fault as per the rules. If the the 
 * access is legal, the fault handler should fix it and return 1. Otherwise it should
 * invoke segfault_exit and return -1*/

int handle_thread_private_fault(struct exec_context *current, u64 addr, int error_code)
{
  //printk("Handle page fauult\n");
   /* your implementation goes here*/
    // determining if its the parent or the thread
    int flag = 0; // kind of flag
    if(current->type == EXEC_CTX_USER) {
      flag = 1;
    }

    // finding the owner, will always be a thread
    // first find the parent:
    struct exec_context * parent;
    if(flag) {
      parent = current;
    }
    else {
      parent = get_ctx_by_pid((current->ppid));
    }
    // parent found, now find the ownner:
    struct thread owner; 
    int found = 0;
    int j;
    for(int i=0;i<MAX_THREADS;i++) {
        
      struct thread temp = parent->ctx_threads->threads[i];
      if(temp.status==TH_UNUSED) continue;
      for(j = 0;j<MAX_PRIVATE_AREAS;j++) {
        if(temp.private_mappings[j].owner == NULL) continue;
        u64 start = temp.private_mappings[j].start_addr;
        u64 end = start + temp.private_mappings[j].length -1;
        if(start<=addr && addr<=end) {
          // owner found
          owner = temp; 
          found = 1;
          break;

        }
      }
      if(found) break;
    }
    if(!found)  
    {
      //printk("BADDD\n");
      segfault_exit(current->pid, current->regs.entry_rip, addr);  
      
    }
    // walk the page table
    
     
     
    //proceeding, owner thread is found

    // The cases when there is no segfault are:
    // parent calls, self-call, other thread call but R/W mapping allowed, R/O but 0x4
    int access_flag = owner.private_mappings[j].flags;
    if(flag || owner.pid == current->pid || (access_flag & TP_SIBLINGS_RDWR)==TP_SIBLINGS_RDWR || ((access_flag & TP_SIBLINGS_RDONLY)==TP_SIBLINGS_RDONLY)&&(error_code==0x4)){
      // that means the fault is repairable, check the point of fault by page walk and repair
      //printk("Inside page walk\n");
      if(flag) {
        //printk("------FLAG--------\n");
        access_flag = 0x7;
      }
      if(owner.pid == current->pid){

        //printk("------SELF CALL--------\n");
        access_flag = 0x7;
      }
      if((access_flag & TP_SIBLINGS_RDWR)==TP_SIBLINGS_RDWR ) {

        //printk("------RDWR flag--------\n");
        access_flag = 0x7;
      }
      if((((access_flag & TP_SIBLINGS_RDONLY)==TP_SIBLINGS_RDONLY)&&(error_code==0x4))) {
        
        //printk("------R/O 0x4--------\n");
        access_flag = 0x5;
      }

      // ! PAGE WALK ROUTINE 
      u64 pgd_addr = (u64)osmap(current->pgd);
      u64 pgd_offset = ((addr&(PGD_MASK))>>PGD_SHIFT)*8; 
      u64 pgd_t = *((u64 *)(pgd_addr+pgd_offset));
      u64 pud_pfn;
      if((pgd_t&0x1)) {   // mapped
        // proceed
        pud_pfn = (pgd_t>>PAGE_SHIFT);
        *((u64 *)(pgd_addr+pgd_offset)) |= access_flag;
        //printk("------cp1--------\n");
      }
      else {
        // implement corrections
        pud_pfn = os_pfn_alloc(OS_PT_REG);
        *((u64 *)(pgd_addr+pgd_offset)) = (pud_pfn<<PAGE_SHIFT)|access_flag; // !check
        //printk("------cp2--------\n");
      }
      u64 pud_addr = (u64)osmap(pud_pfn);
      u64 pud_offset = ((addr&(PUD_MASK))>>PUD_SHIFT)*8;
      u64 pud_t = *((u64 *)(pud_addr+pud_offset));
      u64 pmd_pfn;
      if((pud_t&0x1)) {
        // proceed
        pmd_pfn = pud_t>>PAGE_SHIFT;
        *((u64 *)(pud_addr+pud_offset)) |= access_flag;
        //printk("------cp3--------\n");
      }
      else {
        pmd_pfn = os_pfn_alloc(OS_PT_REG);
        *((u64 *)(pud_addr+pud_offset)) = (pmd_pfn<<PAGE_SHIFT)|access_flag;
        //printk("------cp4--------\n");
      }
      u64 pmd_addr = (u64)osmap(pmd_pfn);
      u64 pmd_offset = ((addr&(PMD_MASK))>>PMD_SHIFT)*8;
      u64 pmd_t = *((u64 *)(pmd_addr+pmd_offset));
      u64 pte_pfn;
      if((pmd_t&0x1)) {
        pte_pfn = pmd_t>>PAGE_SHIFT;
        *((u64 *)(pmd_addr+pmd_offset)) |= access_flag;
        //printk("------cp5--------\n");
      }
      else {
        pte_pfn = os_pfn_alloc(OS_PT_REG);
        *((u64 *)(pmd_addr+pmd_offset)) = (pte_pfn<<PAGE_SHIFT)|access_flag;
        //printk("------cp6--------\n");
      }
      u64 pte_addr = (u64)osmap(pte_pfn);
      u64 pte_offset = ((addr&(PTE_MASK))>>PTE_SHIFT)*8;
      u64 pte_t = *((u64 *)(pte_addr+pte_offset));
      u64 phys_pfn;
      if((pte_t&0x1)) {
        phys_pfn = pte_t>>PAGE_SHIFT;
        *((u64 *)(pte_addr+pte_offset)) |= access_flag;
        //printk("------cp7--------\n");
        ////printk("Faulting last 3 bits are %d\n",pte_t&0x7);
      }
      else {
        ////printk("Faulting last 3 bits are %d\n",pte_t&0x7);
        phys_pfn = os_pfn_alloc(USER_REG);
        *((u64 *)(pte_addr+pte_offset)) = (phys_pfn<<PAGE_SHIFT)|access_flag;
        //printk("------cp8--------\n");
      }
      return 1;
           
    }
    //printk("Reaching segfault exit at %d\n", current->pid);
    segfault_exit(current->pid, current->regs.entry_rip, addr); 
    return -1;
}

/*This is a handler called from scheduler. The 'current' refers to the outgoing context and the 'next' 
 * is the incoming context. Both of them can be either the parent process or one of the threads, but only
 * one of them can be the process (as we are having a system with a single user process). This handler
 * should apply the mapping rules passed in the gmalloc calls. */

int handle_private_ctxswitch(struct exec_context *current, struct exec_context *next)
{
  
   /* your implementation goes here*/
   // find the parent
   struct exec_context* parent;
   if(current->type == EXEC_CTX_USER) {
     parent = current;
   }
   else if(next->type == EXEC_CTX_USER) {
     parent = next;
   }
   else {
     parent = get_ctx_by_pid(current->ppid);
   }
   for(int i = 0;i<MAX_THREADS;i++) {
     struct thread temp = parent->ctx_threads->threads[i]; 
     if(temp.status==TH_UNUSED) continue;
     for(int j=0;j<MAX_PRIVATE_AREAS;j++) {
      if(temp.private_mappings[j].owner!=NULL) {
        //a private area
        for(u64 addr = temp.private_mappings[j].start_addr; addr<temp.private_mappings[j].start_addr+temp.private_mappings[j].length;addr+=4096 ) { //! check
                  if(next==parent || next->pid == temp.pid || (temp.private_mappings[j].flags&TP_SIBLINGS_RDWR)) {
                        // page walk 1...0x7
                        // ! PAge walk routine
                        ////printk("routine 1....\n");
                      u64 pgd_addr = (u64)osmap(current->pgd);
                      u64 pgd_offset = ((addr&(PGD_MASK))>>PGD_SHIFT)*8; 
                      u64 pgd_t = *((u64 *)(pgd_addr+pgd_offset));
                      u64 pud_pfn;
                      if((pgd_t&0x1)) {   // mapped
                        // proceed
                        
                       // pgd_t=(pgd_t)|0x7;
                       // *((u64 *)(pgd_addr+pgd_offset)) = pgd_t;
                        pud_pfn = (pgd_t>>PAGE_SHIFT);
                        
                        ////asm volatile("invlpg (%0)" ::"r" (osmap(pud_pfn)) : "memory");
                        ////asm volatile ("invlpg %0"::"r"(osmap(pud_pfn)));
                        ////printk("------cp1 ctx--------\n");
                      }
                      else {
                        continue;
                      }
                      u64 pud_addr = (u64)osmap(pud_pfn);
                      u64 pud_offset = ((addr&(PUD_MASK))>>PUD_SHIFT)*8;
                      u64 pud_t = *((u64 *)(pud_addr+pud_offset));
                      u64 pmd_pfn;
                      if((pud_t&0x1)) {
                        // proceed
                        
                      //  pud_t=(pud_t)|0x7;
                       // *((u64 *)(pud_addr+pud_offset)) = pud_t;
                        pmd_pfn = pud_t>>PAGE_SHIFT;
                        //asm volatile ("invlpg %0"::"r"(osmap(pmd_pfn)));
                        ////printk("------cp3 ctx--------\n");
                      }
                      else {
                        continue;
                      }
                      u64 pmd_addr = (u64)osmap(pmd_pfn);
                      u64 pmd_offset = ((addr&(PMD_MASK))>>PMD_SHIFT)*8;
                      u64 pmd_t = *((u64 *)(pmd_addr+pmd_offset));
                      u64 pte_pfn;
                      if((pmd_t&0x1)) {
                        
                      //  pmd_t=(pmd_t)|0x7;
                      //  *((u64 *)(pmd_addr+pmd_offset)) = pmd_t;
                        pte_pfn = pmd_t>>PAGE_SHIFT;
                        //asm volatile ("invlpg %0"::"r"(osmap(pte_pfn)));
                        ////printk("------cp5 ctx--------\n");
                      }
                      else {
                        continue;
                      }
                      u64 pte_addr = (u64)osmap(pte_pfn);
                      u64 pte_offset = ((addr&(PTE_MASK))>>PTE_SHIFT)*8;
                      u64 pte_t = *((u64 *)(pte_addr+pte_offset));
                      u64 phys_pfn;
                      if((pte_t&0x1)) {
                        
                        pte_t=(pte_t)|0x7;
                        *((u64 *)(pte_addr+pte_offset)) = pte_t;
                        phys_pfn = pte_t>>PAGE_SHIFT;
                        //asm volatile("invlpg (%0)" ::"r" (addr));
                        //asm volatile ("invlpg %0"::"r"(osmap(phys_pfn)));
                        ////printk("------cp7 ctx--------\n");
                        
                      }
                      else {
                        continue;
                      }          

                  }
                  else if(temp.private_mappings[j].flags & TP_SIBLINGS_NOACCESS) {
                      // page walk 2...0x1  
                      // !Page Walk routine 2
                      ////printk("ROUTINE 2............\n");
                    u64 pgd_addr = (u64)osmap(current->pgd);
                    u64 pgd_offset = ((addr&(PGD_MASK))>>PGD_SHIFT)*8; 
                    u64 pgd_t = *((u64 *)(pgd_addr+pgd_offset));
                    u64 pud_pfn;
                    if((pgd_t&0x1)) {   // mapped
                      // proceed
                      
                     // pgd_t=((pgd_t)&(~0x7))|0x1;
                     // *((u64 *)(pgd_addr+pgd_offset)) = pgd_t;
                      pud_pfn = (pgd_t>>PAGE_SHIFT);
                      //asm volatile ("invlpg %0"::"r"(osmap(pud_pfn)));
                      ////printk("------cp1 ctx--------\n");
                    }
                    else {
                      continue;
                    }
                    u64 pud_addr = (u64)osmap(pud_pfn);
                    u64 pud_offset = ((addr&(PUD_MASK))>>PUD_SHIFT)*8;
                    u64 pud_t = *((u64 *)(pud_addr+pud_offset));
                    u64 pmd_pfn;
                    if((pud_t&0x1)) {
                      // proceed
                    
                     // pud_t=((pud_t)&(~0x7))|0x1;
                     // *((u64 *)(pud_addr+pud_offset)) = pud_t;
                      pmd_pfn = pud_t>>PAGE_SHIFT;
                      //asm volatile ("invlpg %0"::"r"(osmap(pmd_pfn)));
                      ////printk("------cp3 ctx--------\n");
                    }
                    else {
                      continue;
                    }
                    u64 pmd_addr = (u64)osmap(pmd_pfn);
                    u64 pmd_offset = ((addr&(PMD_MASK))>>PMD_SHIFT)*8;
                    u64 pmd_t = *((u64 *)(pmd_addr+pmd_offset));
                    u64 pte_pfn;
                    if((pmd_t&0x1)) {
                      
                     // pmd_t=((pmd_t)&(~0x7))|0x1;
                     // *((u64 *)(pmd_addr+pmd_offset)) = pmd_t;
                      pte_pfn = pmd_t>>PAGE_SHIFT;
                      //asm volatile ("invlpg %0"::"r"(osmap(pte_pfn)));
                      ////printk("------cp5 ctx--------\n");
                    }
                    else {
                      continue;
                    }
                    u64 pte_addr = (u64)osmap(pte_pfn);
                    u64 pte_offset = ((addr&(PTE_MASK))>>PTE_SHIFT)*8;
                    u64 pte_t = *((u64 *)(pte_addr+pte_offset));
                    u64 phys_pfn;
                    if((pte_t&0x1)) {
                      
                      pte_t=((pte_t)&(~0x7))|0x1;
                      *((u64 *)(pte_addr+pte_offset)) = pte_t;
                      phys_pfn = pte_t>>PAGE_SHIFT;
                      
                      //asm volatile ("invlpg %0"::"r"(osmap(phys_pfn)));
                      ////printk("------cp7 ctx--------\n");
                      
                    }
                    else {
                      continue;
                    }          

                    }
                    else if(temp.private_mappings[j].flags & TP_SIBLINGS_RDONLY) {
                      // page walk3...0x5
                      // ! Page walk routine 3
                      ////printk("routine 3....\n");
                    u64 pgd_addr = (u64)osmap(current->pgd);
                    u64 pgd_offset = ((addr&(PGD_MASK))>>PGD_SHIFT)*8; 
                    u64 pgd_t = *((u64 *)(pgd_addr+pgd_offset));
                    u64 pud_pfn;
                    if((pgd_t&0x1)) {   // mapped
                      // proceed
                      
                     // pgd_t=((pgd_t)&(~0x7))|0x5;
                     // *((u64 *)(pgd_addr+pgd_offset)) = pgd_t;
                      pud_pfn = (pgd_t>>PAGE_SHIFT);
                      //asm volatile ("invlpg %0"::"r"(osmap(pud_pfn)));
                      ////printk("------cp1 ctx--------\n");
                    }
                    else {
                      continue;
                    }
                    u64 pud_addr = (u64)osmap(pud_pfn);
                    u64 pud_offset = ((addr&(PUD_MASK))>>PUD_SHIFT)*8;
                    u64 pud_t = *((u64 *)(pud_addr+pud_offset));
                    u64 pmd_pfn;
                    if((pud_t&0x1)) {
                      // proceed
                      
                      //pud_t=((pud_t)&(~0x7))|0x5;
                      //*((u64 *)(pud_addr+pud_offset)) = pud_t;
                      pmd_pfn = pud_t>>PAGE_SHIFT;
                      //asm volatile ("invlpg %0"::"r"(osmap(pmd_pfn)));
                      ////printk("------cp3 ctx--------\n");
                    }
                    else {
                      continue;
                    }
                    u64 pmd_addr = (u64)osmap(pmd_pfn);
                    u64 pmd_offset = ((addr&(PMD_MASK))>>PMD_SHIFT)*8;
                    u64 pmd_t = *((u64 *)(pmd_addr+pmd_offset));
                    u64 pte_pfn;
                    if((pmd_t&0x1)) {
                      
                     // pmd_t=((pmd_t)&(~0x7))|0x5;
                     // *((u64 *)(pmd_addr+pmd_offset)) = pmd_t;
                      pte_pfn = pmd_t>>PAGE_SHIFT;
                      //asm volatile ("invlpg %0"::"r"(osmap(pte_pfn)));
                      ////printk("------cp5 ctx--------\n");
                    }
                    else {
                      continue;
                    }
                    u64 pte_addr = (u64)osmap(pte_pfn);
                    u64 pte_offset = ((addr&(PTE_MASK))>>PTE_SHIFT)*8;
                    u64 pte_t = *((u64 *)(pte_addr+pte_offset));
                    u64 phys_pfn;
                    if((pte_t&0x1)) {
                      
                      pte_t=((pte_t)&(~0x7))|0x5;
                      *((u64 *)(pte_addr+pte_offset)) = pte_t;
                      phys_pfn = pte_t>>PAGE_SHIFT;
                      //asm volatile("invlpg (%0)" ::"r" (addr));
                      //asm volatile ("invlpg %0"::"r"(osmap(phys_pfn)));
                      ////printk("------cp7--------\n");
                      
                    }
                    else {
                      continue;
                    }          
               }
               asm volatile("invlpg (%0)" ::"r" (addr) );
        }
      }
     }
   } 
   return 0;	

}

