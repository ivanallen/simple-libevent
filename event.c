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
#include <sys/types.h>
#include <sys/time.h>
//#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#ifdef USE_LOG
#include "log.h"
#else
#define LOG_DBG(x)
#define log_error(x)	perror(x)
#endif

#include "queue.h"
#include "event.h"

#ifndef howmany
#define        howmany(x, y)   (((x)+((y)-1))/(y))
#endif

/* Prototypes */
void event_add_post(struct event *);

TAILQ_HEAD (timeout_list, event) timequeue;
TAILQ_HEAD (event_wlist, event) writequeue;
TAILQ_HEAD (event_rlist, event) readqueue;
TAILQ_HEAD (event_ilist, event) addqueue;

int event_inloop = 0;
int event_fds;		/* Highest fd in fd set */
int event_fdsz;
fd_set *event_readset;
fd_set *event_writeset;

void
event_init(void)
{
	TAILQ_INIT(&timequeue);
	TAILQ_INIT(&writequeue);
	TAILQ_INIT(&readqueue);
	TAILQ_INIT(&addqueue);
}

/*
 * Called with the highest fd that we know about.  If it is 0, completely
 * recalculate everything.
 */

// 这个函数，主要用来调整读写集合的大小，参数 max 表示最大的描述符
int
events_recalc(int max)
{
	fd_set *readset, *writeset;
	struct event *ev;
	int fdsz;

	event_fds = max;

  // 找到最大的描述符
	if (!event_fds) {
		TAILQ_FOREACH(ev, &writequeue, ev_write_next)
			if (ev->ev_fd > event_fds)
				event_fds = ev->ev_fd;

		TAILQ_FOREACH(ev, &readqueue, ev_read_next)
			if (ev->ev_fd > event_fds)
				event_fds = ev->ev_fd;
	}

  // NFDBITS = 32， sizeof(fd_mask) = 4
  // howmany 表示这些描述符存到 fd_set 需要占用几个 4 字节
  // fdsz 表示占用几个字节. fd_mask 实际上是一个 int 类型
	fdsz = howmany(event_fds + 1, NFDBITS) * sizeof(fd_mask);
  // 当前描述符容量不够了，才会调整大小
	if (fdsz > event_fdsz) {
    // 重新调整 event_readset 的大小
		if ((readset = realloc(event_readset, fdsz)) == NULL) {
			log_error("malloc");
			return (-1);
		}

		if ((writeset = realloc(event_writeset, fdsz)) == NULL) {
			log_error("malloc");
			free(readset);
			return (-1);
		}

    // 清空读写集合
		memset(readset + event_fdsz, 0, fdsz - event_fdsz);
		memset(writeset + event_fdsz, 0, fdsz - event_fdsz);

		event_readset = readset;
		event_writeset = writeset;
		event_fdsz = fdsz;
	}

	return (0);
}

/*
 * 1. 计算超时时间最小的那个超时时间（超时队列队头的那个事件）
 * 2. select 监听读写事件，超时时间就是第 1 步计算出来的
 * 3. 处理读写事件
 * 4. 将 add 队列中未处理的描述符添加到读写集合 
 */
