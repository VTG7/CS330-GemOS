#include<pipe.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>


// Per process info for the pipe.
struct pipe_info_per_process {

    // TODO:: Add members as per your need...
    int readopen; // whether the read end of the pipe, for that process is open or not.
    int writeopen;// whether the write end of the pipe, for that process is open or not.
    int pid; // stores the pid of the process.
};

// Global information for the pipe.
struct pipe_info_global {

    char *pipe_buff;    // Pipe buffer: DO NOT MODIFY THIS.

    // TODO:: Add members as per your need...
    int read_index;     // the read pointer
    int write_index;    // the write pointer 
    int num_process;    // number of processes associated with the pipe
    int av_space;       // stores the space available in the pipe buffer

};

// Pipe information structure.
// NOTE: DO NOT MODIFY THIS STRUCTURE.
struct pipe_info {

    struct pipe_info_per_process pipe_per_proc [MAX_PIPE_PROC];
    struct pipe_info_global pipe_global;

};


// Function to allocate space for the pipe and initialize its members.
struct pipe_info* alloc_pipe_info () {
	
    // Allocate space for pipe structure and pipe buffer.
    struct pipe_info *pipe = (struct pipe_info*)os_page_alloc(OS_DS_REG);
    char* buffer = (char*) os_page_alloc(OS_DS_REG);

    // Assign pipe buffer.
    pipe->pipe_global.pipe_buff = buffer;

    /**
     *  TODO:: Initializing pipe fields
     *  
     *  Initialize per process fields for this pipe.
     *  Initialize global fields for this pipe.
     *
     */

    // Begin
    // filling per process and global info fields
    pipe->pipe_per_proc[0].readopen = 1;      
    pipe->pipe_per_proc[0].writeopen = 1; 
    struct exec_context * current = get_current_ctx();
    pipe->pipe_per_proc[0].pid = current->pid;
    for(int index = 0;index<MAX_PIPE_PROC;index++) {
        if(index==0) {
            pipe->pipe_global.num_process =0;
            continue;
        }
        pipe->pipe_per_proc[index].pid = -1;
    }
    pipe->pipe_global.num_process++;
    pipe->pipe_global.read_index=-1;
    pipe->pipe_global.write_index=-1;
    pipe->pipe_global.av_space=MAX_PIPE_SIZE;
    // Return the pipe.
    return pipe;

}

// Function to free pipe buffer and pipe info object.
// NOTE: DO NOT MODIFY THIS FUNCTION.
void free_pipe (struct file *filep) {

    os_page_free(OS_DS_REG, filep->pipe->pipe_global.pipe_buff);
    os_page_free(OS_DS_REG, filep->pipe);

}

// Fork handler for the pipe.
int do_pipe_fork (struct exec_context *child, struct file *filep) {

    /**
     *  TODO:: Implementation for fork handler
     *
     *  You may need to update some per process or global info for the pipe.
     *  This handler will be called twice since pipe has 2 file objects.
     *  Also consider the limit on no of processes a pipe can have.
     *  Return 0 on success.
     *  Incase of any error return -EOTHERS.
     *
     */

    // Begin

    //checking if the file pointer passed is NULL
    if(!filep) {
        return 0;
    } 

    //checking if the limit of the process has been reached
    if(filep->pipe->pipe_global.num_process==MAX_PIPE_PROC) return -EOTHERS;

    //pid of parent and child:
    struct exec_context *current = get_current_ctx();
    int pid_parent = current->pid;   
    int pid_child = child->pid;
    // !!!! DEBUG
    //printk("PPID through child %d \n",child->ppid);
    //printk("PPID through exec context %d \n",pid_parent);
    // index of parent
    int index_parent=-1;
    for(int i = 0;i<MAX_PIPE_PROC;i++) {
        if(filep->pipe->pipe_per_proc[i].pid==pid_parent) {
            index_parent = i;
            break;
        }

    }
    // not checking for index=-1 since kuch to hoga hi non zero index
    // // !!!! DEBUG
    // printk("parent index inside fork is %d\n", index_parent);

    // now the idea is, if the pid_child is present in the per process info, that means that 
    // fork has been called once already, if not, then this is the first call

    //UPDATE: second call unnecessary, but i will leave it here, since it makes more intuitive sense to me
    int flag = 0;
    for(int i = 0;i<MAX_PIPE_PROC;i++) {
        if(filep->pipe->pipe_per_proc[i].pid==pid_child) {
            // second call
            flag = 1;
            // // !!!! DEBUG
            // printk("Second call\n");
            if(filep->mode == O_READ){
                // read end hai
                filep->pipe->pipe_per_proc[i].readopen = filep->pipe->pipe_per_proc[index_parent].readopen; 
            }
            else if(filep->mode == O_WRITE) {
                // write end hai
                filep->pipe->pipe_per_proc[i].writeopen = filep->pipe->pipe_per_proc[index_parent].writeopen; 

            }
            else {
                return -EOTHERS;
            }
        }
    }
    if(flag) {
        //second call thi, return 
        return 0;
    }
    // control reaches here if its the first call
    for(int i = 0;i<MAX_PIPE_PROC;i++) {
        if(filep->pipe->pipe_per_proc[i].pid==-1) {
            //use this index to store the child'd pid now
            filep->pipe->pipe_per_proc[i].pid = pid_child;
            if(filep->mode == O_READ){
                // read end hai
                filep->pipe->pipe_per_proc[i].readopen = filep->pipe->pipe_per_proc[index_parent].readopen; 
                filep->pipe->pipe_per_proc[i].writeopen = 0; // this is necessary to be done on the first call

            }
            else if(filep->mode == O_WRITE) {
                // write end hai
                filep->pipe->pipe_per_proc[i].writeopen = filep->pipe->pipe_per_proc[index_parent].writeopen; 
                filep->pipe->pipe_per_proc[i].readopen = 0; // this is necessary to be done on the first call

            }
            else {
                return -EOTHERS;
            }
            filep->pipe->pipe_global.num_process++;
            break;
        }
    }
    // Return successfully.
    return 0;

}

