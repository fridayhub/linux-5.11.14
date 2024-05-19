// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * linux/ipc/msgutil.c
 * Copyright (C) 1999, 2004 Manfred Spraul
 */

#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/security.h>
#include <linux/slab.h>
#include <linux/ipc.h>
#include <linux/msg.h>
#include <linux/ipc_namespace.h>
#include <linux/utsname.h>
#include <linux/proc_ns.h>
#include <linux/uaccess.h>
#include <linux/sched.h>

#include "util.h"

DEFINE_SPINLOCK(mq_lock);

/*
 * The next 2 defines are here bc this is the only file
 * compiled when either CONFIG_SYSVIPC and CONFIG_POSIX_MQUEUE
 * and not CONFIG_IPC_NS.
 */
struct ipc_namespace init_ipc_ns = {
	.ns.count = REFCOUNT_INIT(1),
	.user_ns = &init_user_ns,
	.ns.inum = PROC_IPC_INIT_INO,
#ifdef CONFIG_IPC_NS
	.ns.ops = &ipcns_operations,
#endif
};

struct msg_msgseg {
	struct msg_msgseg *next;
	/* the next part of the message follows immediately */
};

#define DATALEN_MSG	((size_t)PAGE_SIZE-sizeof(struct msg_msg)) // msg_msg 一页中分配的用户数据大小
#define DATALEN_SEG	((size_t)PAGE_SIZE-sizeof(struct msg_msgseg)) // msg_msgseg 一页中分配的用户数据大小


static struct msg_msg *alloc_msg(size_t len)
{
	struct msg_msg *msg;
	struct msg_msgseg **pseg; // 单链表的header，始终指向最后一个元素的next地址
	size_t alen;

	alen = min(len, DATALEN_MSG); // 确定msg_msg 中分配给用户data的大小
	msg = kmalloc(sizeof(*msg) + alen, GFP_KERNEL_ACCOUNT);
	if (msg == NULL)
		return NULL;

	msg->next = NULL;
	msg->security = NULL;

	len -= alen; // 计算剩余用户data的大小
	pseg = &msg->next;
	while (len > 0) {
		struct msg_msgseg *seg;

		cond_resched();

		alen = min(len, DATALEN_SEG);
		seg = kmalloc(sizeof(*seg) + alen, GFP_KERNEL_ACCOUNT);
		if (seg == NULL)
			goto out_err;
		*pseg = seg;  // 将header中的next指向新分配的 msg_msgseg
		seg->next = NULL;
		pseg = &seg->next; // 更新pseg
		len -= alen;
	}

	return msg;

out_err:
	free_msg(msg);
	return NULL;
}

struct msg_msg *load_msg(const void __user *src, size_t len)
{
	struct msg_msg *msg;
	struct msg_msgseg *seg;
	int err = -EFAULT;
	size_t alen;

	msg = alloc_msg(len);
	if (msg == NULL)
		return ERR_PTR(-ENOMEM);

	alen = min(len, DATALEN_MSG);
	if (copy_from_user(msg + 1, src, alen)) // 拷贝数据，紧贴msg结构体之后
		goto out_err;

	for (seg = msg->next; seg != NULL; seg = seg->next) {
		len -= alen;
		src = (char __user *)src + alen;
		alen = min(len, DATALEN_SEG);
		if (copy_from_user(seg + 1, src, alen))
			goto out_err;
	}

	err = security_msg_msg_alloc(msg);
	if (err)
		goto out_err;

	return msg;

out_err:
	free_msg(msg);
	return ERR_PTR(err);
}
#ifdef CONFIG_CHECKPOINT_RESTORE
struct msg_msg *copy_msg(struct msg_msg *src, struct msg_msg *dst)
{
	struct msg_msgseg *dst_pseg, *src_pseg;
	size_t len = src->m_ts;
	size_t alen;
//信息泄漏： 若是我们能够控制一个 msg_msg 的 header，将其 m_sz 成员改为一个较大的数，我们就能够越界读取出最多将近一张内存页大小的数据
	if (src->m_ts > dst->m_ts) // message text size 检查
		return ERR_PTR(-EINVAL);

	alen = min(len, DATALEN_MSG);
	memcpy(dst + 1, src + 1, alen); // +1 是为了跳过 结构体部分

	for (dst_pseg = dst->next, src_pseg = src->next;
	     src_pseg != NULL;
	     dst_pseg = dst_pseg->next, src_pseg = src_pseg->next) {

		len -= alen;
		alen = min(len, DATALEN_SEG);
		memcpy(dst_pseg + 1, src_pseg + 1, alen);
	}

	dst->m_type = src->m_type;
	dst->m_ts = src->m_ts;

	return dst;
}
#else
struct msg_msg *copy_msg(struct msg_msg *src, struct msg_msg *dst)
{
	return ERR_PTR(-ENOSYS);
}
#endif
int store_msg(void __user *dest, struct msg_msg *msg, size_t len)
{
	size_t alen;
	struct msg_msgseg *seg;

	alen = min(len, DATALEN_MSG);
	if (copy_to_user(dest, msg + 1, alen))
		return -1;

	for (seg = msg->next; seg != NULL; seg = seg->next) {
		len -= alen;
		dest = (char __user *)dest + alen;
		alen = min(len, DATALEN_SEG);
		if (copy_to_user(dest, seg + 1, alen))
			return -1;
	}
	return 0;
}

void free_msg(struct msg_msg *msg)
{
	struct msg_msgseg *seg;

	security_msg_msg_free(msg);

	seg = msg->next;
	kfree(msg);
	while (seg != NULL) {
		struct msg_msgseg *tmp = seg->next;

		cond_resched();
		kfree(seg);
		seg = tmp;
	}
}
