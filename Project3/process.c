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

    // TODO setup a message queue and save the id to the mailbox_id

    // TODO set the signal handler for receiving packets

    return -1;
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
 * TODO Send a packet to a mailbox identified by the mailbox_id, and send a SIGIO to the pid.
 * Return 0 if success, -1 otherwise.
 */
int send_packet(packet_t *packet, int mailbox_id, int pid) {
    return -1;
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

    // TODO send packets to the receiver
    // the number of packets sent at a time depends on the WINDOW_SIZE.
    // you need to change the message_id of each packet (initialized to -1)
    // with the message_id included in the ACK packet sent by the receiver


    return -1;
}

/**
 * TODO Handle TIMEOUT. Resend previously sent packets whose ACKs have not been
 * received yet. Reset the TIMEOUT.
 */
void timeout_handler(int sig) {

}

/**
 * TODO Send an ACK to the sender's mailbox.
 * The message id is determined by the receiver and has to be included in the ACK packet.
 * Return 0 if success, -1 otherwise.
 */
int send_ACK(int mailbox_id, pid_t pid, int packet_num) {
    // TODO construct an ACK packet

    int delay = rand() % MAX_DELAY;
    sleep(delay);

    // TODO send an ACK for the packet it received

    return -1;
}

/**
 * TODO Handle DATA packet. Save the packet's data and send an ACK to the sender.
 * You should handle unexpected cases such as duplicate packet, packet for a different message,
 * packet from a different sender, etc.
 */
void handle_data(packet_t *packet, process_t *sender, int sender_mailbox_id) {

}

/**
 * TODO Handle ACK packet. Update the status of the packet to indicate that the packet
 * has been successfully received and reset the TIMEOUT.
 * You should handle unexpected cases such as duplicate ACKs, ACK for completed message, etc.
 */
void handle_ACK(packet_t *packet) {

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
}

/**
 * TODO Initialize the message structure and wait for a message from another process.
 * Save the message content to the data and return 0 if success, -1 otherwise
 */
int receive_message(char *data) {
    return -1;
}
