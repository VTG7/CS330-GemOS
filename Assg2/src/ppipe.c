#include<ppipe.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>


// Per process information for the ppipe.
struct ppipe_info_per_process {

    // TODO:: Add members as per your need...
    int readopen; // whether the read end of the pipe, for that process is open or not.
    int writeopen;// whether the write end of the pipe, for that process is open or not.
    int pid; // stores the pid of the process.
    int read_index; //  stores the read pointer of that process
    int flush_to_read; // stores the flush pointer(global) to read pointer(process) distance
    int datasize; //stores the datasize left to read for that specific process  
};

// Global information for the ppipe.
struct ppipe_info_global {

    char *ppipe_buff;       // Persistent pipe buffer: DO NOT MODIFY THIS.

    // TODO:: Add members as per your need... /
    int num_process;    // number of processes associated with the pipe
    int flush_index;  // stores the flush pointer
    int write_global;   // the global write index  
    int avail_space_global; // the available space, on a global level
};

// Persistent pipe structure.
// NOTE: DO NOT MODIFY THIS STRUCTURE.
struct ppipe_info {

    struct ppipe_info_per_process ppipe_per_proc [MAX_PPIPE_PROC];
    struct ppipe_info_global ppipe_global;

};

int abs(int x) {    // helper function to find the absolute value.
    if(x<0)  return -x;
    else return x;
}
// Function to allocate space for the ppipe and initialize its members.
struct ppipe_info* alloc_ppipe_info() {

    // Allocate space for ppipe structure and ppipe buffer.
    struct ppipe_info *ppipe = (struct ppipe_info*)os_page_alloc(OS_DS_REG);
    char* buffer = (char*) os_page_alloc(OS_DS_REG);

    // Assign ppipe buffer.
    ppipe->ppipe_global.ppipe_buff = buffer;

    /**
     *  TODO:: Initializing pipe fields
     *
     *  Initialize per process fields for this ppipe.
     *  Initialize global fields for this ppipe.
     *
     */ 
   // Begin
    // filling per process and global info fields
    ppipe->ppipe_per_proc[0].readopen = 1;      
    ppipe->ppipe_per_proc[0].writeopen = 1; 
    struct exec_context * current = get_current_ctx();
    ppipe->ppipe_per_proc[0].pid = current->pid;
    ppipe->ppipe_global.num_process =0;   
    ppipe->ppipe_global.flush_index = MAX_PPIPE_SIZE-1;
    for(int index = 0;index<MAX_PPIPE_PROC;index++) {
        ppipe->ppipe_per_proc[index].read_index = -1;
        if(index) ppipe->ppipe_per_proc[index].pid = -1;
        ppipe->ppipe_per_proc[index].flush_to_read = 0;
        ppipe->ppipe_per_proc[index].datasize = 0; 
    }
    ppipe->ppipe_global.num_process++;
    ppipe->ppipe_global.write_global=-1;
    ppipe->ppipe_global.avail_space_global=MAX_PPIPE_SIZE;
    // Return the ppipe.
    return ppipe;

}

// Function to free ppipe buffer and ppipe info object.
// NOTE: DO NOT MODIFY THIS FUNCTION.
void free_ppipe (struct file *filep) {

    os_page_free(OS_DS_REG, filep->ppipe->ppipe_global.ppipe_buff);
    os_page_free(OS_DS_REG, filep->ppipe);

} 

