#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "process.h"

// messaging config
int WINDOW_SIZE;
int MAX_DELAY;
int TIMEOUT;
int DROP_RATE;
//test add
// process information
process_t myinfo;
int mailbox_id;
// a message id is used by the receiver to distinguish a message from other messages
// you can simply increment the message id once the message is completed
int message_id = 0;
// the message status is used by the sender to monitor the status of a message
message_status_t message_stats;
// the message is used by the receiver to store the actual content of a message
message_t *message;

int num_available_packets; // number of packets that can be sent (0 <= n <= WINDOW_SIZE)
int is_receiving = 0; // a helper varibale may be used to handle multiple senders

int consecutive_TO = 0; //number of times we have timed out since last ACK 


/**
 * TODO complete the definition of the function
 * 1. Save the process information to a file and a process structure for future use.
 * 2. Setup a message queue with a given key.
 * 3. Setup the signal handlers (SIGIO for handling packet, SIGALRM for timeout).
 * Return 0 if success, -1 otherwise.
 */
int init(char *process_name, key_t key, int wsize, int delay, int to, int drop) {
    myinfo.pid = getpid();
    strcpy(myinfo.process_name, process_name);
    myinfo.key = key;

    // open the file
    FILE* fp = fopen(myinfo.process_name, "wb");
    if (fp == NULL) {
        printf("Failed opening file: %s\n", myinfo.process_name);
        return -1;
    }
    // write the process_name and its message keys to the file
    if (fprintf(fp, "pid:%d\nprocess_name:%s\nkey:%d\n", myinfo.pid, myinfo.process_name, myinfo.key) < 0) {
        printf("Failed writing to the file\n");
        return -1;
    }
    fclose(fp);

    WINDOW_SIZE = wsize;
    MAX_DELAY = delay;
    TIMEOUT = to;
    DROP_RATE = drop;

    printf("[%s] pid: %d, key: %d\n", myinfo.process_name, myinfo.pid, myinfo.key);
    printf("window_size: %d, max delay: %d, timeout: %d, drop rate: %d%%\n", WINDOW_SIZE, MAX_DELAY, TIMEOUT, DROP_RATE);

    // setup a message queue and save the id to the mailbox_id
    if((mailbox_id = msgget(myinfo.key, 0777 | IPC_CREAT)) == -1)
    		return -1;

    // set the signal handler for receiving packets
    //alarm signal handler
    struct sigaction timeout_act;
    sigfillset(&timeout_act.sa_mask);
    timeout_act.sa_handler = timeout_handler;
    
    if(sigaction(SIGALRM, &timeout_act, NULL) == -1)
    		return -1;
    
    //I/O signal handler
    struct sigaction io_act;
    sigfillset(&io_act.sa_mask);
    io_act.sa_handler = receive_packet;
    
    if(sigaction(SIGIO, &io_act, NULL) == -1)
    		return -1;

    return 0;
}

/**
 * Get a process' information and save it to the process_t struct.
 * Return 0 if success, -1 otherwise.
 */
int get_process_info(char *process_name, process_t *info) {
    char buffer[MAX_SIZE];
    char *token;

    // open the file for reading
    FILE* fp = fopen(process_name, "r");
    if (fp == NULL) {
        return -1;
    }
    // parse the information and save it to a process_info struct
    while (fgets(buffer, MAX_SIZE, fp) != NULL) {
        token = strtok(buffer, ":");
        if (strcmp(token, "pid") == 0) {
            token = strtok(NULL, ":");
            info->pid = atoi(token);
        } else if (strcmp(token, "process_name") == 0) {
            token = strtok(NULL, ":");
            strcpy(info->process_name, token);
        } else if (strcmp(token, "key") == 0) {
            token = strtok(NULL, ":");
            info->key = atoi(token);
        }
    }
    fclose(fp);
    return 0;
}

/**
 * Send a packet to a mailbox identified by the local_mailbox_id, and send a SIGIO to the pid.
 * Return 0 if success, -1 otherwise.
 */
int send_packet(packet_t *packet, int local_mailbox_id, int pid) {
	printf("sending packet with contents: %s \n", packet->data);
	printf("sending to mailbox: %d \n", local_mailbox_id);
    if(msgsnd(local_mailbox_id, (void *)packet, sizeof(packet_t), 0) == -1)
    		return -1;
    	
    	if(kill(pid, SIGIO) == -1)
    		return -1;
    	
    	return 0;
}

