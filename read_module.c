/* Copyright (C) 2015, Santiago F. Maudet
 * This file is part of char01 module.
 *
 * char01 module is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * char01 module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* !\brief
*   This  is a user application to read data from the module buffer.
*   USAGE: ./test_module /dev/char_01
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define BUF_SIZE 200

int main (int argc, char ** argv){
  int fd;
  int index =-1, nbytes=0;
  char data[BUF_SIZE];

  if(argc < 2){
    perror("usage: ./readers /dev/[device_name]\n");
    return -1;
  }

  fd = open(argv[1],O_RDONLY);

  if( fd == -1){
    perror("Error openning device\n");
    return -1;
  }
  nbytes=0;
  while( index != 0){
    index=read(fd,data,BUF_SIZE);
    if (index > 0){
	printf("Leido:%s, equivalen a %i\n",data,index);
	nbytes+=index;
	printf("Nbytes: %i\n",nbytes);
    } 
    sleep(1);
  }
  printf("Fin del lectura.\n");
	
  return 0;
}
