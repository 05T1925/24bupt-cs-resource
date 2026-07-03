#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define N 1010  // 定义网格最大尺寸

// 定义全局变量
int n, m, k, d;  // n: 网格大小, m: 分店数量, k: 客户数量, d: 障碍点数量
int cnt[N][N];   // 存储每个位置的订餐总量
int vis[N][N];   // 标记是否访问过该位置
int trace[N][N]; // 标记障碍点位置
int dis[N][N];   // 存储每个位置到最近分店的最短距离

// 方向数组：上右下左（顺时针方向）
int dx[] = {-1, 0, 1, 0};
int dy[] = {0, 1, 0, -1};

// 自定义队列结构
typedef struct {
    int x, y;
} Point;

Point queue[N * N]; // 网格最多有N*N个点
int front, rear;    // 队列头尾指针

// 检查坐标是否在网格内
int check(int x, int y) {
    return (x >= 1 && x <= n && y >= 1 && y <= n);
}

int main() {
    // 读取输入数据
    scanf("%d %d %d %d", &n, &m, &k, &d);
    
    // 初始化距离为极大值 (0x3f3f3f3f ≈ 1e9)
    memset(dis, 0x3f, sizeof(dis));
    memset(vis, 0, sizeof(vis));
    memset(trace, 0, sizeof(trace));
    memset(cnt, 0, sizeof(cnt));
    
    // 初始化队列
    front = rear = 0;
    
    // 处理分店位置
    for (int i = 0; i < m; i++) {
        int x, y;
        scanf("%d %d", &x, &y);
        // 设置分店距离为0，标记为已访问
        dis[x][y] = 0;
        vis[x][y] = 1;
        // 将分店加入队列（多源BFS起点）
        queue[rear].x = x;
        queue[rear].y = y;
        rear++;
    }
    
    // 处理客户位置及订餐量
    for (int i = 0; i < k; i++) {
        int x, y, c;
        scanf("%d %d %d", &x, &y, &c);
        cnt[x][y] += c;  // 同一位置可能有多个客户
    }
    
    // 处理障碍点
    for (int i = 0; i < d; i++) {
        int x, y;
        scanf("%d %d", &x, &y);
        trace[x][y] = 1;  // 标记为障碍
    }
    
    // 多源BFS（广度优先搜索）
    while (front != rear) {
        // 取出队首元素
        Point cur = queue[front];
        front++;
        int x = cur.x;
        int y = cur.y;
        
        // 遍历四个方向
        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i];
            int ny = y + dy[i];
            
            // 检查新位置是否有效
            if (check(nx, ny) && !trace[nx][ny] && !vis[nx][ny]) {
                // 更新距离：当前点距离+1
                dis[nx][ny] = dis[x][y] + 1;
                vis[nx][ny] = 1;  // 标记为已访问
                
                // 新位置入队
                queue[rear].x = nx;
                queue[rear].y = ny;
                rear++;
            }
        }
    }
    
    // 计算总成本（使用long long防止溢出）
    long long ans = 0;
    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= n; j++) {
            // 累加：订餐量 × 最短距离
            ans += (long long)cnt[i][j] * dis[i][j];
        }
    }
    
    // 输出结果
    printf("%lld\n", ans);
    
    return 0;
}