int
event_dispatch(void)
{
	struct timeval tv;
	struct event *ev, *old;
	int res, maxfd;

	/* Calculate the initial events that we are waiting for */
  // 初始化读写集合的大小
	if (events_recalc(0) == -1)
		return (-1);

	while (1) {
    // 清空描述符集合
		memset(event_readset, 0, event_fdsz);
		memset(event_writeset, 0, event_fdsz);

    // 将读定队列中的事件添加到对应的读写集合中
		TAILQ_FOREACH(ev, &writequeue, ev_write_next)
				FD_SET(ev->ev_fd, event_writeset);

		TAILQ_FOREACH(ev, &readqueue, ev_read_next)
				FD_SET(ev->ev_fd, event_readset);

    // 计算超时队列队头事件还有多久超时
    // 如果超时队列为空，则 tv 设置成 5 秒
		timeout_next(&tv);
    printf("time remain: %lu s : %lu ms\n", tv.tv_sec, 1000*tv.tv_usec);

    // 监听读写事件集合，超时时间为最小的那个事件的时间
		if ((res = select(event_fds + 1, event_readset, 
				  event_writeset, NULL, &tv)) == -1) {
			if (errno != EINTR) {
				log_error("select");
				return (-1);
			}
			continue;
		}

		LOG_DBG((LOG_MISC, 80, __FUNCTION__": select reports %d",
			 res));

		maxfd = 0;
    // 事件循环开始
		event_inloop = 1;
    // 检查读事件
		for (ev = TAILQ_FIRST(&readqueue); ev;) {
			old = TAILQ_NEXT(ev, ev_read_next);
			if (FD_ISSET(ev->ev_fd, event_readset)) {
        // 将事件从所有队列移除
				event_del(ev);
        // 执行回调函数
				(*ev->ev_callback)(ev->ev_fd, EV_READ,
						   ev->ev_arg);
			} else if (ev->ev_fd > maxfd)
				maxfd = ev->ev_fd;

			ev = old;
		}

    // 检查写事件
		for (ev = TAILQ_FIRST(&writequeue); ev;) {
			old = TAILQ_NEXT(ev, ev_read_next);
			if (FD_ISSET(ev->ev_fd, event_writeset)) {
				event_del(ev);
				(*ev->ev_callback)(ev->ev_fd, EV_WRITE,
						   ev->ev_arg);
			} else if (ev->ev_fd > maxfd)
				maxfd = ev->ev_fd;

			ev = old;
		}
    // 事件循环结束
		event_inloop = 0;

    // 先将所有 add 队列中的事件，转移到读写队列中
		for (ev = TAILQ_FIRST(&addqueue); ev; 
		     ev = TAILQ_FIRST(&addqueue)) {
			TAILQ_REMOVE(&addqueue, ev, ev_add_next);
			ev->ev_flags &= ~EVLIST_ADD;
			
			event_add_post(ev);

      // 更新 maxfd
			if (ev->ev_fd > maxfd)
				maxfd = ev->ev_fd;
		}

    // 重新分配读写集合的大小
		if (events_recalc(maxfd) == -1)
			return (-1);

		timeout_process();
	}

	return (0);
}

// 初始化事件对象
void
event_set(struct event *ev, int fd, short events,
	  void (*callback)(int, short, void *), void *arg)
{
	ev->ev_callback = callback;
	ev->ev_arg = arg;
	ev->ev_fd = fd;
	ev->ev_events = events;
	ev->ev_flags = EVLIST_INIT;
}

/*
 * Checks if a specific event is pending or scheduled.
 */

int
event_pending(struct event *ev, short event, struct timeval *tv)
{
	int flags = ev->ev_flags;

	/*
	 * We might not have been able to add it to the actual queue yet,
	 * check if we will enqueue later.
	 */
	if (ev->ev_flags & EVLIST_ADD)
		flags |= (ev->ev_events & (EV_READ|EV_WRITE));

	event &= (EV_TIMEOUT|EV_READ|EV_WRITE);

	/* See if there is a timeout that we should report */
	if (tv != NULL && (flags & event & EV_TIMEOUT))
		*tv = ev->ev_timeout;

	return (flags & event);
}

void
event_add(struct event *ev, struct timeval *tv)
{
	LOG_DBG((LOG_MISC, 55,
		 "event_add: event: %p, %s%s%scall %p",
		 ev,
		 ev->ev_events & EV_READ ? "EV_READ " : " ",
		 ev->ev_events & EV_WRITE ? "EV_WRITE " : " ",
		 tv ? "EV_TIMEOUT " : " ",
		 ev->ev_callback));
	if (tv != NULL) {
		struct timeval now;
		struct event *tmp;

		gettimeofday(&now, NULL);
    // 设置绝对超时时间, tv 是相对超时时间
		timeradd(&now, tv, &ev->ev_timeout);

		LOG_DBG((LOG_MISC, 55,
			 "event_add: timeout in %d seconds, call %p",
			 tv->tv_sec, ev->ev_callback));
    // 如果设置了 EVLIST_TIMEOUT，先将事件从超时队列移除,待会将其加入到
    // 超时队列正确的位置
		if (ev->ev_flags & EVLIST_TIMEOUT)
			TAILQ_REMOVE(&timequeue, ev, ev_timeout_next);

		/* Insert in right temporal order */
    // 将 ev 插入到超时队列，超时时间由小到大。
		for (tmp = TAILQ_FIRST(&timequeue); tmp;
		     tmp = TAILQ_NEXT(tmp, ev_timeout_next)) {
         // 遍历超时队列，如果事件 ev 的超时时间 <= 队列中某个事件就 break
		     if (timercmp(&ev->ev_timeout, &tmp->ev_timeout, <=))
			     break;
		}

    // 将 ev 插入到 tmp 前面
		if (tmp)
			TAILQ_INSERT_BEFORE(tmp, ev, ev_timeout_next);
    // 插入到超时队列尾端
		else
			TAILQ_INSERT_TAIL(&timequeue, ev, ev_timeout_next);

    // 设置超时标记
		ev->ev_flags |= EVLIST_TIMEOUT;
	}

  // 事件循环已经开始, 则不能把事件加到读写队列，先加入到 add 队列等待处理
  // 这种情况会在处理事件的事件回调函数中发生。
	if (event_inloop) {
		/* We are in the event loop right now, we have to
		 * postpone the change until later.
		 */
    // 如果事件已经被加到了 add 队列就直接返回
		if (ev->ev_flags & EVLIST_ADD)
			return;

    // 将事件添加到 add 队列，同时置事件标志位为 EVLIST_ADD
		TAILQ_INSERT_TAIL(&addqueue, ev, ev_add_next);
		ev->ev_flags |= EVLIST_ADD;
	} else
  // 事件循环还没开始，根据事件类型，将事件添加到读队列或写队列
		event_add_post(ev);
}

