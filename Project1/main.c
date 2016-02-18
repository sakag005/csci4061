#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h> 
#include <unistd.h>

#include "util.h"

#define MAX_DEPENDS 10

//linked list
static list_item* first;

static int commands[3] = {0, 0, 0};

int hasZero(list_item * list)
{
	list_item* current = list;
	while(current != NULL)
	{
		Node nd = *((Node *)current->item);
		if(nd.numParents == 0){
			return 1; //has zero, is not clean
		}
		current = current->next;
	}
	return 0; //does not have zero  
}

int forkExec(Node **toBeExeced, int numElements){
	int i;
	int k = 0;
	int comp;
	
	for(i=0;i<numElements;i++){
		char **execargv;
		pid_t childpid;
		int execargc;
		int status;
		int recompile = 1;
		//if recompile is 1, then build
                if(commands[1] == 0){
                        printf("no -B]\n");
                        recompile = 0;
                        for(k;k<toBeExeced[i]->sizeDepends;k++){
                                comp = compare_modification_time(toBeExeced[i]->dependencies[k],toBeExeced[i]->target);
				int child_timestamp = get_file_modification_time(toBeExeced[i]->dependencies[k]);
				int parent_timestamp = get_file_modification_time(toBeExeced[i]->target);
                                printf("child timestamp: %d \n", child_timestamp);
                                printf("parent timestamp: %d \n", parent_timestamp);
                                printf("comp: %d \n",comp);
                                printf("recompile: %d \n",recompile);
				//if the timestamp for one doesn't exist, or the timestamp of the child is greater (newer) than the parent
                                if(comp < 2){
                                        recompile = 1;

                                }
                                printf("recompile: %d \n",recompile);
			}
			}
		//if child is older than the parent, don't rebuild
		if(recompile == 0){
			continue;
		}
		
		if (strcmp (toBeExeced[i]->command, " ") != 0){	
			if(commands[0] == 1){
				printf("%s\n", toBeExeced[i]->command);
				if (toBeExeced[i]->toParent != NULL){
					toBeExeced[i]->toParent->numParents--;
				}
				continue;
			}
			execargc = makeargv (toBeExeced[i]->command," ",&execargv);
			toBeExeced[i]->pid = childpid = fork();
		}else
		{
			continue;
		}
		if (childpid == -1){
			perror("Failed to Fork\n");
			return -1;
		}
		if(childpid ==0){
			printf("building\n");
			execvp(execargv[0],&execargv[0]);
			perror("Child failed to Exec \n");
			return -1;
		}
		if(childpid > 0){
			wait(&status);
			if(toBeExeced[i]->toParent != NULL){
				toBeExeced[i]->toParent->numParents--;
			}
		}
		freemakeargv(execargv);
	}
	return 1;
}

int run(){
	list_item* copy = first;

	list_item* save = copy;
	int r;	
	while(hasZero(copy))
	{
		int i = 0;
		Node** nodes_to_execute = malloc(sizeof(Node) * MAX_DEPENDS);
		while(copy != NULL)
		{
			Node nd = *((Node *)copy->item);
			if(nd.numParents == 0){
				((Node*)copy->item)->numParents = -10; //has zero, is not clean
				//nodes_to_execute[i] = &nd;
				nodes_to_execute[i] = ((Node*)copy->item);
				i++;
			}
			copy = copy->next;
		}  
		r = forkExec(nodes_to_execute,i);
		free(nodes_to_execute);
		copy = save;
	}
	return r;
}

void assignParents()
{
	list_item* current = first;
	while(current != NULL)
	{
		Node* nd = ((Node *)current->item);		
		int depCounter = 0;
		
		list_item* others = first;
		while(others != NULL)
		{
			Node* otherND = ((Node *)others->item);
			
			int j;
			for(j = 0; j < nd->sizeDepends; j++)
			{
				if(strcmp(otherND->target, nd->dependencies[j]) == 0)
				{
					otherND->toParent = nd;
					depCounter++;
				}
			}
			
			others = others->next;
		}
		
		nd->numParents = depCounter;
		current = current->next;
	}
}

void free_node(Node* other)
{
	if(other->target != NULL) free(other->target);
	if(strcmp(other->command,  " ") != 0)
		free(other->command);

	int i;
	for(i = 0; i < other->sizeDepends; i++)
		free(other->dependencies[i]);

	if((other->sizeDepends)>0) free(other->dependencies);
	
	free(other);
}

void freeEverything()
{
	list_item* current = first;
	list_item* temp;
	while(current != NULL)
	{
		temp = current;
		current = current->next;
		Node* nd = ((Node *)temp->item);
		
		free_node(nd);
		free(temp);
	}
}

