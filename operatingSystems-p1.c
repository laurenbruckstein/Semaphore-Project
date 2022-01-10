#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/stddef.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/cs1550.h> 

/* To make sure each sem has a unique id */
int id_counter = 0; 

/* Initialize system wide RWLOCK --> creates a variable not a pointer */
static DEFINE_RWLOCK(sem_rwlock); 

/* Initialize a global semaphore list */
static LIST_HEAD(sem_list); 
 
/**
 * Creates a new semaphore. The long integer value is used to
 * initialize the semaphore's value.
 *
 * The initial `value` must be greater than or equal to zero.
 *
 * On success, returns the identifier of the created
 * semaphore, which can be used with up() and down().
 *
 * On failure, returns -EINVAL or -ENOMEM, depending on the
 * failure condition.  
 */     
SYSCALL_DEFINE1(cs1550_create, long, value)      
{   
    /* Initial value must be greater than or equal to zero */
    if(!(value >= 0)){
        return -EINVAL;
    }

    /* Create a new semaphore */
    struct cs1550_sem *sem = kmalloc(sizeof(struct cs1550_sem), GFP_ATOMIC); 

    /* If sem == NULL, the sem_id passed to this syscall was not found in the sem_list */
    if(sem == NULL){
        return -ENOMEM;
    }

    /* Initialize the new semaphores' structure variables */
    sem->value = value;
    sem->sem_id = id_counter;
    INIT_LIST_HEAD(&sem->list); 
    INIT_LIST_HEAD(&sem->waiting_tasks);  
    spin_lock_init(&sem->lock);

    /* TESTING --> printk(KERN_WARNING "In create, semID=%d .\n", sem->sem_id); */

    /* Allocate and Initialize the current process's task structure */
    struct cs1550_task *task_node = kmalloc(sizeof(struct cs1550_task), GFP_ATOMIC);
    INIT_LIST_HEAD(&task_node->list);

    /* Surround protected regions, use write_lock() when adding/deleting from sem_list */
    write_lock(&sem_rwlock); 

        /* Incremenet the gloabl variable id_counter for the next sem_id */ 
         id_counter += 1;

        /* Add sem to the list of semaphores */ 
        list_add(&sem->list, &sem_list);

    write_unlock(&sem_rwlock);

    /* Return the id of the new semaphore */
    return sem->sem_id;

}    
     
/**     
 * Performs the down() operation on an existing semaphore
 * using the semaphore identifier obtained from a previous call
 * to cs1550_create().  
 *
 * This decrements the value of the semaphore, and *may cause* the
 * calling process to sleep (if the semaphore's value goes below 0)
 * until up() is called on the semaphore by another process.
 * 
 * Returns 0 when successful, or -EINVAL or -ENOMEM if an error
 * occurred.  
 */
SYSCALL_DEFINE1(cs1550_down, long, sem_id) 
{  
    /* Surround protected regions, use read_lock() when traversing sem_list */
    read_lock(&sem_rwlock); 

    /* Search for the sem_list to find the sem_id passed in the parameters */
    struct cs1550_sem *sem = NULL;
    list_for_each_entry(sem, &sem_list, list){
        /* If the current sem_id matches the sem_id from the syscall's parameter */
        if(sem->sem_id == sem_id){  
            /* The sem_id from parameters has been found, exit the traversal loop */
            break;
        }         
    }  

    /* If sem == NULL, the sem_id passed to this syscall was not found in the sem_list */
    if(sem == NULL){ 
        return -EINVAL;  
    }  

    /* TESTING --> printk(KERN_WARNING "cs1550_down syscall: pid=%d entered down.\n", current->pid);*/
    
    /* Surround protected regions, use spin_lock() when decrementing sem->value and accessing FIFO queue */
    spin_lock(&sem->lock);

    /* Decrement sem->value */
    sem->value = sem->value - 1;  
 
    /* TESTING --> printk(KERN_WARNING "semValue=%d .\n", sem->value); */

    /* If the semaphore value is less than 0, no more resources are present --> put process to sleep */
    if(sem->value < 0){           

        /* Allocate and initialize a task entry */
        struct cs1550_task *task_node = kmalloc(sizeof(struct cs1550_task), GFP_ATOMIC);
        
        /* Insert a task entry to the queue of waiting tasks */
        list_add_tail(&task_node->list, &sem->waiting_tasks);

        /* Save pointers to the current running process */
        task_node->task = current;

        /* Change the current process's state to sleep */
        set_current_state(TASK_INTERRUPTIBLE);

        spin_unlock(&sem->lock);
        
        /* Call the scheduler to schedule the next process to run */
        schedule();
    }
    else{ 
        /* If the semaphore value is >= 0, there are still resources present. The down() operation is complete. */
        spin_unlock(&sem->lock);     
    }

    /* TESTING --> printk(KERN_WARNING "cs1550_down syscall: pid=%d leaving down.\n", current->pid);*/

    read_unlock(&sem_rwlock); 

    /* Return 0 upon success */
    return 0;
}
 
