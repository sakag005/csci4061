#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h> 
#include <unistd.h>
#include <stdbool.h>

#include "util.h"

//for some reason I get a seg fault when I close the file (last line)
//only printing, will add structure later
int parse(char * lpszFileName)
{
	int nLine=0;
	char szLine[1024];
	char * lpszLine;
	FILE * fp = file_open(lpszFileName);

	bool afterTarget = false;	
	char * previousTarget;
	int i;
	

	if(fp == NULL)
	{
		return -1;
	}

	while(file_getline(szLine, fp) != NULL) 
	{
		printf("line %d first char: %c \n", nLine, lpszLine[0]);
		nLine++;
		// this loop will go through the given file, one line at a time
		// this is where you need to do the work of interpreting
		// each line of the file to be able to deal with it later
		//check if empty line
		if(strlen(lpszLine) == 1){
			continue;
		}
		
		//Remove newline character at end if there is one
		lpszLine = strtok(szLine, "\n"); 
		
		//check if first char is space

		if(lpszLine[0] == ' '){
			while(isspace(*lpszLine)) lpszLine++; //continue until not space
			if( (*lpszLine=='\0') || (*lpszLine=='#') ){ //comment OK
				continue; //skip line
			}else{
				printf("ERROR ERROR LINE INCORRECT SYNTAX \n");
				return 0;
			}
		}

		//check if comment
		if(lpszLine[0] == '#'){
			continue;
		}

		//if it IS a command -> build node with previous target 
		//here print previous target + command
		if(lpszLine[0] == '\t'){
			if(afterTarget == false){
				printf("ERROR command doesn't follow target/dep \n");
				return 0;
			}
			//printf("target: %s \n", previousTarget);
			//get command
			char * command = "";
			int l = strlen(lpszLine);
			for(i=0;i<l;i++){
				char c = lpszLine[i];
				size_t len = strlen(command);
				char * str2 = malloc(len+1+1);
				strcpy(str2,command);
				str2[len]=c;
				str2[len+1] = '\0';
				len = strlen(str2);
				command = malloc(len+1+1);
				strcpy(command,str2);
				free(str2);
			}
			printf("command: %s with target: %s \n", command, previousTarget);
			afterTarget = false;
			free(command);
			continue;
		}

		//check if line is target/depend
		char * target = "";
		int l = strlen(lpszLine);
		for(i=0;i<l;i++)
		{
			char c = lpszLine[i];
			size_t len = strlen(target);
			char * str2 = malloc(len+1+1);
			strcpy(str2,target);
			str2[len]=c;
			str2[len+1] = '\0';
			len = strlen(str2);
			target = malloc(len+1+1);
			strcpy(target,str2);
			afterTarget = true;
			previousTarget = malloc(strlen(target));
			strcpy(previousTarget,target);
			free(str2);
		}
		printf("target: %s \n", target);
		free(target);
		


		//printf(lpszLine);
		
		//You need to check below for parsing. 
		//Skip if blank or comment.
		//Remove leading whitespace.
		//Skip if whitespace-only.
		//Only single command is allowed.
		//If you found any syntax error, stop parsing. 
		//If lpszLine starts with '\t' it will be command else it will be target.
		//It is possbile that target may not have a command as you can see from the example on project write-up. (target:all)
		//You can use any data structure (array, linked list ...) as you want to build a graph
	}

	//Close the makefile. For some reason if this isn't checked it will raise
	//a segfault 
	free(previousTarget);
	if(fp == NULL){
		fclose(fp);
	}

	return 0;
}

void show_error_message(char * lpszFileName)
{
	fprintf(stderr, "Usage: %s [options] [target] : only single target is allowed.\n", lpszFileName);
	fprintf(stderr, "-f FILE\t\tRead FILE as a maumfile.\n");
	fprintf(stderr, "-h\t\tPrint this message and exit.\n");
	fprintf(stderr, "-n\t\tDon't actually execute commands, just print them.\n");
	fprintf(stderr, "-B\t\tDon't check files timestamps.\n");
	fprintf(stderr, "-m FILE\t\tRedirect the output to the file specified .\n");
	exit(0);
}

int main(int argc, char**argv)
{
	parse(argv[1]);
	return 0;	
}

int main1(int argc, char **argv) 
{
	// Declarations for getopt
	extern int optind;
	extern char * optarg;
	int ch;
	char * format = "f:hnBm:";
	
	// Default makefile name will be Makefile
	char szMakefile[64] = "Makefile";
	char szTarget[64];
	char szLog[64];

	while((ch = getopt(argc, argv, format)) != -1) 
	{
		switch(ch) 
		{
			case 'f':
				strcpy(szMakefile, strdup(optarg));
				break;
			case 'n':
				break;
			case 'B':
				break;
			case 'm':
				strcpy(szLog, strdup(optarg));
				break;
			case 'h':
			default:
				show_error_message(argv[0]);
				exit(1);
		}
	}

	argc -= optind;
	argv += optind;

	// at this point, what is left in argv is the targets that were 
	// specified on the command line. argc has the number of them.
	// If getopt is still really confusing,
	// try printing out what's in argv right here, then just running 
	// with various command-line arguments.

	if(argc > 1)
	{
		show_error_message(argv[0]);
		return EXIT_FAILURE;
	}

	//You may start your program by setting the target that make4061 should build.
	//if target is not set, set it to default (first target from makefile)
	if(argc == 1)
	{
	}
	else
	{
	}


	/* Parse graph file or die */
	if((parse(szMakefile)) == -1) 
	{
		return EXIT_FAILURE;
	}

	//after parsing the file, you'll want to check all dependencies (whether they are available targets or files)
	//then execute all of the targets that were specified on the command line, along with their dependencies, etc.
	return EXIT_SUCCESS;
}

