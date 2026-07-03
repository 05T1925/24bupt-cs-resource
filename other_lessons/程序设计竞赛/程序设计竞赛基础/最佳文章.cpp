#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MX 105
#define ALPHA 26
#define INF (1LL << 60)  // 定义负无穷为极小数

// AC自动机数据结构
int Next[MX][ALPHA], Fail[MX], End[MX];
int rear, root;
int Queue[MX];  // BFS队列

// 创建新节点
int newNode() {
    rear++;
    for (int i = 0; i < ALPHA; i++) {
        Next[rear][i] = 0;  // 初始化为0（不存在）
    }
    End[rear] = 0;
    return rear;
}

// 初始化AC自动机
void Init() {
    rear = 0;
    root = newNode();
}

// 插入单词到Trie树
void Insert(char *s) {
    int len = strlen(s);
    int u = root;
    for (int i = 0; i < len; i++) {
        int c = s[i] - 'a';
        if (!Next[u][c]) {
            Next[u][c] = newNode();
        }
        u = Next[u][c];
    }
    End[u]++;  // 单词结束位置计数
}

// 构建AC自动机（包括fail指针和更新End值）
void Build() {
    int head = 0, tail = 0;
    // 初始化根节点的fail指针
    Fail[root] = root;
    // 根节点的直接子节点入队
    for (int c = 0; c < ALPHA; c++) {
        if (Next[root][c]) {
            int v = Next[root][c];
            Fail[v] = root;
            Queue[tail++] = v;
        } else {
            Next[root][c] = root;  // Trie图优化：直接指向根
        }
    }
    // BFS构建fail指针
    while (head < tail) {
        int u = Queue[head++];
        for (int c = 0; c < ALPHA; c++) {
            if (Next[u][c]) {
                int v = Next[u][c];
                Fail[v] = Next[Fail[u]][c];  // 核心：fail指针传递
                Queue[tail++] = v;
            } else {
                Next[u][c] = Next[Fail[u]][c];  // Trie图优化
            }
        }
    }
    // 按BFS顺序更新End值（包含fail链上的单词）
    for (int i = 0; i < tail; i++) {
        int u = Queue[i];
        End[u] += End[Fail[u]];
    }
}

// 向量乘矩阵：F = F * M
void mul_vector_matrix(long long F[], long long M[MX][MX], int tot) {
    long long temp[MX];
    for (int j = 0; j < tot; j++) {
        temp[j] = -INF;
        for (int k = 0; k < tot; k++) {
            if (F[k] > -INF && M[k][j] > -INF) {
                if (F[k] + M[k][j] > temp[j]) {
                    temp[j] = F[k] + M[k][j];
                }
            }
        }
    }
    for (int j = 0; j < tot; j++) F[j] = temp[j];
}

// 矩阵乘法：res = M1 * M2
void mul_matrix_matrix(long long M1[MX][MX], long long M2[MX][MX], int tot, long long res[MX][MX]) {
    long long temp[MX][MX];
    for (int i = 0; i < tot; i++) {
        for (int j = 0; j < tot; j++) {
            temp[i][j] = -INF;
            for (int k = 0; k < tot; k++) {
                if (M1[i][k] > -INF && M2[k][j] > -INF) {
                    if (M1[i][k] + M2[k][j] > temp[i][j]) {
                        temp[i][j] = M1[i][k] + M2[k][j];
                    }
                }
            }
        }
    }
    for (int i = 0; i < tot; i++) {
        for (int j = 0; j < tot; j++) {
            res[i][j] = temp[i][j];
        }
    }
}

int main() {
    int n;
    long long m;
    scanf("%d %lld", &n, &m);
    Init();
    char s[105];

    // 读入所有单词并插入AC自动机
    for (int i = 0; i < n; i++) {
        scanf("%s", s);
        Insert(s);
    }
    Build();  // 构建AC自动机

    int tot = rear;  // 节点总数（1~rear），矩阵大小tot×tot
    long long base[MX][MX];  // 转移矩阵

    // 初始化转移矩阵为负无穷
    for (int i = 0; i < tot; i++) {
        for (int j = 0; j < tot; j++) {
            base[i][j] = -INF;
        }
    }

    // 构建转移矩阵
    for (int i = 1; i <= rear; i++) {  // 遍历所有节点
        for (int c = 0; c < ALPHA; c++) {  // 遍历所有字符
            int j = Next[i][c];  // 状态i通过字符c转移到的状态j
            if (End[j] > base[i-1][j-1]) {
                base[i-1][j-1] = End[j];  // 矩阵赋值：节点i->j的价值
            }
        }
    }

    // 初始化状态向量F（长度为tot）
    long long F[MX];
    for (int i = 0; i < tot; i++) {
        F[i] = -INF;
    }
    F[0] = 0;  // 起始状态：根节点（下标0）重要度为0

    // 矩阵快速幂：处理m次转移
    long long temp_mat[MX][MX];  // 临时矩阵存储中间结果
    while (m) {
        if (m & 1) {
            mul_vector_matrix(F, base, tot);  // F = F * base
        }
        // 计算 base = base * base
        mul_matrix_matrix(base, base, tot, temp_mat);
        // 复制回base
        for (int i = 0; i < tot; i++) {
            for (int j = 0; j < tot; j++) {
                base[i][j] = temp_mat[i][j];
            }
        }
        m >>= 1;  // 指数减半
    }

    // 寻找最大重要度
    long long ans = 0;
    for (int i = 0; i < tot; i++) {
        if (F[i] > ans) ans = F[i];
    }
    printf("%lld\n", ans);

    return 0;
}
