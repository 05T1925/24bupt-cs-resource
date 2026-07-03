#include <stdio.h>
#include <string.h>
#include <limits.h>

#define MX 105
#define ALPHA 26
#define INF (LLONG_MIN / 2) 

typedef long long ll;

int Next[MX][ALPHA], Fail[MX], End[MX];
int rear, root;
int q[MX];
ll base[MX][MX], F[MX], temp_mat[MX][MX];

int newNode() {
    rear++;
    memset(Next[rear], 0, sizeof(Next[0]));
    End[rear] = 0;
    return rear;
}

void init() {
    rear = 0;
    root = newNode();
}

void insert(char *s) {
    int u = root;
    for (int i = 0; s[i]; i++) {
        int c = s[i] - 'a';
        if (!Next[u][c]) Next[u][c] = newNode();
        u = Next[u][c];
    }
    End[u]++;
}

void build() {
    int head = 0, rear = 0;
    Fail[root] = root;
    for (int c = 0; c < ALPHA; c++) {
        int v = Next[root][c];
        if (v) {
            Fail[v] = root;
            q[rear++] = v;
        } else {
            Next[root][c] = root;
        }
    }

    while (head < rear) {
        int u = q[head++];
        for (int c = 0; c < ALPHA; c++) {
            int v = Next[u][c];
            if (v) {
                Fail[v] = Next[Fail[u]][c];
                q[rear++] = v;
            } else {
                Next[u][c] = Next[Fail[u]][c];
            }
        }
    }
    
    for (int i = 0; i < rear; i++) {
        int u = q[i];
        End[u] += End[Fail[u]];
    }
}

void vec_mat_mul(ll F[], ll M[MX][MX], int n) {
    ll temp[MX];
    for (int j = 0; j < n; j++) {
        temp[j] = INF;
        for (int k = 0; k < n; k++) {
            if (F[k] != INF && M[k][j] != INF) {
                ll val = F[k] + M[k][j];
                if (val > temp[j]) temp[j] = val;
            }
        }
    }
    memcpy(F, temp, sizeof(ll) * n);
}

void mat_mat_mul(ll A[MX][MX], ll B[MX][MX], int n, ll res[MX][MX]) {
    ll temp[MX][MX];
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            temp[i][j] = INF;
            for (int k = 0; k < n; k++) {
                if (A[i][k] != INF && B[k][j] != INF) {
                    ll val = A[i][k] + B[k][j];
                    if (val > temp[i][j]) temp[i][j] = val;
                }
            }
        }
    }
    memcpy(res, temp, sizeof(ll) * MX * MX);
}

int main() {
    int n;
    ll m;
    scanf("%d %lld", &n, &m);
    init();
    
    char s[105];
    for (int i = 0; i < n; i++) {
        scanf("%s", s);
        insert(s);
    }
    build();
    
    int tot = rear;
    for (int i = 0; i < tot; i++) 
        for (int j = 0; j < tot; j++) 
            base[i][j] = INF;
    
    for (int u = 1; u <= tot; u++) {            //  зЂвт  ==
        for (int c = 0; c < ALPHA; c++) {
            int v = Next[u][c];
            if (End[v] > base[u-1][v-1]) 
                base[u-1][v-1] = End[v];
        }
    }
    
    for (int i = 0; i < tot; i++) 
        F[i] = INF;
    F[0] = 0;
    
    while (m) {
        if (m & 1) {vec_mat_mul(F, base, tot);}
        mat_mat_mul(base, base, tot, temp_mat);
        memcpy(base, temp_mat, sizeof(ll) * MX * MX);
        m >>= 1;
    }
    
    ll ans = 0;
    for (int i = 0; i < tot; i++) 
        if (F[i] > ans) ans = F[i];
    
    printf("%lld\n", ans);
    return 0;
}
