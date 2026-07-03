#include<stdio.h>
#include<math.h>

int main1(){
	
	int input = 0;
	int sum = 0;
	
	scanf("%d", &input);
	int i = 0;
	while(input)
	{
		int bit = input%10;
		if(bit%2 == 1)
		{
			sum += 1*pow(10, i);
			i++;
		}
		else{
			sum += 0*pow(10, i);
			i++;
		}
		input /= 10;
	}
	printf("%d\n", sum);
	return 0;
}

/*
		if(bit%2 == 1)
		{
			bit=1;
		}
		else{
			bit=0;
		}
		sum += bit*pow(10, i);//i++р╡©ирт
		i++;
		input /= 10;
*/

int main2(){
	int n = 0;
	while(scanf("%d", &n) == 1)
	{
		int i = 0;
		int j = 0;
		for( i=0;i<n;i++){
			for(j=0; j<n; j++){
				if(i+j<n-1)
				{
					printf("  ");
				}
				else{
					printf("* ");
				}
			}
			printf("\n");
		}
	}
	return 0;
}
