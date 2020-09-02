/*
 *  zero_page_refcount.c -  view zero_page reference counter in real time
 *  with the proc filesystem.
 *
 *  Author: Matthew Ruffell <matthew.ruffell@canonical.com>
 *
 * Steps:
 *
 * $ sudo apt-get -y install gcc make libelf-dev linux-headers-$(uname -r)
 *
 * cat <<EOF >Makefile
obj-m=zero_page_refcount.o
KVER=\$(shell uname -r)
MDIR=\$(shell pwd)
default:
$(echo -e '\t')make -C /lib/modules/\$(KVER)/build M=\$(MDIR) modules
clean:
$(echo -e '\t')make -C /lib/modules/\$(KVER)/build M=\$(MDIR) clean
EOF
 *
 * $ make
 * $ sudo insmod zero_page_refcount.ko
 * # To display current zero_page reference count:
 * $ cat /proc/zero_page_refcount
 * # To set zero_page reference count to near overflow:
 * $ cat /proc/zero_page_refcount_set
 */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/atomic.h>
#include <asm/pgtable.h>

static int zero_page_refcount_show_set(struct seq_file *m, void *v) {
  struct page *page = virt_to_page(empty_zero_page);
  atomic_set(&page->_refcount, 0xFFFF7FFFFF00);
  seq_printf(m, "Zero Page Refcount set to 0x1FFFFFFFFF000\n");
  return 0;
}

static int zero_page_refcount_open_set(struct inode *inode, struct  file *file) {
  return single_open(file, zero_page_refcount_show_set, NULL);
}

static const struct file_operations zero_page_refcount_set_fops = {
  .owner = THIS_MODULE,
  .open = zero_page_refcount_open_set,
  .read = seq_read,
  .llseek = seq_lseek,
  .release = single_release,
};

static int zero_page_refcount_show(struct seq_file *m, void *v) {
  struct page *page = virt_to_page(empty_zero_page);
  int reference_count = atomic_read(&page->_refcount);
  seq_printf(m, "Zero Page Refcount: 0x%x or %d\n", reference_count, reference_count);
  return 0;
}

static int zero_page_refcount_open(struct inode *inode, struct  file *file) {
  return single_open(file, zero_page_refcount_show, NULL);
}

static const struct file_operations zero_page_refcount_fops = {
  .owner = THIS_MODULE,
  .open = zero_page_refcount_open,
  .read = seq_read,
  .llseek = seq_lseek,
  .release = single_release,
};

static int __init zero_page_refcount_init(void) {
  proc_create("zero_page_refcount", 0, NULL, &zero_page_refcount_fops);
  proc_create("zero_page_refcount_set", 0, NULL, &zero_page_refcount_set_fops);
  return 0;
}

static void __exit zero_page_refcount_exit(void) {
  remove_proc_entry("zero_page_refcount", NULL);
  remove_proc_entry("zero_page_refcount_set", NULL);
}

MODULE_LICENSE("GPL");
module_init(zero_page_refcount_init);
module_exit(zero_page_refcount_exit);
