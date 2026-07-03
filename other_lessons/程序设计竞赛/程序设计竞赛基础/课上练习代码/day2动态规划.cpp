#include<stdio.h>
#include<stdlib.h>

                        //1.矩阵连乘问题
//带备忘录技能 m[i][j] 防止重复计算
int recurMatrixChain(int i, int j)
{
	if(m[i][j] > 0) return m[i][j];
	if(i == j)return 0;
	int u = recurMatrixChain(i, i)+ recurMatrixChain(i+ 1, j) + p[i-1]*p[i]*p[j];
	s[i][j] = i;
	for(int k = i + 1;k < j;k++){
		int t = recurMatrixChain(i, k) + recurMatrixChain(k + 1,j) + p[i-1]*p[k]*p[j];
		if(t < u){
			u = t;
			s[i][j] = k;
		}
	}
	m[i][j] = u;
   return u;	
}

//动态规划 自下而上
void MatrixChain(int *p, int n, int **m, int **s)
{
	for(int i=1; i<=n; i++){
		m[i][i] = 0;
	}
	for(int r=2; r<=n; r++){
		for(int i=1; i<=n-r; i++){
			int j = i+r-1;
			m[i][j] = m[i+1][j] + p[i-1]*p[i]*p[j];
			s[i][j] = i;
			for(int k = i+1; k<=j; k++){
				int t = m[i][k] + m[k+1][j] + p[i-1]*p[k]*p[j];
				if(t < m[i][j]){
					m[i][j] = t;
					s[i][j] = k;
				}
			}
		}
	}
}


                    //2.最长公共子序列
//计算最优值
void LCSLength(int m, int n, char *x, char *y, int **c, int **b)
{
	int i, j; 
	for(i=1; i<=m; i++){  c[i][0] = 0;	} //初始化 Y[j]为空时
	for(i=1; i<=n; i++){  c[0][i] = 0;	}
	
	for(i = 1; i<=m; i++){
		for(j=1; j<=n; j++){
			if(x[i] == y[j]){
				c[i][j] = c[i-1][j-1] + 1;
				b[i][j] = 1;}
			
			else if(c[i-1][j] >= c[i][j-1] ){
				c[i][j] = c[i-1][j];
				b[i][j] = 2;}
			else{
				c[i][j] = c[i][j-1];
				b[i][j] = 3;}
			
		}
		
	}
}
//构造最长公共子序列
void LCS(int i, int j, char *x, int **b)
{
	if(i == 0 || j == 0)return;
	if(b[i][j]==1){
		LCS(i-1, j-1, x, b );
		count << x[i];
	}
	else if(b[i][j] == 2) LCS(i-1, j, x, b);
	else LCS(i, j-1, x, b);
}

//利用最长公共子序列求解最长上升/递减子序列问题
void LIS(int a[], int x[], int m)
{//该算法求解x[1:m]这个长度为m的整型数组的最长递减子序列长度
	
	for(int i=1; i<=m; i++){
		scanf("%d", &x[i]); //输入数组x
		for(int i = 1; i<=m; i++){
			a[i] = x[i]; //副本a
			sort(a, 1, m); //对数组a进行递减归并排序
			LCSLength(x, a, m, m);//求数组x与数组a的最长公共子序列长度
		}
	}
}

//利用最长公共子序列求解回文词的构造问题
int huiwen(int n, char x[], char y[])
//该算法求解x[1:n]这个长度为n的字符串变为回文串时最少要插入的字符个数
{
	for(int i=1; i<=n; i++){
		scanf("%c", &x[i]);//向字符数组x中输入一个字符串
	}
	for(int i=1; i<=n; i++){
		y[i] = x[n-i+1];//求字符串x的逆串，保存在字符数组y中
	}
	LCSLength(x, y, n, n);//求原串x与逆串y的最长公共子序列长度
	return n-c[n][n];//返回将串x变为回文串最少要插入的字符个数,c为全局数组
}


//一个数组的两段最大子数组的和
int max1[MAXN], max2[MAXN];
int getMax(int a[],int n)
{
	int Max, dp;
	
	max1[0] = dp =a[0];
	for(int i=1; i<n; i++){
		if(dp<0){
			dp = a[i];
		}
		else dp = dp + a[i];
		if(max1[i-1]<dp){
			max1[i] = dp;
 		}
 		else max1[i] = max1[i-1];
	}
	
	max2[n-1] = dp =a[n-1];
		for(int i=n-2; i>=0; i--){
			if(dp<0){
				dp = a[i];
			}
			else dp = dp + a[i];
			if(max2[i+1]<dp){
				max2[i] = dp;
	 		}
	 		else max2[i] = max2[i+1];
		}
		
		Max = max1[0] + max2[1];
		for(int i = 1; i < n-1; i++){
			if(Max < max1[i] + max2[i+1]){
		      Max = max1[i] + max2[i+1];
			}
			return Max;
		}
}


                  //背包0/1问题
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

//背包客具体算法实现
int knaspack(int v, int w, int c, int n, int **m){
	int imax = min(w[n]-1, c);
	
}


int knapsackBasic(int w, int wt[], int val[], int n, int dp[][10], int selected[]){
	for(int i=0; i<=n; i++){
		for(int w =0; w <= W; w++){
			if(i==0 || w==0){
				dp[i][w] = 0;
			} else if(wt[i-1] <= w){
				dp[i][w] = max(val[i-1] + dp[i-1][w-wt[i-1]], dp[i-1][w]);
			}else{
				dp[i][w] = dp[i-1][w];
			}
		}
	}
	
	int w = W;a
	
}