int removeNonTargets(char* argv)
{
	// build list of targets to keep
	char* target = argv;
	list_item* current = first;
	list_item* n_first = NULL;
	
	Node* nd;
	//find the node in the linked list structure
	while(current != NULL)
	{
		nd = ((Node *)current->item);
		if(strcmp(target, nd->target) == 0)
		{
			//capture current location in case the rest of the list is deleted
			n_first = current;
			break;
		}
		
		current = current->next;
	}
	if(n_first == NULL) return -1;
	
	//We will not be executing anything after target
	nd->toParent = NULL;
	
	if(nd->numParents == 0)
	{
		current = first;
		list_item* temp;
		while(current != NULL)
		{
			temp = current;
			current = current->next;
			
			Node* other = ((Node *)temp->item);
			//free irrelevant nodes
			if(nd != other)
			{
				free_node(other);
				if(temp != first) free(temp);
			}
		}
		
		if(first != n_first)
		{	
			free(first);
			first = n_first;
		}
		first->next = NULL;
	}else
	{
		
		current = first;
		list_item* temp;
		list_item* lastValid = first;
		while(current != NULL)
		{
			temp = current;
			current = current->next;
			
			Node* other = ((Node *)temp->item);
			
			//look for dependencies
			int dependsOn = 0;
			
			Node* check = other;
			while(check != NULL)
			{
				if(check == nd)
				{
					dependsOn = 1;
					break;
				}
				check = check->toParent;
			}
			
			//keep in global list if dependency, otherwise delete from list
			if(dependsOn)
			{
				lastValid = temp;
				
			}else if(temp == first){ //want to update first element in linked list if it needs to be deleted
				first = current;
				lastValid = first;			
				free_node(other);
				free(temp);
			}else
			{
				free_node(other);
				lastValid->next = temp->next;
				free(temp);
			}
		}
	}
	
	return 1;
}

