#include<stdio.h>
#include<string.h>
#include<stdlib.h>

#define N 1010

typedef struct{
	int x, y; 
} Point ;

int n, m, k, d;
int cnt[N][N];
int vis[N][N];
int trace[N][N];
int dis[N][N];

Point queue[N*N];
int head, tail;

int dx[4] = {-1, 0 , 1, 0};
int dy[4] = {0, 1, 0, -1};



int check(int x,int y){
	return (x >= 1 && x <= n && y >= 1 && y <= n);
}

int main(){
	scanf("%d %d %d %d", &n, &m, &k, &d);
	
	memset(dis, 0x3f, sizeof(dis)); //zhuyi1 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	memset(vis,0,sizeof(vis));
	memset(trace,0,sizeof(trace));
	memset(cnt,0,sizeof(cnt));
	
	head = tail = 0;
	
	for(int i=0;i<m;i++){
		int x ,y;
		scanf("%d %d", &x, &y);
		dis[x][y] = 0;
		vis[x][y] = 1;
		
		queue[tail].x = x;
		queue[tail].y = y;
		tail++;
	}
	
	for(int i=0;i<k;i++){
		int x, y, c;
		scanf("%d %d %d", &x, &y, &c);
		cnt[x][y] += c;
	}
	
	for(int i=0;i<d;i++){
		int x, y;
		scanf("%d %d", &x, &y);
		trace[x][y] =  1;
	}
	
	while(head <tail){
		Point cur = queue[head];
		head++;
		int x = cur.x;
		int y = cur.y;
		
		for(int i=0;i<4;i++){
			int nx = x + dx[i];
			int ny = y + dy[i];
			
			if(check(nx, ny) && !trace[nx][ny] && !vis[nx][ny]){
				dis[nx][ny] = dis[x][y] + 1;
				vis[nx][ny] = 1;
				
				queue[tail].x = nx;
				queue[tail].y = ny;
				tail++;
				
			}
		}
	}
	
	long long ans = 0;
	for(int i = 1;i<=n;i++){             // ======= ×¢Òâ ÊÇ=1    <=n
		for(int j=1;j<=n;j++){
			ans += (long long)cnt[i][j] * dis[i][j];
			
		}
	}
	
	printf("%lld\n", ans);
	
	return 0;
}
