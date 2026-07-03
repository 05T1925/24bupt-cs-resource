#include<stdio.h>

int Q[10000][20];
int K[10000][20];
int V[10000][20];
int W[10000];
long long T[20][20];
long long M[10000][20];

int main()
{
	int n, d;
	scanf("%d %d", &n, &d);
	
	for(int i = 0; i<n; i++){
		for(int j=0; j<d; j++){
			scanf("%d", &Q[i][j]);
		}
	}
	
	for(int i=0;i<n;i++){
		for(int j=0;j<d;j++){
			scanf("%d", &K[i][j]);
		}
	}
	
	for(int i=0;i<n;i++){
		for(int j=0;j<d;j++){
			scanf("%d", &V[i][j]);
		}
	}
	
	for(int i=0;i<n;i++){
		scanf("%d", &W[i]);
	}
	
	/*
	for(int i=0;i<d;i++){
			for(int j=0;j<d;j++){
				T[i][j] = 0;
			}
		}
	*/
	
	for(int i=0;i<d;i++){
		for(int j=0;j<d;j++){
			T[i][j] = 0;
			for(int k=0;k<n;k++){
				T[i][j] += (long long)K[k][i] * (long long)V[k][j];
			}
		}
	}
	
	for(int i=0;i<n;i++){
		for(int j=0;j<d;j++){
			M[i][j] = 0;
			for(int k=0;k<d;k++){
				M[i][j] += (long long)Q[i][k] * T[k][j];
			}
		}
	}
	
	    for (int i = 0; i < n; i++) {
	        for (int j = 0; j < d; j++) {
	            M[i][j] *= (long long)W[i]; // 使用long long防止溢出
	        }
	    }
	    
	    // 输出结果
	    for (int i = 0; i < n; i++) {
	        for (int j = 0; j < d; j++) {
	            // 输出每个元素，最后一个元素后不加空格
	            printf("%lld", M[i][j]);
	            if (j < d - 1) printf(" ");
	        }
	        printf("\n");
	    }
	    
	    return 0;
}
