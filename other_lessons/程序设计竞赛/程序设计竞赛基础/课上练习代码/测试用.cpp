#include<stdio.h>
#include<stdlib.h>

int max(int a , int b){
	return (a>b)?a:b;
}

// 解决0/1背包问题的动态规划函数
int knapSack(int W, int wt[], int val[], int n) {
    int i=0, w=0;
    // 创建二维数组dp
    int **dp = (int **)malloc((n + 1) * sizeof(int *));
    for (i = 0; i <= n; i++) {
        dp[i] = (int *)malloc((W + 1) * sizeof(int));
    }

    // 初始化dp数组
    for (i = 0; i <= n; i++) {
        for (w = 0; w <= W; w++) {
            if (i == 0 || w == 0)
                dp[i][w] = 0;
            else if (wt[i - 1] <= w)
                dp[i][w] = max(val[i - 1] + dp[i - 1][w - wt[i - 1]], dp[i - 1][w]);
            else
                dp[i][w] = dp[i - 1][w];
        }
    }

    // 保存结果
    int result = dp[n][W];

    // 释放内存
    for (i = 0; i <= n; i++) {
        free(dp[i]);
    }
    free(dp);

    return result;
}

int main() {
    // 物品价值数组
    int val[] = {6, 3, 5, 4, 6};
    // 物品重量数组
    int wt[] = {2, 2, 6, 5, 4};
    // 背包容量
    int W = 10;
    // 物品数量
    int n = sizeof(val) / sizeof(val[0]);
    
    // 调用背包函数计算最大价值
    int max_value = knapSack(W, wt, val, n);
    
    printf("背包能装入的最大价值为: %d\n", max_value);
    
    return 0;
}
