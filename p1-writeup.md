# Explain the pros and cons of using a FIFO queue (as compared to a hash table) for the global semaphore list.

A FIFO (First in First Out) Queue will schedule, in this context, processes, to run based on the order in which the processes enter the queue. This is good to prevent starvation from occuring, which is when a process waits inside of the queue indefinitely or forever. Since the processes will run in order of entering the queue, each process is expected to run and will never get stuck waiting. 

On the other hand, there is no implementation for prioritizing processes. A FIFO queue in this context may increase the probability of a priority inversion occuring, in turn wasting CPU time. Since processes are running based on their entry into the queue, lower priority processes may get to run before more important/higher priority processes. A lower priority process may block a higher priority process from running.
 
