#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h> 
#include <unistd.h>

#include "util.h"

//linked list
static list_item* first;
static list_item* current;

//This function will parse makefile input from user or default makeFile. 
int parse(char * lpszFileName)
{
	int nLine=0;
	char szLine[1024];
	char * lpszLine;
	FILE * fp = file_open(lpszFileName);

	if(fp == NULL)
	{
		return -1;
	}
	
	//This parsing is currently unfinished
	while(file_getline(szLine, fp) != NULL) 
	{
		static int firstNode = 1;
		nLine++;
		// this loop will go through the given file, one line at a time
		// this is where you need to do the work of interpreting
		// each line of the file to be able to deal with it later
		
		//Remove newline character at end if there is one
		lpszLine = strtok(szLine, "\n"); 

		
		while(lpszLine != NULL)
		{	
			Node* nd;
			//if there is no tab, it's a target line
			if(lpszLine[0] != '\t')
			{
				nd  = (Node*)malloc(sizeof(Node));
				//look for colon
				char* col = strchr(lpszLine, ':');
			
				if(col != NULL)
				{					
					//copy target
					nd->target = (char *)malloc(((int)(col - lpszLine)) * sizeof(char));
					strncpy(nd->target, lpszLine, (int)(col - lpszLine));

					nd->numParents = 0;
					
					int sizeDep = strlen(col);
					
					int j;
					for(j = 1; j < sizeDep; j++)
					{
						if((col[j] != ' ') && (col[j-1] == ' '))
							nd->numParents++;
					}
	
					//copy dependencies
					nd->dependencies = (char **)malloc(nd->numParents * sizeof(char*));
					int i = 0;
					int k = 0;
					for(i = 1; i < sizeDep; i++)
					{
						if((col[i] != ' ') && (col[i-1] == ' '))
						{
							char* end = strchr(&col[i], ' ');
							if(end != NULL)
							{
								nd->dependencies[k] = (char *)malloc(((int)(end - &col[i])) * sizeof(char));
								strncpy(nd->dependencies[k], &col[i], ((int)(end - &col[i])));
								k++;
							}else if(i<sizeDep)
							{
								nd->dependencies[k] = (char *)malloc((sizeDep - i) * sizeof(char));
								strncpy(nd->dependencies[k], &col[i], (sizeDep - i));
								break;
							}
						}
					}

				}
				
				lpszLine = strtok(NULL, "\n");
			}
			if((lpszLine != NULL) && (lpszLine[0] == '\t'))
			{
				//this is a command, so copy the whole line
				nd->command = (char*)malloc(strlen(lpszLine+1)*sizeof(char));
				strcpy(nd->command, lpszLine+1);

				//add node to the front of the global linked list
				if(firstNode)
				{
					list_item* new_item = (list_item *)malloc(sizeof(list_item));

					new_item->item = (void *)nd;
					new_item->next = NULL;
					current = new_item;
					first = current;
					firstNode = 0;
				}else
				{
					list_item* new_item = (list_item *)malloc(sizeof(list_item));

					new_item->item = (void *)nd;
					new_item->next = current;
					current = new_item;
					first = current;
				}
				
				lpszLine = strtok(NULL, "\n");
			}else
			{
				nd->command = " ";
				//add node to the front of the global linked list
				if(firstNode)
				{
					list_item* new_item = (list_item *)malloc(sizeof(list_item));

					new_item->item = (void *)nd;
					new_item->next = NULL;
					current = new_item;
					first = current;
					firstNode = 0;
				}else
				{
					list_item* new_item = (list_item *)malloc(sizeof(list_item));

					new_item->item = (void *)nd;
					new_item->next = current;
					current = new_item;
					first = current;
				}
			}
		}
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

	//Close the makefile. 
	fclose(fp);

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

void printNodes()
{
	current = first;
	int i = 0;
	while(current != NULL)
	{
		Node nd = *((Node *)current->item);		
		printf("target:%s:end\n", nd.target);
		printf("\tcommand:%s:end\n", nd.command);
		current = current->next;
		int j;
		for(j = 0; j < nd.numParents; j++)
		{
			printf("\tdependency:%s:end\n", nd.dependencies[j]);
		}

		i++;
	}
}

int main(int argc, char **argv) 
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

	first = current;
	/* Parse graph file or die */
	if((parse(szMakefile)) == -1) 
	{
		return EXIT_FAILURE;
	}

	printNodes();

	//after parsing the file, you'll want to check all dependencies (whether they are available targets or files)
	//then execute all of the targets that were specified on the command line, along with their dependencies, etc.
	return EXIT_SUCCESS;
}
