#include<stdio.h>
#include<string.h>

#define MaxN 101
#define MaxM 101
#define MaxT 301
#define QuSize (MaxN * MaxM * MaxT + 5)

typedef struct {
	int x, y, t;
} State;

State queue[QuSize];
int head, tail;
int vis[MaxN][MaxM][MaxT];
int stime[MaxN][MaxM];
int etime[MaxN][MaxM];

int dx[4] = {0, 1, 0, -1};
int dy[4] = {1, 0, -1, 0};

int main(){
	
	int n, m, tnum;
	scanf("%d %d %d", &n, &m, &tnum);
	
	memset(stime, -1, sizeof(stime));
	memset(etime, -1, sizeof(etime));
	memset(vis, 0, sizeof(vis));
	head = 0;
	tail = 0;
	State start = {1, 1, 0};
	queue[tail++] = start;
	vis[1][1][0] = 1;	
	
	
	for(int i=0;i< tnum;i++){
		int r, c, a, b;
		scanf("%d %d %d %d", &r, &c, &a, &b);
		stime[r][c] = a;
		etime[r][c] = b;
	}	
	

	
	if(n ==1 && m ==1){
		printf("0\n");
		return 0;
	}
	

	
	while(head < tail){
		State cur = queue[head++];
		for(int i = 0; i<4; i++){
			int nx = cur.x + dx[i];
			int ny = cur.y + dy[i];
			int nt = cur.t + 1;
			
			if(nx<1||nx>n||ny<1||ny>m)continue;
			if(nt>=MaxT) continue;
			if(vis[nx][ny][nt]) continue;
			
			int dangerous = 0;
			if(stime[nx][ny] != -1){
				if(nt >=stime[nx][ny] && nt <=etime[nx][ny]){
					dangerous = 1;
				}
			}
			if(dangerous) continue;
					
			vis[nx][ny][nt] = 1;
			State next = {nx,ny,nt};
			queue[tail++] = next;
			
			if(nx == n&&ny == m){
				printf("%d\n", nt);
				return 0;
			}
		}

	}
	
	printf("-1\n");
	return 0;
}