/**
 * Get the number of packets needed to send a data, given a packet size.
 * Return the number of packets if success, -1 otherwise.
 */
int get_num_packets(char *data, int packet_size) {
    if (data == NULL) {
        return -1;
    }
    if (strlen(data) % packet_size == 0) {
        return strlen(data) / packet_size;
    } else {
        return (strlen(data) / packet_size) + 1;
    }
}

/**
 * Create packets for the corresponding data and save it to the message_stats.
 * Return 0 if success, -1 otherwise.
 */
int create_packets(char *data, message_status_t *message_stats) {
    if (data == NULL || message_stats == NULL) {
        return -1;
    }
    int i, len;
    for (i = 0; i < message_stats->num_packets; i++) {
        if (i == message_stats->num_packets - 1) {
            len = strlen(data)-(i*PACKET_SIZE);
        } else {
            len = PACKET_SIZE;
        }
        message_stats->packet_status[i].is_sent = 0;
        message_stats->packet_status[i].ACK_received = 0;
        message_stats->packet_status[i].packet.message_id = -1;
        message_stats->packet_status[i].packet.mtype = DATA;
        message_stats->packet_status[i].packet.pid = myinfo.pid;
        strcpy(message_stats->packet_status[i].packet.process_name, myinfo.process_name);
        message_stats->packet_status[i].packet.num_packets = message_stats->num_packets;
        message_stats->packet_status[i].packet.packet_num = i;
        message_stats->packet_status[i].packet.total_size = strlen(data);
        memcpy(message_stats->packet_status[i].packet.data, data+(i*PACKET_SIZE), len);
        message_stats->packet_status[i].packet.data[len] = '\0';
    }
    return 0;
}

/**
 * Get the index of the next packet to be sent.
 * Return the index of the packet if success, -1 otherwise.
 */
int get_next_packet(int num_packets) {
    int packet_idx = rand() % num_packets;
    int i = 0;

    i = 0;
    while (i < num_packets) {
        if (message_stats.packet_status[packet_idx].is_sent == 0) {
            // found a packet that has not been sent
            return packet_idx;
        } else if (packet_idx == num_packets-1) {
            packet_idx = 0;
        } else {
            packet_idx++;
        }
        i++;
    }
    // all packets have been sent
    return -1;
}

/**
 * Use probability to simulate packet loss.
 * Return 1 if the packet should be dropped, 0 otherwise.
 */
int drop_packet() {
    if (rand() % 100 > DROP_RATE) {
        return 0;
    }
    return 1;
}

/**
 * TODO Send a message (broken down into multiple packets) to another process.
 * We first need to get the receiver's information and construct the status of
 * each of the packet.
 * Return 0 if success, -1 otherwise.
 */
