#include <stdio.h>
#include <string.h>
#include <algorithm>

const int MAXN = 45;
const int MAX_TIME = 410; // 40*10 + 10

int dp[MAXN][MAX_TIME][MAX_TIME];
int n;
int a[MAXN], b[MAXN], c[MAXN], d[MAXN];

// 递归进行状态转移
void dfs(int i, int x, int y) {
    // 终止条件：已访问或超出时间范围
    if (dp[i][x][y] || x >= MAX_TIME || y >= MAX_TIME) 
        return;
    
    dp[i][x][y] = 1; // 标记当前状态可达
    
    // 递归终止：所有任务处理完成
    if (i > n) 
        return;
    
    // 1. 在两个CPU上运行（不占用GPU）
    dfs(i+1, x + b[i], y + b[i]);
    
    // 2. 在两个CPU+GPU上运行
    dfs(i+1, x + d[i], y + d[i]);
    
    // 3. 在单个CPU+GPU上运行（当前绑定GPU的CPU）
    dfs(i+1, x + c[i], y);
    
    // 4. 在单个CPU+GPU上运行（切换绑定关系）
    if (x <= y) {
        dfs(i+1, y + c[i], x); // 交换x,y表示GPU绑定关系变化
    }
    
    // 5. 在单个CPU上运行（当前绑定GPU的CPU）
    dfs(i+1, x + a[i], y);
    
    // 6. 在单个CPU上运行（未绑定GPU的CPU）
    dfs(i+1, x, y + a[i]);
}

int main() {
    // 初始化
    memset(dp, 0, sizeof(dp));
    
    // 读取输入
    scanf("%d", &n);
    for (int i = 1; i <= n; i++) {
        scanf("%d %d %d %d", &a[i], &b[i], &c[i], &d[i]);
    }
    
    // 从初始状态开始DFS
    dfs(1, 0, 0);
    
    // 寻找最优解
    int ans = 1e9;
    for (int x = 0; x < MAX_TIME; x++) {
        for (int y = 0; y < MAX_TIME; y++) {
            if (dp[n+1][x][y]) {
                // 完成时间为两个CPU耗时的最大值
                int time = (x > y) ? x : y;
                if (time < ans) 
                    ans = time;
            }
        }
    }
    
    printf("%d\n", ans);
    return 0;
}
