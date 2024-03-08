#include <linux/syscalls.h>

#include "jiachenghack.h"

static int jiacheng_main_thread(void * params) {
	while(1) {
		// struct task_struct *task = &init_task;
		// struct mm_struct *mm = NULL;
		// struct vm_area_struct *p;
		// for_each_process(task) {
		// 	printk("pid: %lu", task->pid);
		// 	// mm = get_task_mm(task);
		// 	mm = task->mm;
		// 	if (!mm) continue;
		// 	struct vm_area_struct *mmap = mm->mmap;
		// 	for (p = mmap; ;) {
		// 		// print
		// 		printk("[%lu, %lu]", p->vm_start, p->vm_end);
		// 		p = p->vm_next;
		// 		if (p == mmap) break;
		// 	}
			
		// }

		msleep(5 * 1000);
	}
	return 0;
}


void jiacheng_hello(void) {
    printk("this is jiacheng_hello from jiacheng.c\n");
}

void jiacheng_main(void) {
	struct task_struct *tsk;
	int pid;
	
	pr_info("jiacheng hack start ---> 20210922 \n");

	pid = kernel_thread(jiacheng_main_thread, NULL, CLONE_FS | CLONE_FILES);

	rcu_read_lock();
	tsk = find_task_by_pid_ns(pid, &init_pid_ns);
	set_cpus_allowed_ptr(tsk, cpumask_of(smp_processor_id()));
	rcu_read_unlock();
}

// Implementation sys_jiacheng_printk System Call
SYSCALL_DEFINE2(jiacheng_printk, const char __user *, info, unsigned long, size) {
	char buffer[size];
	if(copy_from_user(buffer, info, size)) {
		return -EFAULT;
	}
	printk(buffer);
	return 0;
}

// 实现/proc/runtime_write
static ssize_t runtime_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
	char buffer[100];

	printk("jiacheng  jiacheng_write start\n");
	if(copy_from_user(buffer, buf, count)) {
		return -EFAULT;		
	}
	printk(buffer);
	
	jiacheng_hello();
	printk("jiacheng jiacheng_write end\n");

	return count;
}


const struct file_operations proc_runtime_write_operations = {
	.llseek	= noop_llseek,
	.write = runtime_write
};

// 实现/proc/runtime_read
static ssize_t runtime_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	char buffer[100];
	loff_t len = 0;
	size_t ret = 0, copied = 0;

	snprintf(buffer, sizeof(buffer), "This is jiacheng_read.\n");

	len = min(count, strlen(buffer));
	if (*ppos > 100) {
		goto out;
	}
	if(copy_to_user(buf, buffer, len)) {
		printk("jiachenghack.c 95 jiacheng_read --> EFAULT");
		ret = -EFAULT;
		goto out;
	}
	copied += len;
	*ppos += len;
out:
	return copied;
}

const struct file_operations proc_runtime_read_operations = {
	.read		= runtime_read,
};