int send_message(char *receiver, char* content) {
    if (receiver == NULL || content == NULL) {
        printf("Receiver or content is NULL\n");
        return -1;
    }
    // get the receiver's information
    if (get_process_info(receiver, &message_stats.receiver_info) < 0) {
        printf("Failed getting %s's information.\n", receiver);
        return -1;
    }
    // get the receiver's mailbox id
    message_stats.mailbox_id = msgget(message_stats.receiver_info.key, 0666);
    if (message_stats.mailbox_id == -1) {
        printf("Failed getting the receiver's mailbox.\n");
        return -1;
    }
    // get the number of packets
    int num_packets = get_num_packets(content, PACKET_SIZE);
    if (num_packets < 0) {
        printf("Failed getting the number of packets.\n");
        return -1;
    }
    // set the number of available packets
    if (num_packets > WINDOW_SIZE) {
        num_available_packets = WINDOW_SIZE;
    } else {
        num_available_packets = num_packets;
    }
    // setup the information of the message
    message_stats.is_sending = 1;
    message_stats.num_packets_received = 0;
    message_stats.num_packets = num_packets;
    message_stats.packet_status = malloc(num_packets * sizeof(packet_status_t));
    if (message_stats.packet_status == NULL) {
        return -1;
    }
    // parition the message into packets
    if (create_packets(content, &message_stats) < 0) {
        printf("Failed paritioning data into packets.\n");
        message_stats.is_sending = 0;
        free(message_stats.packet_status);
        return -1;
    }
    //printf("number of packets in stats: %d with data %s \n", message_stats.num_packets, &message_stats.packet_status->packet.data);
    message_stats.free_slots = 0;  // setfree slots
    // TODO send packets to the receiver
    // the number of packets sent at a time depends on the WINDOW_SIZE.
    // you need to change the message_id of each packet (initialized to -1)
    // with the message_id included in the ACK packet sent by the receiver
    int i = 0;
    int x;
    message_stats.packet_status[i].packet.packet_num = i;
    send_packet(&message_stats.packet_status[i].packet, message_stats.mailbox_id, message_stats.receiver_info.pid); 
    message_stats.packet_status[i].is_sent = 1;

	printf("first while loop \n");
	
    while (message_stats.packet_status[i].ACK_received == 0){
      if(consecutive_TO == MAX_TIMEOUT){
	printf("TIMEOUT\n");
	return -1;
      }
    }
	printf("just outside \n");
	
    for(x = 1; x < num_packets; x++){
      message_stats.packet_status[x].packet.message_id = message_stats.packet_status[i].packet.message_id;
    }
    i++;
    message_stats.free_slots--;
    
	printf("second while loop \n");
    while(i < num_available_packets){
      send_packet(&message_stats.packet_status[i].packet, message_stats.mailbox_id, message_stats.receiver_info.pid); 
      message_stats.packet_status[i].is_sent = 1;
      i++;
    }

	printf("third while loop \n");
    while (message_stats.num_packets_received < num_packets){
      
      if(consecutive_TO == MAX_TIMEOUT){
	printf("TIMEOUT\n");
	return -1;
      }
      if(message_stats.free_slots > 0){
      i++;
      printf ("i is equal to: %d\n", i);
      printf("about to send next packet: %d\n", message_stats.packet_status[i].packet.packet_num);
      send_packet(&message_stats.packet_status[i].packet, message_stats.mailbox_id, message_stats.receiver_info.pid); 
      message_stats.packet_status[i].is_sent = 1;
      message_stats.free_slots--;
      }
      
      
    }
      // check is consectutive_TO == MAX_TIMEOUTS to exit
    consecutive_TO = 0;
    return 0;
}

/**
 * TODO Handle TIMEOUT. Resend previously sent packets whose ACKs have not been
 * received yet. Reset the TIMEOUT.
 */
void timeout_handler(int sig) {
	printf("inside timeout handler \n");
	int i;
	for(i = 0; i < message_stats.num_packets; i++){
			if(message_stats.packet_status[i].ACK_received == 0 && message_stats.packet_status[i].is_sent == 1){
				send_packet(&message_stats.packet_status[i].packet, message_stats.mailbox_id, message_stats.receiver_info.pid);
			}
	}	
	consecutive_TO++;
	printf("before alarm \n");
	alarm(TIMEOUT);
	printf("outside alarm \n");
}

/**
 * TODO Send an ACK to the sender's mailbox.
 * The message id is determined by the receiver and has to be included in the ACK packet.
 * Return 0 if success, -1 otherwise.
 */
int send_ACK(int local_mailbox_id, pid_t pid, int packet_num) {
    // TODO construct an ACK packet
	
	printf("sending ACK to mailbox %d \n ", local_mailbox_id);
	packet_t pack;
	pack.mtype = ACK;
	pack.message_id = message_id;
	pack.packet_num = packet_num; 
	pack.pid = myinfo.pid;
    int delay = rand() % MAX_DELAY;
    sleep(delay);

    // TODO send an ACK for the packet it received
	if(msgsnd(local_mailbox_id,(void*) &pack, sizeof(packet_t),0)== -1)
		return -1;
	if(kill(pid,SIGIO) == -1)
		return -1;
    return 0;
}

/**
 * TODO Handle DATA packet. Save the packet's data and send an ACK to the sender.
 * You should handle unexpected cases such as duplicate packet, packet for a different message,
 * packet from a different sender, etc.
 */