/**
 * Performs the up() operation on an existing semaphore
 * using the semaphore identifier obtained from a previous call
 * to cs1550_create().
 *
 * This increments the value of the semaphore, and *may cause* the
 * calling process to wake up a process waiting on the semaphore,
 * if such a process exists in the queue.  
 *
 * Returns 0 when successful, or -EINVAL if the semaphore ID is
 * invalid.
 */
SYSCALL_DEFINE1(cs1550_up, long, sem_id)
{  
    /* Surround protected regions, use read_lock() when traversing sem_list */  
    read_lock(&sem_rwlock);    

    /* Search for the sem_list to find the sem_id passed in the parameters */
    struct cs1550_sem *sem = NULL;
    list_for_each_entry(sem, &sem_list, list){
        /* If the current sem_id matches the sem_id from the syscall's parameter */
        if(sem->sem_id == sem_id){  
            /* The sem_id from parameters has been found, exit the traversal loop */
            break;
        }         
    }  

    /* If sem == NULL, the sem_id passed to this syscall was not found in the sem_list */ 
    if(sem == NULL){ 
        return -EINVAL; 
    }   

    /*TESTING --> printk(KERN_WARNING "cs1550_up syscall: pid=%d entered up.\n", current->pid);*/

    /* Surround protected regions, use spin_lock() when decrementing sem->value and accessing FIFO queue */
    spin_lock(&sem->lock);

    /* Increment sem->value */
    sem->value = sem->value + 1;  

    /* TESTING --> printk(KERN_WARNING "semValue=%d .\n", sem->value); */
 
    /* If the sem->value is <= 0, proceed. */
    if(sem->value <= 0){  

        /* If there are processes waiting, proceed. */
        if(!list_empty(&sem->waiting_tasks)){

            /* Get a pointer to the first entry in the list */
            struct cs1550_task *head = list_first_entry(&sem->waiting_tasks, struct cs1550_task, list);

            /* Delete the first entry from the list */
            list_del(&head->list); 

            /* Wake up a process from the FIFO queue that was put to sleep in down() */
            wake_up_process(head->task);
          
            /* Free up the space on the kernel that head was taking up */
            kfree(head);
        }
    }

    /* TESTING --> printk(KERN_WARNING "cs1550_up syscall: pid=%d leaving up.\n", current->pid); */

    spin_unlock(&sem->lock);

    read_unlock(&sem_rwlock);

    /* Return 0 upon success */
    return 0;    
}

/** 
 * Removes an already-created semaphore from the system-wide
 * semaphore list using the identifier obtained from a previous
 * call to cs1550_create().
 *
 * Returns 0 when successful or -EINVAL if the semaphore ID is
 * invalid or the semaphore's process queue is not empty.
 */
SYSCALL_DEFINE1(cs1550_close, long, sem_id)
{ 

    /* Surround protected regions, use write_lock() when adding/deleting from sem_list */
    write_lock(&sem_rwlock);

    /* Search for the sem_list to find the sem_id passed in the parameters */
    struct cs1550_sem *sem = NULL;
    list_for_each_entry(sem, &sem_list, list){
        /* If the current sem_id matches the sem_id from the syscall's parameter */
        if(sem->sem_id == sem_id){  
            /* The sem_id from parameters has been found, exit the traversal loop */
            break;
        }         
    }  

    /* If sem == NULL, the sem_id passed to this syscall was not found in the sem_list */ 
    if(sem == NULL){ 
        return -EINVAL; 
    }  

    /* Surround protected regions, use spin_lock() when decrementing sem->value and accessing FIFO queue */
    spin_lock(&sem->lock); 

    /* If there are no processes waiting, proceed. */
    if(list_empty(&sem->waiting_tasks)){

        /* Delete the found semaphore from the system_wide list */  
        list_del(&sem->list);

        /* Unlock the spinlock() */
        spin_unlock(&sem->lock); 

        /* Free the allocated space that the deleted semaphore took up */
        kfree(sem); 

    }
    else{
        /* If there are processes waiting, you cannot close() */

        /* Unlock the spinlock() */
        spin_unlock(&sem->lock); 

        /* return since the semaphores process queue is not empty */
        return -EINVAL;
    }

    write_unlock(&sem_rwlock);    

    /* Return 0 unpon success */
    return 0;
} 
