#ifndef _LINUX_JIACHENG_H
#define _LINUX_JIACHENG_H

void jiacheng_hello(void);
void jiacheng_main(void);

void jiacheng_page_range_cold(struct vm_area_struct *vma, unsigned long start, unsigned long size);


extern const struct file_operations proc_runtime_write_operations;
extern const struct file_operations proc_runtime_read_operations;

#endif