// Fork handler for ppipe.
int do_ppipe_fork (struct exec_context *child, struct file *filep) {
    
    /**
     *  TODO:: Implementation for fork handler
     *
     *  You may need to update some per process or global info for the ppipe.
     *  This handler will be called twice since ppipe has 2 file objects.
     *  Also consider the limit on no of processes a ppipe can have.
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
    if(filep->ppipe->ppipe_global.num_process==MAX_PPIPE_PROC) return -EOTHERS;

    //pid of parent and child:
    struct exec_context *current = get_current_ctx();
    int pid_parent = current->pid;   
    int pid_child = child->pid;
    // !!!! DEBUG
    //printk("PPID through child %d \n",child->ppid);
   // printk("PPID through exec context %d \n",pid_parent);
    // index of parent
    int index_parent = -1;
    for(int i = 0;i<MAX_PPIPE_PROC;i++) {
        if(filep->ppipe->ppipe_per_proc[i].pid==pid_parent) {
            index_parent = i;
            break;
        }

    }
    if(index_parent==-1) return -EOTHERS;    // not necessary here though..
    // // !!!! DEBUG
    // printk("parent index inside fork is %d\n", index_parent);
    // now the idea is, if the pid_child is present in the per process info, that means that 
    // fork has been called once already, if not, then this is the first call
    int flag = 0;
    for(int i = 0;i<MAX_PPIPE_PROC;i++) {
        if(filep->ppipe->ppipe_per_proc[i].pid==pid_child) {
            // second call
            flag = 1;
            // // !!!! DEBUG
            // printk("Second call\n");
            if(filep->mode == O_READ){
                // read end hai
                filep->ppipe->ppipe_per_proc[i].readopen = filep->ppipe->ppipe_per_proc[index_parent].readopen; 
            }
            else if(filep->mode == O_WRITE) {
                // write end hai
                filep->ppipe->ppipe_per_proc[i].writeopen = filep->ppipe->ppipe_per_proc[index_parent].writeopen; 

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
    for(int i = 0;i<MAX_PPIPE_PROC;i++) {
        if(filep->ppipe->ppipe_per_proc[i].pid==-1) {
            //use this index to store the child'd pid now
            filep->ppipe->ppipe_per_proc[i].pid = pid_child;
            if(filep->mode == O_READ){
                // read end hai
                filep->ppipe->ppipe_per_proc[i].readopen = filep->ppipe->ppipe_per_proc[index_parent].readopen; 
                filep->ppipe->ppipe_per_proc[i].writeopen = 0; // this is necessary to be done on the first call
                filep->ppipe->ppipe_per_proc[i].read_index = filep->ppipe->ppipe_per_proc[index_parent].read_index; //indexes need to be copied
                filep->ppipe->ppipe_per_proc[i].flush_to_read = filep->ppipe->ppipe_per_proc[index_parent].flush_to_read;
                filep->ppipe->ppipe_per_proc[i].datasize = filep->ppipe->ppipe_per_proc[index_parent].datasize;

            }
            else if(filep->mode == O_WRITE) {
                // write end hai
                filep->ppipe->ppipe_per_proc[i].writeopen = filep->ppipe->ppipe_per_proc[index_parent].writeopen; 
                filep->ppipe->ppipe_per_proc[i].readopen = 0; // this is necessary to be done on the first call
                filep->ppipe->ppipe_per_proc[i].read_index = filep->ppipe->ppipe_per_proc[index_parent].read_index; // indexes need to be copied
                filep->ppipe->ppipe_per_proc[i].flush_to_read = filep->ppipe->ppipe_per_proc[index_parent].flush_to_read;
                filep->ppipe->ppipe_per_proc[i].datasize = filep->ppipe->ppipe_per_proc[index_parent].datasize;
            }
            else {
                return -EOTHERS;
            }
            filep->ppipe->ppipe_global.num_process++;
            break;
        }
    }
    // Return successfully.
    return 0;
   
}


// Function to close the ppipe ends and free the ppipe when necessary.
long ppipe_close (struct file *filep) {

    /**
     *  TODO:: Implementation of Pipe Close
     *
     *  Close the read or write end of the ppipe depending upon the file
     *      object's mode.
     *  You may need to update some per process or global info for the ppipe.
     *  Use free_pipe() function to free ppipe buffer and ppipe object,
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
    for(int i = 0;i<MAX_PPIPE_PROC;i++) {
        if(filep->ppipe->ppipe_per_proc[i].pid==pid_parent) {
            index = i;
            break;
        }

    } 
    if(index==-1) return -EOTHERS;   
    if(filep->mode == O_READ) {
        filep->ppipe->ppipe_per_proc[index].readopen = 0;
    }
    else if(filep->mode == O_WRITE){
        filep->ppipe->ppipe_per_proc[index].writeopen = 0;
    }
    else return -EOTHERS;
    if(filep->ppipe->ppipe_per_proc[index].readopen == 0 && filep->ppipe->ppipe_per_proc[index].writeopen == 0) {
        filep->ppipe->ppipe_global.num_process--;
        filep->ppipe->ppipe_per_proc[index].pid = -1; // basically, making this index available again...#### confirm it if that has to be done.
        if(filep->ppipe->ppipe_global.num_process ==0) free_ppipe(filep);
    }
    int ret_value;

    // Close the file.
    ret_value = file_close (filep);         // DO NOT MODIFY THIS LINE.

    // And return.
    return ret_value;

}

// Function to perform flush operation on ppipe.
int do_flush_ppipe (struct file *filep) {

    /**
     *  TODO:: Implementation of Flush system call
     *
     *  Reclaim the region of the persistent pipe which has been read by 
     *      all the processes.
     *  Return no of reclaimed bytes.
     *  In case of any error return -EOTHERS.
     *
     */

    // Begin


    // the current process calling flush must have atleast one open end...implement the same
    int index = -1;
    struct exec_context *current = get_current_ctx();
    int pid_parent = current->pid;
    for(int i = 0;i<MAX_PPIPE_PROC;i++) {
        if(filep->ppipe->ppipe_per_proc[i].pid==pid_parent) {
            index = i;
            break;
        }
    }
    if(index==-1) return -EOTHERS;

    int reclaimed_bytes = MAX_PPIPE_SIZE+1;
    for(int i = 0;i<MAX_PPIPE_PROC;i++) {
        if(filep->ppipe->ppipe_per_proc[i].pid==-1) continue;
        if(filep->ppipe->ppipe_per_proc[i].readopen==1) {
            if(filep->ppipe->ppipe_per_proc->flush_to_read < reclaimed_bytes) {
                reclaimed_bytes = filep->ppipe->ppipe_per_proc->flush_to_read;
            }
        }
    }
    if(reclaimed_bytes==MAX_PPIPE_SIZE+1) reclaimed_bytes = 0; // ek bhi valid process nahi hai to reclaim

    // Now for each process, going to update the flush_to_read parameter:
    for(int i = 0;i<MAX_PPIPE_PROC;i++) {
        if(filep->ppipe->ppipe_per_proc[i].pid==-1) continue;
        if(filep->ppipe->ppipe_per_proc[i].readopen==1) {
            filep->ppipe->ppipe_per_proc[i].flush_to_read-=reclaimed_bytes;
        }  
    }

    // Now updating the flush pointer:
    filep->ppipe->ppipe_global.flush_index = (filep->ppipe->ppipe_global.flush_index + reclaimed_bytes)%MAX_PPIPE_SIZE;
    
    // Now updating the avail_space_global
    filep->ppipe->ppipe_global.avail_space_global+=reclaimed_bytes; 
    // Return reclaimed bytes.
  
    return reclaimed_bytes;

}

