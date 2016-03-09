/*
Analysed by etc3tera

IGA
{
	CHAR "IGA0",		// 0x00
	
	CHAR Size1[2 - 5]			// Start At 01
	CHAR File_Metadata[Size1]

	CHAR Size2[2 - 5]
	CHAR FilenameString[Size2]	// Splitted by File_Metadata.filename_len
	
	// Rest are Real File Data.....
	
*/

#include <tchar.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <Windows.h>

#define BUFSIZE 4096

typedef struct _BLOCKSIZE_METADATA{
	int filename_offset;
	int start_offset;
	int file_len;
} BLOCKSIZE_METADATA;

std::list<unsigned int> meta_decode_loop(FILE *fp, unsigned int maxLength);
unsigned int meta_decode(FILE *fp, unsigned int maxLength, unsigned int *readBytes);
void decode_bytes(char *str, int len, int *key);

int _tmain(int argc, _TCHAR* argv[])
{
	if(argc != 3){
		printf("Use: IGAExtractor <IGA FILE> <output_folder_name>\n");
		return 0;
	}

	FILE *fp;
	_wfopen_s(&fp, argv[1], L"rb");

	char buffer[BUFSIZE];
	unsigned long blocksize;
	unsigned long readsize = 16;
	unsigned int rdsize;
	char *fnamestr;

	if(fp != NULL){
		fread(buffer, 1, 4, fp);
		buffer[4] = '\0';

		if(strcmp("IGA0", buffer) != 0)
			printf("Not support IGA File");
		else{
			fseek(fp, 16L, SEEK_SET);

			// Block Size Metadata 
			blocksize = meta_decode(fp, 5, &rdsize);
			std::list<unsigned int> size_metadata = meta_decode_loop(fp, blocksize);
			int num_files = size_metadata.size() / 3;
			BLOCKSIZE_METADATA *blockmeta = (BLOCKSIZE_METADATA *)malloc(sizeof(BLOCKSIZE_METADATA) * num_files);

			int i = 0;
			
			for(auto it = size_metadata.begin(); it != size_metadata.end(); it++){
				blockmeta[i].filename_offset = *it;
				it++;
				blockmeta[i].start_offset = *it;
				it++;
				blockmeta[i++].file_len = *it;
			}

			readsize += blocksize + rdsize;
			fseek(fp, readsize, SEEK_SET);

			// All Filename String
			// Filename splitted by BLOCKSIZE_METADATA.filename_offset
			blocksize = meta_decode(fp, 5, &rdsize);
			std::list<unsigned int> filename_metadata = meta_decode_loop(fp, blocksize);

			fnamestr = (char *)malloc(filename_metadata.size());
			i = 0;
			for(auto it = filename_metadata.begin(); it != filename_metadata.end(); it++){
				fnamestr[i++] = (char)(*it);
			}

			// Packed Files Binary Data Region
			readsize += blocksize + rdsize;
			fseek(fp, readsize, SEEK_SET);

			char filename[256];
			char path[512];
			bool ret;
			int numConvert, filenamePos, remainingBytes, readBytes, len;
			FILE *fout;
			int encKey;
				
			if(wcstombs_s((size_t *)&numConvert, path, (size_t)512, argv[2], (size_t)512) != 0){
				printf("cannot initialize path");
				exit(1);
			}
				
			ret = CreateDirectoryA(path, NULL);
			if(ret == 0 & GetLastError() != ERROR_ALREADY_EXISTS){
				wprintf(L"Cannot create directory %s", argv[1]);
				exit(1);
			}
			strcat(path, "/");
			filenamePos = numConvert;

			for(i = 0; i < num_files; i++){
				int len;
				if(i == num_files - 1)
					len = filename_metadata.size() - blockmeta[i].filename_offset;
				else
					len = blockmeta[i+1].filename_offset - blockmeta[i].filename_offset;

				memcpy(filename, &fnamestr[blockmeta[i].filename_offset], len);
				filename[len] = '\0';

				strcpy(&path[filenamePos], filename);
				fopen_s(&fout, path, "wb");
				if(fout == NULL)
				{
					printf("Cannot create file %s", path);
					exit(1);
				}

				remainingBytes = blockmeta[i].file_len;
				encKey = 1;

				while(!feof(fp) && remainingBytes > 0){
					len = remainingBytes > BUFSIZE ? BUFSIZE : remainingBytes;
					readBytes = fread(buffer, 1, len, fp);
					
					decode_bytes(buffer, readBytes, &encKey);

					fwrite(buffer, 1, readBytes, fout);
					remainingBytes -= readBytes;
				}

				fclose(fout);
				printf("Write %s\n", filename);
			}
		}

		fclose(fp);
	}
	else
		wprintf(L"Cannot open file %s", argv[1]);
}

std::list<unsigned int> meta_decode_loop(FILE *fp, unsigned int maxLength){
	unsigned int temp_ans;
	bool has_value = false;

	unsigned int counter = 0;
	unsigned int readBytes;

	std::list<unsigned int> temp;
	
	while(counter < maxLength){
		temp_ans = meta_decode(fp, 8, &readBytes);
		temp.push_back(temp_ans);
		counter += readBytes;
	}

	if(has_value)
		temp.push_back(temp_ans);

	return temp;
}

unsigned int meta_decode(FILE *fp, unsigned int maxLength, unsigned int *readBytes){
	unsigned char b;
	unsigned int acc = 0;

	unsigned int counter = 0;
	
	while(counter < maxLength){
		fread(&b, 1, 1, fp);
		counter++;
		acc = (acc << 7) | b;
		if(acc & 1 == 1){
			acc = acc >> 1;
			break;
		}
	}

	if(readBytes != NULL)
		*readBytes = counter;

	return acc;
}

void decode_bytes(char *str, int len, int *key){
	for(int i = 0; i < len; i++){
		*key = (*key + 1) % 256;
		str[i] = str[i] ^ *key;
	}
}