void handle_data(packet_t *packet, process_t *sender, int sender_mailbox_id) {
	printf("number of packets: %d \n", packet->num_packets);
	if(message->num_packets_received == 0){
		if((message->data = (char*) malloc((packet->total_size + 1) * sizeof(char))) == NULL )
			perror("message data Malloc failed\n");
		if((message->is_received = (int*) calloc(packet->num_packets,sizeof(int))) == NULL )
			perror("is_received Malloc failed\n");
		if(get_process_info(packet->process_name, &message->sender) == -1)
			perror("get_process_info failed\n");
	}
	//only write data to message if data hasn't been previously received
	if(message->is_received[packet->packet_num] == 0){
		message->num_packets_received++;
		message->is_received[packet->packet_num] = 1;

		int i = 0;
		int j = 0;
	
		//if it's the last packet 
		if(packet->packet_num == packet->num_packets - 1){
			int size = strlen(packet->data);
			for(j = PACKET_SIZE * packet->packet_num; j < (PACKET_SIZE * packet->packet_num) + size - 1; j++){
				message->data[j] = packet->data[i];
				i++;
			}
			//add the null terminator
			message->data[j+1] = '\0';
			
		}else{ //else for all other packets
			for(j = PACKET_SIZE * packet->packet_num; j < (PACKET_SIZE * packet->packet_num) + PACKET_SIZE; j++){
				message->data[j] = packet->data[i];
				
				//printf("packet data: %s \n", &packet->data[i]);
				//printf("message data: %s \n", &message->data[j]);
				i++;
			}
		}
		//send ACK that data has been received
		if(send_ACK(sender_mailbox_id,sender->pid,packet->packet_num) == -1)
			perror("ACK failed to send\n");
	}
	if(message->num_packets_received == packet->num_packets){
		message->is_complete = 1;
	}
}

/**
 * TODO Handle ACK packet. Update the status of the packet to indicate that the packet
 * has been successfully received and reset the TIMEOUT.
 * You should handle unexpected cases such as duplicate ACKs, ACK for completed message, etc.
 */
void handle_ACK(packet_t *packet) {
	printf("handling ACK \n");	
	if(!message_stats.packet_status[packet->packet_num].ACK_received && packet->message_id == message_id)
	{
		message_stats.packet_status[packet->packet_num].ACK_received = 1;
		message_stats.num_packets_received++;
		message_stats.packet_status[packet->packet_num].packet.message_id = packet->message_id;
		
		consecutive_TO = 0;
		message_stats.free_slots++;
		
		alarm(TIMEOUT);
		
	}
}

/**
 * Get the next packet (if any) from a mailbox.
 * Return 0 (false) if there is no packet in the mailbox
 */
int get_packet_from_mailbox(int mailbox_id) {
    struct msqid_ds buf;

    return (msgctl(mailbox_id, IPC_STAT, &buf) == 0) && (buf.msg_qnum > 0);
}

/**
 * TODO Receive a packet.
 * If the packet is DATA, send an ACK packet and SIGIO to the sender.
 * If the packet is ACK, update the status of the packet.
 */
void receive_packet(int sig) {
    // TODO you have to call drop_packet function to drop a packet with some probability
    // if (drop_packet()) {
    //     ...
    // }
    packet_t pckt;

    if(msgrcv(mailbox_id, (void *)&pckt, sizeof(packet_t), 0, 0) == -1)
    		perror("failed to read mailbox\n");
    	
    //printf("received message of size %d, \n", x); 

    if(pckt.mtype == ACK){
	  printf("About to handle ACK\n");
	  handle_ACK(&pckt);
	  printf("handled ACK\n");
    }
	else if(pckt.mtype == DATA)
	{
		int user_mailbox_id;
		if(message->num_packets_received == 0){
			if(get_process_info(pckt.process_name, &message->sender) == -1)
				perror("get_process_info failed\n");}
		if((user_mailbox_id = msgget(message->sender.key, 0777 | IPC_CREAT)) == -1)
			perror("failed to get mailbox\n");
	
		handle_data(&pckt, &message->sender, user_mailbox_id);
	}

	printf("receiver and my mailbox is: %d \n", mailbox_id);
	printf("received packet with contents: %s \n", pckt.data);
}

/**
 * TODO Initialize the message structure and wait for a message from another process.
 * Save the message content to the data and return 0 if success, -1 otherwise
 */
int receive_message(char *data) {
	
	if((message = (message_t*) malloc(sizeof(message_t))) == NULL){
	  perror("malloc failed\n");	
	  return -1;
	}
	message->num_packets_received = 0;
	message->is_complete = 0;
	
	while(message->num_packets_received == 0)
		pause();
	
	while(!message->is_complete)
		pause();
	
	strcpy(data, message->data);
	
	free(message->data);
	free(message->is_received);
	free(message);
	message = NULL;

    return 0;

}