void
event_add_post(struct event *ev)
{
  // 读事件，还没加到读队列的话，就添加到读队列
	if ((ev->ev_events & EV_READ) && !(ev->ev_flags & EVLIST_READ)) {
		TAILQ_INSERT_TAIL(&readqueue, ev, ev_read_next);
		
		ev->ev_flags |= EVLIST_READ;
	}
	
  // 写事件，如果还没加到写队列的话，就添加到写队列
	if ((ev->ev_events & EV_WRITE) && !(ev->ev_flags & EVLIST_WRITE)) {
		TAILQ_INSERT_TAIL(&writequeue, ev, ev_write_next);
		
		ev->ev_flags |= EVLIST_WRITE;
	}
}

// 将事件从所有队列移除
void
event_del(struct event *ev)
{
	LOG_DBG((LOG_MISC, 80, "event_del: %p, callback %p",
		 ev, ev->ev_callback));

  // 从 add 队列移除
	if (ev->ev_flags & EVLIST_ADD) {
		TAILQ_REMOVE(&addqueue, ev, ev_add_next);

		ev->ev_flags &= ~EVLIST_ADD;
	}

	if (ev->ev_flags & EVLIST_TIMEOUT) {
		TAILQ_REMOVE(&timequeue, ev, ev_timeout_next);

		ev->ev_flags &= ~EVLIST_TIMEOUT;
	}

	if (ev->ev_flags & EVLIST_READ) {
		TAILQ_REMOVE(&readqueue, ev, ev_read_next);

		ev->ev_flags &= ~EVLIST_READ;
	}

	if (ev->ev_flags & EVLIST_WRITE) {
		TAILQ_REMOVE(&writequeue, ev, ev_write_next);

		ev->ev_flags &= ~EVLIST_WRITE;
	}
}

// 计算超时队列队头事件还有多久到期
int
timeout_next(struct timeval *tv)
{
	struct timeval now;
	struct event *ev;

  // 超时队列为空，将 tv 设置为 5 秒
	if ((ev = TAILQ_FIRST(&timequeue)) == NULL) {
		timerclear(tv);
    // TIMEOUT_DEFAULT = 5
		tv->tv_sec = TIMEOUT_DEFAULT;
		return (0);
	}

  // 获取当前时间
	if (gettimeofday(&now, NULL) == -1)
		return (-1);

  // 如果当前超时队列队头元素超时时间已经到期，就清空 tv
	if (timercmp(&ev->ev_timeout, &now, <=)) {
		timerclear(tv);
		return (0);
	}

  // 如果超时时间还没到，就计算还有多久到期
	timersub(&ev->ev_timeout, &now, tv);

	LOG_DBG((LOG_MISC, 60, "timeout_next: in %d seconds", tv->tv_sec));
	return (0);
}

void
timeout_process(void)
{
	struct timeval now;
	struct event *ev;

	gettimeofday(&now, NULL);

  // 将所有到期的事件对象移除
	while ((ev = TAILQ_FIRST(&timequeue)) != NULL) {
    // 只要超时队列第一个事件没有超时，那后面的都没超时，退出循环
		if (timercmp(&ev->ev_timeout, &now, >))
			break;

		TAILQ_REMOVE(&timequeue, ev, ev_timeout_next);
		ev->ev_flags &= ~EVLIST_TIMEOUT;

		LOG_DBG((LOG_MISC, 60, "timeout_process: call %p",
			 ev->ev_callback));
		(*ev->ev_callback)(ev->ev_fd, EV_TIMEOUT, ev->ev_arg);
	}
}