// Function to close the pipe ends and free the pipe when necessary.
long pipe_close (struct file *filep) {

    /**
     *  TODO:: Implementation of Pipe Close
     *
     *  Close the read or write end of the pipe depending upon the file
     *      object's mode.
     *  You may need to update some per process or global info for the pipe.
     *  Use free_pipe() function to free pipe buffer and pipe object,
     *      whenever applicable.
     *  After successful close, it return 0.
     *  Incase of any error return -EOTHERS.
     *
     */


    // Begin

    int index = -1;  // the process index
    //pid  
    struct exec_context *current = get_current_ctx();
    int pid_parent = current->pid;   
    // index 
    for(int i = 0;i<MAX_PIPE_PROC;i++) {
        if(filep->pipe->pipe_per_proc[i].pid==pid_parent) {
            index = i;
            break;
        }

    } 
    if(index==-1) return -EOTHERS;   
    if(filep->mode == O_READ) {
        filep->pipe->pipe_per_proc[index].readopen = 0;
    }
    else if(filep->mode == O_WRITE){
        filep->pipe->pipe_per_proc[index].writeopen = 0;
    }
    else return -EOTHERS;
    if(filep->pipe->pipe_per_proc[index].readopen == 0 && filep->pipe->pipe_per_proc[index].writeopen == 0) {
        filep->pipe->pipe_global.num_process--;
        filep->pipe->pipe_per_proc[index].pid = -1; // basically, making this index available again...
        if(filep->pipe->pipe_global.num_process ==0) free_pipe(filep);
    }
    int ret_value;

     
    // Close the file and return.
    ret_value = file_close (filep);         // DO NOT MODIFY THIS LINE.

    // And return.
    return ret_value;

}

