/*
 * Backup iN Large
 * use to backup big file like VM or TC volume by updating
 * only small blocks that have changed, based on CRC32
 *
 * Copyright (C) 2015 - Frédéric Lamorce frederic.lamorce@gmail.com
 *
 * This file is part of Backup iN Large
 *
 * Backup iN Large is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Backup iN Large is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Backup iN Large.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <zlib.h>

static char debug=0;
#define Dlog(M, ...)	do { if(debug) {fprintf(stderr, M "\n", ##__VA_ARGS__); } } while(0)

// return filesize in bytes
long long fsize(FILE *f)
{
		long long filesize;
		off_t cur_offset = ftello(f);		// save current offset
		
		fseeko(f, 0, SEEK_END);   			// may be non-portable
    filesize=ftello(f);
    
    fseeko(f, cur_offset, SEEK_SET);	// reset to previous offset
    
    return filesize;
}

// returned number of necessary blocks to fully store filesize
// rounded up
unsigned int nb_blocks(long long filesize, unsigned long blocksize)
{
	unsigned int nb;
	
	nb=(long long)(filesize / blocksize);
	if((long long)(filesize % blocksize))
		nb++;

	return nb;
}

// create a CRC file for a given file and block size
int create_crc_file(char *filename, char *filename_crc, unsigned long bs)
{
	FILE *f;
	FILE *fcrc;						// CRC destination file 
	unsigned int i, nb;
	long long filesize;
	char *buf;						// buffer to read block
	size_t bufread;
	unsigned long cur_crc=crc32(0L, Z_NULL, 0);	// current block CRC

	Dlog("C opening file %s", filename);
	f=fopen(filename, "rb");
	if(f==NULL)
	{
			fprintf(stderr, "Cannot open file %s\n", filename);
			return -1;
	}
	
	// open CRC destination file
	fcrc=fopen(filename_crc, "wb");
	if(fcrc==NULL)
	{
		fprintf(stderr, "Cannot create CRC dest file %s\n", filename_crc);
		return -2;
	}
	
	// get file size
  filesize=fsize(f);
  Dlog("C size %llu bytes", filesize);

	// calculate number of block required
	nb=nb_blocks(filesize, bs);
	Dlog("C number of blocks required : %u", nb);

	fwrite(&bs, 1, sizeof(unsigned long), fcrc);

	buf=(char*)malloc(bs);
	// loop to read the nb blocks
	for(i=0; i<nb; i++)
	{
			bufread=fread(buf, 1, bs, f);
			// calculate CRC for block
			cur_crc=crc32(0, buf, bufread);
			Dlog("C block %05u : %08lX", i, cur_crc);
			fwrite(&cur_crc, 1, sizeof(unsigned long), fcrc);
	}

	// cleanup
	free(buf);
#ifdef __linux__
	syncfs(fileno(fcrc));
#endif
	fclose(fcrc);
	fclose(f);

	return 0;
}

int main(int argc, char *argv[])
{
	int opt;
	char *source, *dest, *dest_crc;
	FILE *fs, *fd;				// source, destination
	FILE *fcrc=NULL;			// CRC destination file 
	unsigned int bsMB=8;	// block size in MB, default 8MB
	unsigned long bs=bsMB<<20;			// block size in bytes, default 8MB
	unsigned int i, nb;
	long long filesize;
	char *buf;						// buffer to read block
	size_t bufread;
	unsigned long cur_crc=crc32(0L, Z_NULL, 0);	// current block CRC
	unsigned long *crc_block;		// array CRC for each blocks
	char changed=0;
	char create_dest=0;
	
	while ((opt = getopt(argc, argv, "b:dh")) != -1)
	{
		switch (opt)
		{
			case 'd':
				debug=1;
				break;
			case 'b':
				bsMB=strtoul(optarg, NULL, 0);
				bs=bsMB<<20;
				Dlog("setting block size to %u MB from arg", bsMB);
				if(bsMB!=0)
					break;
			case 'h':
			default: /* '?' */
				fprintf(stderr, "Usage: %s [-h] [-d] [-b n] <source> <destination>\n", argv[0]);
				fprintf(stderr, "       -h   : this help screen\n");
				fprintf(stderr, "       -d   : enable debug log\n");
				fprintf(stderr, "       -b n : set block size to n MB for creation (default 8)\n");
				exit(-1);
		}
	}

	source = argv[optind];
	dest = argv[optind+1];

	if(optind >= argc || source==NULL || dest==NULL)
	{
        fprintf(stderr, "Expected source and destination file names\n");
        exit(-1);
  }

  // name for the CRC file
	dest_crc=(char*)malloc(strlen(dest)+5);
	sprintf(dest_crc, "%s.crc", dest);

	// we need the source file, can't do anything without it
	Dlog("opening source file %s", source);
	fs=fopen(source, "rb");
	if(fs==NULL)
	{
			fprintf(stderr, "Cannot open source file %s\n", source);
			exit(-2);
	}

	// open destination file
	Dlog("opening destination file %s", dest);
	fd=fopen(dest, "r+b");
	if(fd==NULL)
	{
			// it's ok if the file does not exist, we create it
			fprintf(stderr, "Destination file %s does not exist, create it\n", dest);
			create_dest=1;	// remember we are creating it, mostly for display purpose
			fd=fopen(dest, "wb");
			if(fd==NULL)
			{
				// if we really cannot create the file, no other choice than exit
				fprintf(stderr, "Cannot create dest file %s\n", dest);
				exit(-3);
			}
	}
  else
  {
    // Destination file exists! open its associated CRC destination file
	  Dlog("opening destination CRC file %s", dest_crc);
	  fcrc=fopen(dest_crc, "rb");
	  if(fcrc==NULL)
	  {
		  // CRC file does not exist, but destination file exists
      // so let's create CRC file based on its content,
      // read >> write so if it's already a backup, it will be
      // faster to write only changed blocks.
      Dlog("creating CRC file %s for %s", dest_crc, dest);
		  if(create_crc_file(dest, dest_crc, bs)<0)
		  {
				fprintf(stderr, "Failed to create CRC file %s for %s", dest_crc, dest);
				exit(-4);
			}
		  // file now exists, re-open it
		  fcrc=fopen(dest_crc, "rb");
		  if(fcrc==NULL)	// you kidding me?
		  {
				fprintf(stderr, "Error opening CRC file %s\n", dest_crc);
				exit(-5);
			}
	  }

	  // read blocksize from file
	  bufread=fread(&bs, 1, sizeof(unsigned long), fcrc);
    if(ferror(fcrc))
    {
      fprintf(stderr, "Error reading block size from CRC file %s\n", dest_crc);
      exit(-6);
    }
  }

	bsMB=bs>>20;
	Dlog("setting blocksize at %u MB", bsMB);
	
	// get source file size
  filesize=fsize(fs);
  Dlog("source size %llu bytes", filesize);

	// calculate number of block required
	nb=nb_blocks(filesize, bs);
	Dlog("number of blocks required : %u", nb);

	// allocate memory for the number of block
	crc_block=(unsigned long*)calloc(nb, sizeof(unsigned long));
	// read array of dest CRC in memory, if CRC file is opened and valid
  if(fcrc)
  {
  	bufread=fread(crc_block, 1, nb*sizeof(unsigned long), fcrc);
  	fclose(fcrc);
  }

	// allocate for the buffer size
	buf=(char*)malloc(bs);
	// loop to read the nb blocks
	for(i=0; i<nb; i++)
	{
			bufread=fread(buf, 1, bs, fs);
			if(ferror(fs))
			{
				fprintf(stderr, "Error %d reading source file %s\n", errno, source);
				exit(-10);
			}
			// calculate CRC for source block
			cur_crc=crc32(0, buf, bufread);
			if(create_dest)
				Dlog("block %05u : source %08lX", i, cur_crc);
			else
				Dlog("block %05u : source %08lX, dest %08lX", i, cur_crc, crc_block[i]);
			// test if current CRC differs from destination CRC block
			// or force write if we have to create destination
			if(create_dest || cur_crc != crc_block[i])
			{
				if(create_dest)
					printf("Creating block %05u\n", i);
				else
					printf("Block %05u differ from %08lX, copying block\n", i, crc_block[i]);
				// seek at right block position to write block
				fseeko(fd, bs*i, SEEK_SET);
				fwrite(buf, 1, bufread, fd);
				if(ferror(fd))
				{
					fprintf(stderr, "Error %d writing destination file %s\n", errno, dest);
					exit(-11);
				}
				// update CRC
				crc_block[i]=cur_crc;
				changed=1;
			}
			//printf("%u%%\r", (i*100)/nb);
	}
	//printf("100%%\n");
	
	// flush new CRC array if needed
	if(changed)
	{
		Dlog("creating destination CRC file %s", dest_crc);
		fcrc=fopen(dest_crc, "wb");
		if(fcrc==NULL)
		{
			fprintf(stderr, "Error creating CRC file %s\n", dest_crc);
			exit(-12);
		}
		fwrite(&bs, 1, sizeof(unsigned long), fcrc);
		fwrite(crc_block, 1, nb*sizeof(unsigned long), fcrc);
#ifdef __linux__
		syncfs(fileno(fcrc));
#endif
		fclose(fcrc);
#ifdef __linux__
		// as something changed in destination, flush the file too
		syncfs(fileno(fd));
#endif
	}
	puts("done.");

	// cleanup
	free(dest_crc);
	free(buf);
	free(crc_block);
	fclose(fd);
	fclose(fs);

	return 0;
}
