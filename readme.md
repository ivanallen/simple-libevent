# 轻量版 libevent 库

本程序根据 libevent0.1 版本修改而来，方便大家学习用的。

# 编译

```
$ make
```
 
- 产生的 event-test 是 event 的测试 demo
- 产生的 queue-test 是 queue.h 中队列相关的测试 demo

# 运行

```
$ ./event-test
```

再开启一个终端：

```
$ echo "hello world" > event.fifo
```