// Read handler for the ppipe.
int ppipe_read (struct file *filep, char *buff, u32 count) {
    
    /**
     *  TODO:: Implementation of PPipe Read
     *
     *  Read the data from ppipe buffer and write to the provided buffer.
     *  If count is greater than the present data size in the ppipe then just read
     *      that much data.
     *  Validate file object's access right.
     *  On successful read, return no of bytes read.
     *  Incase of Error return valid error code.
     *      -EACCES: In case access is not valid.
     *      -EINVAL: If read end is already closed.
     *      -EOTHERS: For any other errors.
     *
     */
    //Begin:

    // checking if it is the read_end 
    if(filep->mode != O_READ) return -EACCES;
    
    int index = -1;  // the process index
    //pid  
    struct exec_context *current = get_current_ctx();
    int pid_parent = current->pid;   
    // index 
    for(int i = 0;i<MAX_PPIPE_PROC;i++) {
        if(filep->ppipe->ppipe_per_proc[i].pid==pid_parent) {
            index = i;
            break;
        }

    } 
    if(index==-1) return -EOTHERS;  

    // checking if the read end of the process is closed:
    if(filep->ppipe->ppipe_per_proc[index].readopen == 0) return -EINVAL;
      
    // data size available to be read for that process
    int datasize = filep->ppipe->ppipe_per_proc[index].datasize;
    int read_index = filep->ppipe->ppipe_per_proc[index].read_index;
    int write_index = filep->ppipe->ppipe_global.write_global;
    char *ppipe_buff = filep->ppipe->ppipe_global.ppipe_buff;
   // if(write_index>=read_index) datasize = write_index-read_index;
    //else datasize = MAX_PIPE_SIZE-(read_index-write_index);
      // !!!! DEBUG
       // printk("NOW READING with write index as %d and  read index as %d \n", write_index, read_index);  
    // count greater than the present data size check
    if(count>datasize) count = datasize;
    
    // reading now
    int bytes_read = 0;
    
    while(bytes_read!=count) {
           
        buff[bytes_read] = ppipe_buff[(read_index+1+bytes_read)%MAX_PPIPE_SIZE]; 
        bytes_read++;
    }
    
    read_index = (read_index+bytes_read)%MAX_PPIPE_SIZE;
    // !!!! DEBUG
    //printk("The pipe_buf stores %s\n", pipe_buff);
    filep->ppipe->ppipe_per_proc[index].read_index = read_index;
    filep->ppipe->ppipe_per_proc[index].datasize-=bytes_read;
    filep->ppipe->ppipe_per_proc[index].flush_to_read+=bytes_read;
    //filep->ppipe->ppipe_global.av_space += bytes_read;
    // Return no of bytes read.
    return bytes_read;
	
}