// Check whether passed buffer is valid memory location for read or write.
int is_valid_mem_range (unsigned long buff, u32 count, int access_bit) {

    /**
     *  TODO:: Implementation for buffer memory range checking
     *
     *  Check whether passed memory range is suitable for read or write.
     *  If access_bit == 1, then it is asking to check read permission.
     *  If access_bit == 2, then it is asking to check write permission.
     *  If range is valid then return 1.
     *  Incase range is not valid or have some permission issue return -EBADMEM.
     *
     */

     // Begin

    
    struct exec_context *current = get_current_ctx();
    struct mm_segment *mms = current->mms;
    struct vm_area * vm_area = current->vm_area;

    //for access_bit ==1
    if(access_bit==1) {
        // checking for read permission
        // first checking if there is a segment in mm_segment
        int avail = 0;
        int i;
        for(i = 0;i<MAX_MM_SEGS-1;i++) {
            if(mms[i].start<=buff && (buff+count-1)<mms[i].next_free && ((mms[i].access_flags&1) ==1)) {
                // check if we need to do <-=next_free or just <next_free will suffice
                // update: mentioned in piazza for < in mms segments[except stack]
                avail = 1;
                break;
            }
        }
        if(avail) return 1;

        //reaching here means not available till now
        // checking the stack
        if(mms[i].start<=buff && (buff+count-1)<=mms[i].end && ((mms[i].access_flags&1) ==1)) {
            avail = 1;   
             
        }
        if(avail) return 1;

        //reaching here means not available in mm_seg at all, checking in vm_area
        while(vm_area!=NULL) {
            if(vm_area->vm_start<=buff && (buff+count-1)<=vm_area->vm_end && ((vm_area->access_flags&1) ==1)) {
                avail = 1;
                break;
            }
            vm_area = vm_area->vm_next;
        }
        if(avail) return 1;

        //reaching here => not availalbe at all
        // -EBADMEM is returned already at the bottom

    }
    else if(access_bit==2) {
        // checking for write permission
        // first checking if there is a segment in mm_segment
        int avail = 0;
        int i;
        for(i = 0;i<MAX_MM_SEGS-1;i++) {
                            // !!!! DEBUG
                //printk("Bitwise and value is %d\n",mms[i].access_flags&2);
            if(mms[i].start<=buff && (buff+count-1)<mms[i].next_free && ((mms[i].access_flags&2) ==2)) {
                
                avail = 1;
                break;
            }
        }
        if(avail) return 1;

        //reaching here means not available till now
        // checking the stack
                         // !!!! DEBUG
                //printk("Bitwise and value in stack is %d\n",mms[i].access_flags&2); 
                //printk("VM area address is %x\n" ,vm_area->vm_next);
        if(mms[i].start<=buff && (buff+count-1)<=mms[i].end && ((mms[i].access_flags&2) ==2)) {
          
            avail = 1;   
        }
        if(avail) return 1;

        //reaching here means not available in mm_seg at all, checking in vm_area
       while(vm_area!=NULL) {
                             // !!!! DEBUG
                // printk("Bitwise and value in vm_area is %d\n",vm_area->access_flags&2); 
            if(vm_area->vm_start<=buff && (buff+count-1)<=vm_area->vm_end && ((vm_area->access_flags&2) ==2)) {
                avail = 1;
                break;
            }
            vm_area = vm_area->vm_next;
        }
        if(avail) return 1;

        //reaching here => not availalbe at all
        // -EBADMEM is returned already at the bottom

    }
    // if nothing is returned from above, it means no area segment found OR the access_bit passed is incorrect
    // in either case, -EBADMEM to be returned. 
    int ret_value = -EBADMEM;

    // Return the finding.
    return ret_value;

}

// Function to read given no of bytes from the pipe.
int pipe_read (struct file *filep, char *buff, u32 count) {

    /**
     *  TODO:: Implementation of Pipe Read
     *
     *  Read the data from pipe buffer and write to the provided buffer.
     *  If count is greater than the present data size in the pipe then just read
     *       that much data.
     *  Validate file object's access right.
     *  On successful read, return no of bytes read.
     *  Incase of Error return valid error code.
     *       -EACCES: In case access is not valid.
     *       -EINVAL: If read end is already closed.
     *       -EOTHERS: For any other errors.
     *
     */

    
    //Begin:

    // checking if the buff lies in valid memory range
    if(is_valid_mem_range((unsigned long)buff, count, 2)!=1) return -EBADMEM;
    // checking if it is the read_end 
    if(filep->mode != O_READ) return -EACCES;

    // checking if the process calling read is a valid process for a pipe
    int index = -1;  // the process index
    //pid  
    struct exec_context *current = get_current_ctx();
    int pid_parent = current->pid;   
    // index 
    for(int i = 0;i<MAX_PIPE_PROC;i++) {
        if(filep->pipe->pipe_per_proc[i].pid==pid_parent) {
            index = i;
            break;
        }

    } 
    if(index==-1) return -EOTHERS;  
    
    // checking if the read end of the process is closed...I did it anyway:
    if(filep->pipe->pipe_per_proc[index].readopen == 0) return -EINVAL;

    // first, calculating the data size
    int datasize = MAX_PIPE_SIZE - filep->pipe->pipe_global.av_space;
    int read_index = filep->pipe->pipe_global.read_index;
    int write_index = filep->pipe->pipe_global.write_index;
    char *pipe_buff = filep->pipe->pipe_global.pipe_buff;
   // if(write_index>=read_index) datasize = write_index-read_index;
    //else datasize = MAX_PIPE_SIZE-(read_index-write_index);
      // !!!! DEBUG
       // printk("NOW READING with write index as %d and  read index as %d \n", write_index, read_index);  
    // count greater than the present data size check
    if(count>datasize) count = datasize;
    
    // reading now
    int bytes_read = 0;
    
    while(bytes_read!=count) {
           
        buff[bytes_read] = pipe_buff[(read_index+1+bytes_read)%MAX_PIPE_SIZE]; 
        bytes_read++;
    }
    
    read_index = (read_index+bytes_read)%MAX_PIPE_SIZE;
    // !!!! DEBUG
    //printk("The pipe_buf stores %s\n", pipe_buff);
    filep->pipe->pipe_global.read_index = read_index;
    filep->pipe->pipe_global.av_space += bytes_read;
    // Return no of bytes read.
    return bytes_read;

}

