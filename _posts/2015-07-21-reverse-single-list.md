---
layout: blog
title: 单链表逆转
description: 有一单链表将其逆转
keywords: 单链表
---

一道比较经典的算法题目：有一单链表将其逆转。刚开始的时候想的磕磕绊绊，静下心来2分钟码定。汗。  

##分析  

每次调整对一个节点，并将其记住。继续处理下一个，可以进入相同的状态，区别仅在于第一个时，前一个“记录”是NULL。



## 代码
```
typedef struct list_ {
    int a;
    struct list_ *next;
}list;

list *reverse_list(list *head)
{
	list *r = NULL,*p = head,*q;
	while(p) {
		q = p->next;
		p->next = r;
		r = p;//remember the current one
		p =q; // to the next
	}
	return r;
}
```
## 测试代码
```
int main(void)
{
	int i = 10;
	list *q = NULL,*p;
	while(--i) {
		p = (list*)malloc(sizeof(list));
		p->a = i;
		p->next = q;
		q =p;
	}
	p = q;
	while(q) {
		printf("%d ", q->a);
		q=q->next;
	}
	q = reverse_list(p);
	p = q;
	while(q) {
		printf("%d ", q->a);q = q->next;
	}
	while (p) {
		q = p->next;
		free(p);
		p = q;
	}
	return 0;
}
```
### output
1 2 3 4 5 6 7 8 9 9 8 7 6 5 4 3 2 1 
