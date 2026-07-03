#include <stdio.h>
#include <stdlib.h>


/*
const int N = 100001;
typedef long long  ll;
ll a[N], b[N] , cnt;

void Merge(ll l, ll mid, ll r){
	ll i = l; j = mid + 1, t = 0;
	while(i <= mid && j <= r) {
		if(a[i] > a[j]){
		b[t++] = a[j++];
		cnt += mid - i +1;
	}
	else b[t++] = a[i++];
}
while(i <= mid)  b[t++] = a[i++];
while(j <= r)    b[t++] = a[j++];
for(i = 0; i < t; i++)  a[l + i] = b[i];

}
*/



// 全局变量，记录逆序对的数量
int count = 0;

// 合并两个有序子数组
void merge(int arr[], int left, int mid, int right) {
    int i, j, k;
    int n1 = mid - left + 1; // 左子数组的长度
    int n2 = right - mid;    // 右子数组的长度

    // 创建临时数组
    int *L = (int *)malloc(n1 * sizeof(int));
    int *R = (int *)malloc(n2 * sizeof(int));

    // 将数据复制到临时数组 L 和 R
    for (i = 0; i < n1; i++)
        L[i] = arr[left + i];
    for (j = 0; j < n2; j++)
        R[j] = arr[mid + 1 + j];

    // 合并临时数组 back 到 arr[left..right]
    i = 0; // 初始化左子数组的索引
    j = 0; // 初始化右子数组的索引
    k = left; // 初始化合并后数组的索引
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            arr[k] = L[i];
            i++;
        } else {
            arr[k] = R[j];
            j++;
            // 统计逆序对：如果 L[i] > R[j]，则左子数组中从 i 到 n1-1 的元素都与 R[j] 构成逆序对
            count += n1 - i;
        }
        k++;
    }

    // 复制左子数组的剩余元素（如果有）
    while (i < n1) {
        arr[k] = L[i];
        i++;
        k++;
    }

    // 复制右子数组的剩余元素（如果有）
    while (j < n2) {
        arr[k] = R[j];
        j++;
        k++;
    }

    // 释放临时数组
    free(L);
    free(R);
}

// 归并排序
void mergeSort(int arr[], int left, int right) {
    if (left < right) {
        // 计算中间位置
        int mid = left + (right - left) / 2;

        // 递归排序左子数组和右子数组
        mergeSort(arr, left, mid);
        mergeSort(arr, mid + 1, right);

        // 合并两个有序子数组
        merge(arr, left, mid, right);
    }
}

int main() {
    // 示例数组
    int arr[] = {2, 4, 1, 3, 5};
    int n = sizeof(arr) / sizeof(arr[0]);

    // 调用归并排序
    mergeSort(arr, 0, n - 1);

    // 输出逆序对的数量
    printf("逆序对的数量：%d\n", count);

    return 0;
}
