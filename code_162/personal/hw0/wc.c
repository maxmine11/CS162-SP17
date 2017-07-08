/* Print  newline,  word,  and  byte counts for each FILE, and a total
       line if more than one FILE is specified. A  word is a non-zero-length
       sequence of characters delimited by white space. */

#include <stdio.h>
#include <stdlib.h>


FILE *myfile;
char ch;
int lines, words, bytes;
// State  works as a cheker to see if the state is in a word (state =1) or not in a word (state=2)
int state;      
int main(int argc, char *argv[]) {

	// Reading the file input in shell from argv[1] if and only if there are any argumets
	if(argc==2){ myfile=fopen(argv[1],"r");}
	myfile=stdin;
	// Initialize our count veriables
	lines=0;
	words=0;
	bytes=0;
	state=0;
	//First check if the file will open succesfully
	//Then we will proceed to check the characters until the end of the
	// file is reached
	if (myfile){
		//While loop will run until the end of the file is reached
		while((ch=fgetc(myfile))!= EOF){
			//Lets first keep the count of words there are
			if (ch==' ' || ch=='\t' || ch=='\n'){
				state=0;
			}
			else if (state==0){
				state=1;
				++words;
			}
			
			//Keep the count of the lines in the file
			if (ch=='\n'){
				++lines;
			}

			//Keep the count of how many bytes in the file
			++bytes; 
		}
	}

	else {
		printf("Not file or string inputed\n");
		exit(1);
	}
	if(myfile != stdin){
		fclose(myfile);
	}
	printf("%d %d %d",lines,words,bytes);
    	return 0;
}
