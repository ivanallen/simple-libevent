/*
 * Copyright 2000 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _EVENT_H_
#define _EVENT_H_

// 下面四个宏表示事件是否在对应的队列中
#define EVLIST_TIMEOUT	0x01
#define EVLIST_READ	0x02
#define EVLIST_WRITE	0x04
#define EVLIST_ADD	0x08
#define EVLIST_INIT	0x80

#define EV_TIMEOUT	EVLIST_TIMEOUT
#define EV_READ		EVLIST_READ
#define EV_WRITE	EVLIST_WRITE

struct event {
  TAILQ_ENTRY (event) ev_read_next;
  TAILQ_ENTRY (event) ev_write_next;
  TAILQ_ENTRY (event) ev_timeout_next;
  TAILQ_ENTRY (event) ev_add_next;

  int ev_fd; // 要监听的事件描述符
  short ev_events; // 要监听什么事件

  struct timeval ev_timeout; // 超时时间

  void (*ev_callback)(int, short, void *arg); // 事件处理函数
  void *ev_arg; // 事件处理函数的参数

  int ev_flags; // 标志位，用来指示事件是否在某个队列中
};

#define TIMEOUT_DEFAULT	5

void event_init(void); // 初始化，实际上就初始化了几个队列
int event_dispatch(void); // 事件派遣函数

int timeout_next(struct timeval *); // 计算超时时间
void timeout_process(void); // 处理超时队列中过期的事件

#define timeout_add(ev, tv)		event_add(ev, tv)
#define timeout_set(ev, cb, arg)	event_set(ev, -1, 0, cb, arg)
#define timeout_del(ev)			event_del(ev)
#define timeout_pending(ev, tv)		event_pending(ev, EV_TIMEOUT, tv)
#define timeout_initalized(ev)		((ev)->ev_flags & EVLIST_INIT)

void event_set(struct event *, int, short, void (*)(int, short, void *), void *); // 初始化事件对象
void event_add(struct event *, struct timeval *); // 将事件添加到对应的队列中
void event_del(struct event *); // 将事件从所有队列移除

int event_pending(struct event *, short, struct timeval *);

#define event_initalized(ev)		((ev)->ev_flags & EVLIST_INIT)

#endif /* _EVENT_H_ */
