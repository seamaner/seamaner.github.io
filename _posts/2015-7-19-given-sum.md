---
layout: blog
title: 求数组中满足给定和的数对
---

两个有序数组a、b，输出两个数组中满足给定和sum的数对，即，sum = a[i] + a[j]。输入sum，输出所有的a[i]、a[j].

##分析
从中间数开始往“两边”找
##代码
```
void print_given_sum(int a[], int len_a, int b[], int len_b,int gsum)
{
    int i = 0, j = len_b -1,sum;
    while(i < len_a - 1 && j >= 0) {
        sum = a[i] + b[j];
        if (gsum > sum) {
            i++;
        } else if (gsum == sum) {
            printf("%d, %d\n", a[i], b[j]);
            i++;j--;
        } else {
            j--;
        }
    }
}
```
##测试代码
```
int main(void)
{
    int a[] = {1,2,3,4,5,6,7};
    int b[] = {5,6,7,8,9,10};
    print_given_sum(a,sizeof(a)/sizeof(int),b,sizeof(b)/sizeof(int),10);
    return 0;
}
```
###output
Output:
1, 9
2, 8
3, 7
4, 6
5, 5