// Write handler for ppipe.
int ppipe_write (struct file *filep, char *buff, u32 count) {

    /**
     *  TODO:: Implementation of PPipe Write
     *
     *  Write the data from the provided buffer to the ppipe buffer.
     *  If count is greater than available space in the ppipe then just write
     *      data that fits in that space.
     *  Validate file object's access right.
     *  On successful write, return no of written bytes.
     *  Incase of Error return valid error code.
     *      -EACCES: In case access is not valid.
     *      -EINVAL: If write end is already closed.
     *      -EOTHERS: For any other errors.
     *
     */
    //Begin:
    // checking if it is the write_end 
    if(filep->mode != O_WRITE) return -EACCES;
    

    int index = -1;  // the process index
    //pid  
    struct exec_context *current = get_current_ctx();
    int pid_parent = current->pid;   
    // index 
    for(int i = 0;i<MAX_PPIPE_PROC;i++) {
        if(filep->ppipe->ppipe_per_proc[i].pid==pid_parent) {
            index = i;
            break;
        }
    } 
    if(index==-1) return -EOTHERS; 
    // checking if the write end is already closed
    if(filep->ppipe->ppipe_per_proc[index].writeopen==0) return -EINVAL;

     // first, finding the available space
    int av_space = filep->ppipe->ppipe_global.avail_space_global;
    int write_index = filep->ppipe->ppipe_global.write_global;
    
    char *ppipe_buff = filep->ppipe->ppipe_global.ppipe_buff;
    //if(write_index>=read_index) av_space = MAX_PIPE_SIZE-(write_index-read_index);
    //else av_space = read_index-write_index;

    // checking if count exceeds the available space
    if(count>av_space) count = av_space;
  
    // writing now
    int bytes_written = 0;
    while(bytes_written!=count) {
        ppipe_buff[(write_index+1+bytes_written)%MAX_PPIPE_SIZE] = buff[bytes_written];
        bytes_written++;
    }
    write_index = (write_index + bytes_written)%MAX_PPIPE_SIZE;
    filep->ppipe->ppipe_global.write_global = write_index;
    filep->ppipe->ppipe_global.avail_space_global-=bytes_written;

    // Now, we have to increase the datasize parameter of each of the existing processes:
    for(int i = 0;i<MAX_PPIPE_PROC;i++) {
        if(filep->ppipe->ppipe_per_proc[i].pid!=-1) {
           filep->ppipe->ppipe_per_proc[i].datasize+=bytes_written; 
        }
    }

    // !!!! DEBUG
    //printk("Available space after write is %d\n",filep->pipe->pipe_global.av_space);
    // Return no of bytes written.
    return bytes_written;   

}

// Function to create persistent pipe.
int create_persistent_pipe (struct exec_context *current, int *fd) {

    /**
     *  TODO:: Implementation of PPipe Create
     *
     *  Find two free file descriptors.
     *  Create two file objects for both ends by invoking the alloc_file() function.
     *  Create ppipe_info object by invoking the alloc_ppipe_info() function and
     *      fill per process and global info fields.
     *  Fill the fields for those file objects like type, fops, etc.
     *  Fill the valid file descriptor in *fd param.
     *  On success, return 0.
     *  Incase of Error return valid Error code.
     *      -ENOMEM: If memory is not enough.
     *      -EOTHERS: Some other errors.
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
    struct ppipe_info * ppipe_info_obj = alloc_ppipe_info();
    if(ppipe_info_obj == NULL) return -ENOMEM;

   


    //filling the fields for file objects
    read_end->type = PPIPE;
    read_end->mode = O_READ;
    read_end->ppipe = ppipe_info_obj;
    read_end->fops->close = ppipe_close;
    //read_end->fops->lseek;
    read_end->fops->write = ppipe_write;
    read_end->fops->read = ppipe_read;

    write_end->type = PPIPE;
    write_end->mode = O_WRITE;
    write_end->ppipe = ppipe_info_obj;
    write_end->fops->close = ppipe_close;
    //write_end->fops->lseek;
    write_end->fops->write = ppipe_write;
    write_end->fops->read = ppipe_read;    

    // filling the fd array
    fd[0] = fd1;
    fd[1] = fd2;

    // Simple return.
    return 0;
}
