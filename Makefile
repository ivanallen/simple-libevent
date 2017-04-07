main:event-test queue-test
event-test:event.c event.h queue.h event-test.c
	gcc -g -o event-test event.c event-test.c
queue-test:queue-test.c queue.h
	gcc -g -o queue-test queue-test.c
