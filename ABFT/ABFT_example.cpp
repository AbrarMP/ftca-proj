// #ifndef ABFT
// #define ABFT

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

long* compute_checksums(long* data, int width, int height)
{	
	long row_checksum, col_checksum = 0;
	for(int j = 0; j<height; j++)
	{
		row_checksum = 0;
		for(int i = 0; i< width;i++)
		{
			row_checksum += data[(width+1)*j+i]; 
		}
		// printf("row %d checksum: %ld\n", j, row_checksum);
		data[(width+1)*(j+1)-1] = row_checksum;
	}

	for(int i = 0; i<=width; i++)
	{
		col_checksum = 0;
		for(int j = 0; j<height; j++)
		{
			col_checksum += data[(width+1)*j+i]; 
		}
		// printf("col %d checksum: %ld\n", i, col_checksum);
		data[(width+1)*height+i] = col_checksum;
	}
}

long* correct_checksum(long* data, int width, int height)
{
	long row_checksum, col_checksum = 0;
	for(int j = 0; j<height; j++)
	{
		row_checksum = 0;
		for(int i = 0; i< width;i++)
		{
			row_checksum += data[(width+1)*j+i]; 
		}
		// printf("row %d checksum: %ld\n", j, row_checksum);
		data[(width+1)*(j+1)-1] = row_checksum;
	}

	for(int i = 0; i<=width; i++)
	{
		col_checksum = 0;
		for(int j = 0; j<height; j++)
		{
			col_checksum += data[(width+1)*j+i]; 
		}
		// printf("col %d checksum: %ld\n", i, col_checksum);
		data[(width+1)*height+i] = col_checksum;
	}
}


long* correct_weighted_checksums(long* data, int width, int height, int& data_errors, int& checksum_errors, int& weighted_checksum_errros)
{
	long row_checksum, weighted_row_checksum, s1, s2 = 0;
	data_errors = checksum_errors = weighted_checksum_errros =0;
	for(int j = 0; j<height; j++)
	{
		row_checksum = weighted_row_checksum = s1 = s2 = 0;
		for(int i = 0; i< width;i++)
		{
			row_checksum += data[(width+2)*j+i];
			weighted_row_checksum += data[(width+2)*j+i]*(i+1); 
		}
		printf("row %d checksum: %ld\n", j, row_checksum);
		printf("weighted row %d checksum: %ld\n", j, weighted_row_checksum);
		s1 = row_checksum - data[(width+2)*(j+1)-2];
		s2 = weighted_row_checksum - data[(width+2)*(j+1)-1];
		if(s1 !=0 && s2 !=0) //correct value based on checksum
		{
			int col = s2/s1;
			data[(width+2)*j+col-1] -= s1;
			data_errors++;
		}
		else if(s1 == 0 && s2 != 0) //weighted checksum in error
		{
			data[(width+2)*(j+1)-1] = weighted_row_checksum;	
			weighted_checksum_errros++;		
		}
		else if(s1 != 0 && s2 == 0) //unweighted checksum in error
		{
			data[(width+2)*(j+1)-2] = row_checksum;
			checksum_errors++;

		}
		else //no error
			;

	}
}	

long* compute_weighted_checksums(long* data, int width, int height)
{
	long row_checksum, weighted_row_checksum = 0;
	for(int j = 0; j<height; j++)
	{
		row_checksum = 0;
		weighted_row_checksum = 0;
		for(int i = 0; i< width;i++)
		{
			row_checksum += data[(width+2)*j+i];
			weighted_row_checksum += data[(width+2)*j+i]*(i+1); 
		}
		printf("row %d checksum: %ld\n", j, row_checksum);
		printf("weighted row %d checksum: %ld\n", j, weighted_row_checksum);
		data[(width+2)*(j+1)-2] = row_checksum;
		data[(width+2)*(j+1)-1] = weighted_row_checksum;
	}
}	

int main(int argc, char **argv) {
	long data[25] = {1, 2, 3, 0, 0, 4, 5, 6, 0, 0, 7, 8, 9, 0, 0, 10, 11, 12, 0, 0, 13, 14, 15, 0, 0};
	int	data_errors, checksum_errors, weighted_checksum_errros =0;

	compute_weighted_checksums(data, 3, 5);
	for(int i = 0; i < 21; i++)
	{
		if(i%5 == 0 && i!=0)
			printf("\n");
		printf("%ld ", data[i]);
			
	}

	printf("\n\nError matrix: \n");
	data[1]=4;
	data[5]=9;
	data[12]=3;
	data[18]=0;
	data[24]=42;
	for(int i = 0; i < 25; i++)
	{
		if(i%5 == 0 && i!=0)
			printf("\n");
		printf("%ld ", data[i]);
			
	}

	correct_weighted_checksums(data, 3, 5, data_errors, checksum_errors, weighted_checksum_errros);

	printf("\n\nCorrected Matrix:\n");
	for(int i = 0; i < 25; i++)
	{
		if(i%5 == 0 && i!=0)
			printf("\n");
		printf("%ld ", data[i]);
			
	}

	printf("\n");
	printf("Data errors: %d\n", data_errors);
	printf("Checksum errors: %d\n", checksum_errors);
	printf("Weighted checksum errors: %d\n", weighted_checksum_errros);

	return 0;
}

// #endif