//This function will parse makefile input from user or default makeFile. 
int parse(char * lpszFileName, char** defTarget)
{

	int nLine=0;
	char szLine[1024];
	char * lpszLine;
	FILE * fp = file_open(lpszFileName);

	if(fp == NULL)
	{
		return -1;
	}
	
	Node* nd;
	//This parsing is currently unfinished
	while(file_getline(szLine, fp) != NULL) 
	{
		static int firstNode = 1;
		static int lastTab = 1;
		nLine++;
		// this loop will go through the given file, one line at a time
		// this is where you need to do the work of interpreting
		// each line of the file to be able to deal with it later
		
		//Remove newline character at end if there is one
		lpszLine = strtok(szLine, "\n"); 
		//printf("%s\n",lpszLine);
		
		if(lpszLine == NULL)
			continue;
		
		if(lpszLine[0] == ' '){
			int i;
			for(i = 1; i<strlen(lpszLine); i++)
			{
				if(lpszLine[i] == '#')
				{
					break;
				}else if(lpszLine[i] != ' ')
				{
					printf("ERROR ERROR LINE INCORRECT SYNTAX \n");
					return -1;
				}
			}
			continue;
		}

		//check if comment
		if(lpszLine[0] == '#'){
			continue;
		}
		
		//if there is no tab, it's a target line
		if(lpszLine[0] != '\t')
		{
			if(lastTab == 0)
			{
				nd->command = " ";
				
				if(firstNode)
				{
					/*if((*defTarget = (char *)malloc(sizeof(char)*strlen(nd->target))) == NULL)
					{	
						printf("ERROR: Insufficient memory");
						return -1;
					}
					strcpy(*defTarget, nd->target);*/
					*defTarget = nd->target;
					//printf("WHAT IS THIS %s\n", *defTarget);

					list_item* new_item;
					if((new_item = (list_item *)malloc(sizeof(list_item))) == NULL)
					{	
						printf("ERROR: Insufficient memory\n");
						return -1;
					}

					new_item->item = (void *)nd;
					new_item->next = NULL;
					first = new_item;
					firstNode = 0;
				}else
				{
					list_item* new_item;
					if((new_item = (list_item *)malloc(sizeof(list_item))) == NULL)
					{	
						printf("ERROR: Insufficient memory\n");
						return -1;
					}
					
					new_item->item = (void *)nd;
					new_item->next = first;
					first = new_item;
				}
			}
			
			if((nd  = (Node*)malloc(sizeof(Node))) == NULL)
			{	
				printf("ERROR: Insufficient memory\n");
				return -1;
			}
			nd->toParent = NULL;
			nd->numParents = 0;
			//look for colon
			char* col = strchr(lpszLine, ':');
		
			if(col != NULL)
			{					
				//copy target
				if((nd->target = (char *)malloc(((int)(col - lpszLine)) * sizeof(char))) == NULL)
				{	
					printf("ERROR: Insufficient memory\n");
					return -1;
				}
				
				strncpy(nd->target, lpszLine, (int)(col - lpszLine));

				nd->sizeDepends = 0;
				
				int sizeDep = strlen(col);
				
				int j;
				for(j = 1; j < sizeDep; j++)
				{
					if((col[j] != ' ') && (col[j-1] == ' '))
						nd->sizeDepends++;
				}

				//copy dependencies
				if((nd->dependencies = (char **)malloc(nd->sizeDepends * sizeof(char*))) == NULL)
				{	
					printf("ERROR: Insufficient memory\n");
					return -1;
				}

				int i = 0;
				int k = 0;
				for(i = 1; i < sizeDep; i++)
				{
					if((col[i] != ' ') && (col[i-1] == ' '))
					{
						char* end = strchr(&col[i], ' ');
						if(end != NULL)
						{
							if((nd->dependencies[k] = (char *)malloc(((int)(end - &col[i])) * sizeof(char))) == NULL)
							{	
								printf("ERROR: Insufficient memory\n");
								return -1;
							}
							strncpy(nd->dependencies[k], &col[i], ((int)(end - &col[i])));
							k++;
						}else if(i<sizeDep)
						{
							if((nd->dependencies[k] = (char *)malloc((sizeDep - i) * sizeof(char))) == NULL)
							{	
								printf("ERROR: Insufficient memory\n");
								return -1;
							}
							strncpy(nd->dependencies[k], &col[i], (sizeDep - i));
							break;
						}
					}
				}

			}
			
			lastTab = 0;
			
		}
		if(lpszLine[0] == '\t')
		{
			//this is a command, so copy the whole line
			
			if((nd->command = (char*)malloc(strlen(lpszLine+1)*sizeof(char))) == NULL)
			{
				printf("ERROR: Insufficient memory\n");
				return -1;
			}
			
			strcpy(nd->command, lpszLine+1);
			
			//add node to the front of the global linked list
			if(firstNode)
			{
				/*if((*defTarget = (char *)malloc(sizeof(char)*strlen(nd->target))) == NULL)
				{	
					printf("ERROR: Insufficient memory");
					return -1;
				}
				strcpy(*defTarget, nd->target);*/
				*defTarget = nd->target;
				
				list_item* new_item;
				if((new_item = (list_item *)malloc(sizeof(list_item))) == NULL)
				{	
					printf("ERROR: Insufficient memory\n");
					return -1;
				}

				new_item->item = (void *)nd;
				new_item->next = NULL;
				first = new_item;
				firstNode = 0;
			}else
			{
				list_item* new_item;
				if((new_item = (list_item *)malloc(sizeof(list_item))) == NULL)
				{	
					printf("ERROR: Insufficient memory\n");
					return -1;
				}

				new_item->item = (void *)nd;
				new_item->next = first;
				first = new_item;
			}
			
			lastTab = 1;
			
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
	list_item* current = first;
	
	while(current != NULL)
	{
		Node nd = *((Node *)current->item);		
		printf("target:  %s\n", nd.target);
		printf("\tcommand:  %s\n", nd.command);
		printf("\tnumParents: %d\n", nd.numParents);
		if(nd.toParent != NULL)
			printf("\tNext node: %s\n", nd.toParent->target);
		
		current = current->next;
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
				commands[0] = 1;
				break;
			case 'B':
				commands[1] = 1;
				break;
			case 'm':
				commands[2] = 1;
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
	
	//redirect output if option is given
	if(commands[2])
	{
		int fd;
		if((fd = open(szLog, O_CREAT | O_WRONLY, 0644)) == -1)
		{
			printf("Error opening log file\n");
			return EXIT_FAILURE;
		}
		if(dup2(fd, 1) == -1)
		{
			printf("Error redirecting the output\n");
			return EXIT_FAILURE;
		}
		if(close(fd) == -1)
		{
			printf("Error closing log file\n");
			return EXIT_FAILURE;
		}
	}
	
	
	
	if(argc > 1)
	{
		show_error_message(argv[0]);
		return EXIT_FAILURE;
	}

	char* defTarget;
	/* Parse graph file or die */
	if((parse(szMakefile, &defTarget)) == -1) 
	{
		return EXIT_FAILURE;
	}

	assignParents();

	//You may start your program by setting the target that make4061 should build.
	//if target is not set, set it to default (first target from makefile)
	if(argc == 1)
	{
		if(removeNonTargets(argv[0]) == -1)
		{
			printf("Error: target was not found");
			return EXIT_FAILURE;
		}
	}
	else
	{
		if(removeNonTargets(defTarget) == -1)
		{
		printf("Error: target was not found");
			return EXIT_FAILURE;
		}
	}

	//printNodes();
	
	if(run() == -1){
		show_error_message(argv[0]);
		return EXIT_FAILURE;
	}

	//free all data structures
	freeEverything();
	
	//after parsing the file, you'll want to check all dependencies (whether they are available targets or files)
	//then execute all of the targets that were specified on the command line, along with their dependencies, etc.
	return EXIT_SUCCESS;
}
