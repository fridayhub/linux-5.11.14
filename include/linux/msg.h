/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_MSG_H
#define _LINUX_MSG_H

#include <linux/list.h>
#include <uapi/linux/msg.h>

/* one msg_msg structure for each message */
struct msg_msg {
	struct list_head m_list; // 8 8
	long m_type; // 8
	size_t m_ts;	// 8	/* message text size */
	struct msg_msgseg *next; // 8
	void *security; // 8
	/* the actual message follows immediately */
};

#endif /* _LINUX_MSG_H */
