#include<stdio.h>
#include<string.h>
#include<algorithm>


const int MN = 45;
const int MT = 410;

int dp[MN][MT][MT];
int n;
int a[MN],b[MN],c[MN],d[MN];

void dfs(int i, int x, int y){
	if(dp[i][x][y] || x >= MT || y >= MT) return;
	
	dp[i][x][y] = 1;
	
	if(i > n) return;
	
	dfs(i + 1, x+b[i], y+b[i]);
	dfs(i + 1, x+d[i], y+d[i]);
	dfs(i + 1, x+c[i], y);
	if(x<=y){
		 dfs(i + 1, y + c[i], x);
	}
	dfs(i + 1, x+a[i], y);
	dfs(i + 1, x, y+a[i]);
}

int main(){
	memset(dp , 0, sizeof(dp));
	scanf("%d", &n);
	for(int i=1;i<=n;i++){
		scanf("%d %d %d %d", &a[i], &b[i], &c[i], &d[i]);
	}
	 
	dfs(1,0,0);
	
	int ans = 1e9;
	for(int x=0;x<MT;x++){
		for(int y=0;y<MT;y++){
			if(dp[n+1][x][y]){
				int time = (x>y) ? x: y;
				ans = (time<ans) ? time: ans;
			}
		}
	}
	printf("%d\n", ans);
	return 0;
}
