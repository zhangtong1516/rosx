/* RosX RT-Kernel
 * Copyright (C) 2016 Arul Bose<bose.arul@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <RosX.h>

struct tasklet * sys_tasklet_list = NULL;
/* Under construction */

int rx_init_tasklet(struct tasklet *t, void(*func)(unsigned long), unsigned long data)
{
    if((!t) || (!func) ){
        return OS_ERR;
    }

    t->status = __RX_DISABLE_TASKLET;
    t->func = func;
    t->data = data;
    t->next = NULL;
    t->prev = NULL;
  
    return OS_OK;

}

/* Enable the tasklet if it is disabled */
void rx_enable_tasklet(struct tasklet *t)
{
    unsigned int imask;

    if(!t) {
        return;
    }

    imask = rx_enter_critical();
    if(t->status == __RX_DISABLE_TASKLET) {
        t->status = __RX_ENABLE_TASKLET;
    }
    rx_exit_critical(imask);

}
/* Disable the tasklet if enabled; In case 
 * it is already scheduled remove from the global task list
 * and then disable; If tasklet is running than it will complete 
 * and will get disabled */
void rx_disable_tasklet(struct tasklet *t)
{
    unsigned int imask;

    if(!t) {
        return;
    }

    imask = rx_enter_critical();
    if(t->status != __RX_DISABLE_TASKLET){
        if(t->status & __RX_SCHED_TASKLET) {
          /* Remove the task from the global tasklet list */
            if(!(t->prev) && !(t->next)) {
                sys_tasklet_list = NULL;
            }else{
                if(t->prev){
                    /* In between */
                    t->prev->next = t->next;
                }else {
                    /* In head */
                    sys_tasklet_list = t->next;
                }

                if(t->next){
                    t->next->prev = t->prev;
                }
            }
            t->next = NULL; 
            t->prev = NULL;
        }
        /* If the tasklet is already running then it will run and gets disabled */
        t->status = __RX_DISABLE_TASKLET;
    }
    rx_exit_critical(imask);

}

/* 1. When called schedule tasklet will be added to the global system tasklist to be processed
   2. Tasklet can be schedule multiple times but runs only once if it is not already running.
   3. A schedule call when a taslet is already running will run again after completion
   4. If the tasklet is disabled while running it will complete its run and then gets disabled
*/
void rx_schedule_tasklet(struct tasklet *t)
{
    struct tasklet *ride;
    unsigned int imask;

    if(!t) {
        return;
    }

    imask = rx_enter_critical();
    if(!(t->status & __RX_SCHED_TASKLET) && ((t->status & __RX_ENABLE_TASKLET))) {
        t->status |= __RX_SCHED_TASKLET;
        /* Add the tasklet to the global tasklet list */
        if(!sys_tasklet_list) {
            sys_tasklet_list = t;	
        }else{
            ride = sys_tasklet_list;
            while(ride->next){
                ride = ride->next;
            }
            ride->next = t;
            t->prev = ride;
        }
    }
	  
    rx_exit_critical(imask);
}

/* Process the tasklets */
void rx_bh_thread()
{
    struct tasklet *run;
    unsigned int imask;

    RX_DEFINE_WAITQUEUE(w);

    while(1) {

         if(0 == rx_wait_queue(&w, (sys_tasklet_list != NULL))) {
             /* Process the tasklet serially */
             imask = rx_enter_critical();
             run = sys_tasklet_list;
             sys_tasklet_list = sys_tasklet_list->next;
             run->status |= __RX_RUNNING_TASKLET;
             run->status &= ~(__RX_SCHED_TASKLET);
             rx_exit_critical(imask);
             
             /* Should run without sleeping with interrupts enabled */
             run->func(run->data);

             imask = rx_enter_critical();
             run->status &= ~(__RX_RUNNING_TASKLET);
             rx_exit_critical(imask);
         }
    }
}
