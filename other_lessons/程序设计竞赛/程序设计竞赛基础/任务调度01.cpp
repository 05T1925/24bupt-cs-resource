#include <stdio.h>
#include <string.h>
#include <algorithm>
using namespace std;

const int INF = 0x3f3f3f3f;
const int MAX_N = 41;
const int MAX_TIME = 400;

int main() {
    int n;
    scanf("%d", &n);
    
    int a[MAX_N], c[MAX_N], d_min[MAX_N];
    for (int i = 1; i <= n; i++) {
        int b, d;
        scanf("%d %d %d %d", &a[i], &b, &c[i], &d);
        d_min[i] = min(b, d); // 方式3取较小时间
    }
    
    int dp[MAX_N][MAX_TIME + 1];
    memset(dp, 0x3f, sizeof(dp)); // 初始化为INF
    dp[0][0] = 0; // 初始状态
    
    for (int i = 1; i <= n; i++) {
        for (int j = 0; j <= MAX_TIME; j++) {
            // 方式1：任务i只占用轨道2（普通CPU）
            dp[i][j] = dp[i - 1][j] + a[i];
            
            // 方式2：任务i只占用轨道1（带GPU的CPU）
            if (j >= c[i]) {
                dp[i][j] = min(dp[i][j], dp[i - 1][j - c[i]]);
            }
            
            // 方式3：任务i独占两个轨道
            if (j >= d_min[i]) {
                dp[i][j] = min(dp[i][j], dp[i - 1][j - d_min[i]] + d_min[i]);
            }
        }
    }
    
    int ans = INF;
    for (int j = 0; j <= MAX_TIME; j++) {
        if (dp[n][j] < INF) {
            ans = min(ans, max(j, dp[n][j]));
        }
    }
    
    printf("%d\n", ans);
    return 0;
}
