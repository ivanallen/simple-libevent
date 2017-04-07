#include <stdio.h>
#include <string.h>
#include "queue.h"

struct person {
  TAILQ_ENTRY(person) male;
  TAILQ_ENTRY(person) female;
  char name[20];
  int age;
};

void print(struct person *p) {
  printf("name = %s, age = %02d\n", p->name, p->age);
}


int main() {
  // 定义队头
  TAILQ_HEAD(male_list, person) malequeue;
  TAILQ_HEAD(female_list, person) femalequeue;
  
  // 初始化
  TAILQ_INIT(&malequeue);
  TAILQ_INIT(&femalequeue);  
 
  struct person p1, p2, p3, p4, p5, p6;
  strcpy(p1.name, "allen");
  p1.age = 20;

  strcpy(p2.name, "luffy");
  p2.age = 21;

  strcpy(p3.name, "nami");
  p3.age = 19;

  strcpy(p4.name, "zoro");
  p4.age = 22;

  strcpy(p5.name, "robin");
  p5.age = 28;

  strcpy(p6.name, "vivi");
  p6.age = 28;

  // 插入元素
  TAILQ_INSERT_HEAD(&malequeue, &p1, male); 
  TAILQ_INSERT_HEAD(&malequeue, &p2, male); 
  TAILQ_INSERT_TAIL(&malequeue, &p4, male); 

  TAILQ_INSERT_HEAD(&femalequeue, &p3, female);
  TAILQ_INSERT_AFTER(&femalequeue, &p3, &p5, female);
  TAILQ_INSERT_BEFORE(&p5, &p6, female);

  struct person *e;

  TAILQ_FOREACH(e, &malequeue, male) {
    print(e); 
  }

  TAILQ_FOREACH(e, &femalequeue, female) {
    print(e); 
  }

  print(TAILQ_LAST(&malequeue, male_list));

  printf("%p\n", *(((struct male_list*)malequeue.tqh_last)->tqh_last));

  return 0;
}