// Function to write given no of bytes to the pipe.
int pipe_write (struct file *filep, char *buff, u32 count) {

    /**
     *  TODO:: Implementation of Pipe Write
     *
     *  Write the data from the provided buffer to the pipe buffer.
     *  If count is greater than available space in the pipe then just write data
     *       that fits in that space.
     *  Validate file object's access right.
     *  On successful write, return no of written bytes.
     *  Incase of Error return valid error code.
     *       -EACCES: In case access is not valid.
     *       -EINVAL: If write end is already closed.
     *       -EOTHERS: For any other errors.
     *
     */

    //Begin
   
    // checking if the buff lies in valid memory range
    if(is_valid_mem_range((unsigned long)buff, count, 1)!=1) return -EBADMEM;
    // checking if it is the write end
    if(filep->mode!=O_WRITE) return -EACCES;

    // checking if the process calling write is a valid process for a pipe
    int index = -1;  // the process index
    //pid  
    struct exec_context *current = get_current_ctx();
    int pid_parent = current->pid;   
    // index 
    for(int i = 0;i<MAX_PIPE_PROC;i++) {
        if(filep->pipe->pipe_per_proc[i].pid==pid_parent) {
            index = i;
            break;
        }

    } 
    if(index==-1) return -EOTHERS; 

    // checking if the write end is already closed
    if(filep->pipe->pipe_per_proc[index].writeopen==0) return -EINVAL;

    // first, finding the available space
    int av_space = filep->pipe->pipe_global.av_space;
    int read_index = filep->pipe->pipe_global.read_index;
    int write_index = filep->pipe->pipe_global.write_index;
    char *pipe_buff = filep->pipe->pipe_global.pipe_buff;
    //if(write_index>=read_index) av_space = MAX_PIPE_SIZE-(write_index-read_index);
    //else av_space = read_index-write_index;

    // checking if count exceeds the available space
    if(count>av_space) count = av_space;
  
    // writing now
    int bytes_written = 0;
    while(bytes_written!=count) {
        pipe_buff[(write_index+1+bytes_written)%MAX_PIPE_SIZE] = buff[bytes_written];
        bytes_written++;
    }
    write_index = (write_index + bytes_written)%MAX_PIPE_SIZE;
    filep->pipe->pipe_global.write_index = write_index;
    filep->pipe->pipe_global.av_space-=bytes_written;

    // !!!! DEBUG
    //printk("Available space after write is %d\n",filep->pipe->pipe_global.av_space);
    // Return no of bytes written.
    return bytes_written;

}

// Function to create pipe.
int create_pipe (struct exec_context *current, int *fd) {

    /**
     *  TODO:: Implementation of Pipe Create
     *
     *  Find two free file descriptors. 
     *  Create two file objects for both ends by invoking the alloc_file() function. 
     *  Create pipe_info object by invoking the alloc_pipe_info() function and
     *       fill per process and global info fields.
     *  Fill the fields for those file objects like type, fops, etc.
     *  Fill the valid file descriptor in *fd param.
     *  On success, return 0.
     *  Incase of Error return valid Error code.
     *       -ENOMEM: If memory is not enough.
     *       -EOTHERS: Some other errors.
     *
     */

    //Begin:

    // Finding 2 free file descriptors(least index)
    int fd0 = 0;
    int fd1 = -1;
    int fd2 = -1;
    while(fd0 < MAX_OPEN_FILES)
    {
        if(!current->files[fd0])
        {
            fd1 = fd0;
            break;
        }
        fd0++;
    }
    fd0=fd1+1; // +1 is necessary, since current->files[fd1] will also give NULL abhi.
    while(fd0 < MAX_OPEN_FILES)
    {
        if(!current->files[fd0])
        {
            fd2 = fd0;
            break;
        }
        fd0++;
    }
    // error check:
    if((fd1*fd2)<0) return -EOTHERS; 

    // Creating 2 file objects for both ends of the pipe
    struct file *read_end = alloc_file();
    struct file *write_end = alloc_file();
    if(read_end==NULL || write_end==NULL) return -ENOMEM;
    current->files[fd1] = read_end;
    current->files[fd2] = write_end;

    // Create pipe_info object
    struct pipe_info * pipe_info_obj = alloc_pipe_info();
    if(pipe_info_obj == NULL) return -ENOMEM;

   


    //filling the fields for file objects
    read_end->type = PIPE;
    read_end->mode = O_READ;
    read_end->pipe = pipe_info_obj;
    read_end->fops->close = pipe_close;
    //read_end->fops->lseek;
    read_end->fops->write = pipe_write;
    read_end->fops->read = pipe_read;

    write_end->type = PIPE;
    write_end->mode = O_WRITE;
    write_end->pipe = pipe_info_obj;
    write_end->fops->close = pipe_close;
    //write_end->fops->lseek;
    write_end->fops->write = pipe_write;
    write_end->fops->read = pipe_read;    

    // filling the fd array
    fd[0] = fd1;
    fd[1] = fd2;

    // Simple return.
    return 0;

}
