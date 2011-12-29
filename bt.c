/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Duy Nguyen <duy@soe.ucsc.edu>
 *
 * This is the modular API for CCN over Bluetooth
 * Based on CCN over Ethernet work done by UCLA 
 * Since this is only an API, you'll need to modify the ccnd daemon accordingly
 * This API can also be used as a stand-alone test for bluetooth devices
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <netinet/in.h>
#include <errno.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>



#include "bt.h"

#define DEBUG 1
#define STANDALONETEST 1
int main() {

	int i;

	char data[4000];

	data[0] = 'h';
	data[1] = 'o';
	data[2] = 'l';
	data[3] = 'a';
	data[4] = '!';

	for(i= 5; i < sizeof(data); i++){
		if(i%20==0)
			data[i] = 'd';
		else
			data[i] = '-';
	}

	data[1488] = 's';
	data[1489] = 'e';
	data[1490] = 'c';
	data[1491] = 'o';
	data[1492] = 'n';
	data[1493] = 'd';

	data[2976] = 't';
	data[2977] = 'h';
	data[2978] = 'i';
	data[2979] = 'r';
	data[2980] = 'd';

	int s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

	send_ccn_over_bt("hci0", (void*)data, sizeof(data), s);


	// allocate space for ccnx packet
	char * ccnx_pkt = (char*)malloc(30000);


	// waiting for all fragments to appear
	int result = 0;
	while(result == 0){
		result = recv_ccn_over_bt("hci0", ccnx_pkt, s);
	}

	// print out full packet contents
	printf("\n");
	printf("----------CCNX PACKET-----------\n");

	printf("size=%d\n",result);
	printf("ccnx_pkt:\n%s\n",ccnx_pkt);

	free(ccnx_pkt);

	return 0;
}


void get_local_btaddr(char * src, int size) {

	char mysrc[18];
	int hcisock =socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
        int dev_id = hci_get_route(NULL);
     	struct hci_dev_info di = { dev_id: dev_id };

        int rv = ioctl (hcisock, HCIGETDEVINFO, (void *)&di);  
	if (rv < 0)
		printf("Error getting device info\n");

        ba2str(&di.bdaddr, mysrc);
	strncpy(src, mysrc, size);

        printf("%s\t%s\n", di.name, mysrc);
}



int send_ccn_over_bt_rec( const char * eth_dev, const void *ccn_data, size_t length, size_t offset, int id, int ssock){

	int send_result = 0;
	int is_more = 0;

	// data pointer is for recursive use so we can traverse the ccn_data
	char * data_ptr = (char *)ccn_data;

	// buffer to be sent over bt 
	char * buffer = (char*)malloc(BT_FRAME_LEN);
	memset(buffer, 0, BT_FRAME_LEN);

	int s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

	if (s== -1) {
		printf("Cannot Run CCN-over-bluetooth:socket returns -1 \n");
		exit(0);
	}

//        char dest[18] = "00:19:0E:0B:5F:6F"; // Host 
        char dest[18] = "00:02:72:1A:C5:0F"; //  Guest 


	// Construct the extended bluetooth header for this packet
	struct ext_bt_header ebth;
	memcpy(ebth.dest_mac, dest, sizeof(dest));

	get_local_btaddr(ebth.src_mac, 18);
	
	// set the connection parameters (who to connect to)
	struct sockaddr_rc addr = { 0 };
    	addr.rc_family = AF_BLUETOOTH;
    	addr.rc_channel = (uint8_t) 1;
    	str2ba(dest, &addr.rc_bdaddr );

        int status = connect(s, (struct sockaddr *)&addr, sizeof(addr));

	if(status == 1)
		printf("status is %d: trouble connecting \n", status);
	else if(status == 0)
		printf("status is %d: connecting\n", status);
	else 
		printf("status is %d : something is wrong\n", status);

	if (0 != status ) {
                perror("connect");
        }


	do {  

	// set protocol to CCNX protocol number
	ebth.protocol = CCNX_PROTO;

	// case: data is larger than bluetooth frame length
	if ( length >= BT_FRAME_LEN - sizeof(struct ext_bt_header) ){

		printf("data >= bt frame length  ext_bt_header= %d\n", sizeof(struct ext_bt_header));

		ebth.length = BT_FRAME_LEN -sizeof(struct ext_bt_header);
		ebth.offset = offset | BT_MF; // set BT_MF flag to 1
		is_more = 1;
	}
	// case: data is equal to or smaller than bluetooth frame length
	else{
		printf("data < bt frame length\n");

		is_more = 0;
		ebth.length = length;
		ebth.offset = 0;  // set BT_MF flag to 0
		ebth.offset = offset;
	}

	// set id
	ebth.id = id;

	// checksum initially set to 0
	ebth.checksum = 0;

	// copy extended bluetooth header to buffer
	memcpy(buffer, &ebth, sizeof(struct ext_bt_header));

	// copy ccnx packet data to buffer
	memcpy(buffer + sizeof(struct ext_bt_header), data_ptr, ebth.length);

	if(DEBUG){
		printf("------------BT CCN SEND------------\n");
		printf("ebth.dest_mac=%s\n", ebth.dest_mac);
		printf("ebth.src_mac=%s\n", ebth.src_mac);
		printf("ebth.protocol=%x\n",ebth.protocol);
		printf("ebth.length=%d\n",ebth.length);
		printf("ebth.id=%d\n",ebth.id);
		printf("ebth.offset=%d\n",ebth.offset&BT_OFF_MASK);
		printf("ebth.checksum=%x\n",ebth.checksum);
		printf("morepackets? %c\n",(ebth.offset&(~BT_OFF_MASK))?'Y':'N');
	}


	// send the packet over raw bluetooth
	send_result = send(s, buffer, sizeof(struct ext_bt_header)+ebth.length, 0);

	if(DEBUG){
		if (send_result == -1)
			printf("bluetooth send error: %s\n", strerror(errno) );
		else
			printf("bluetooth frame sent successfully\n");
		printf("----------------------------------\n\n");
	}

	// updating
	if (length - ebth.length > 0) {

	length = length - ebth.length;
	offset = (offset * BT_OFF_BLKSIZE + ebth.length)/BT_OFF_BLKSIZE;
	memset(buffer, 0, BT_FRAME_LEN);
	data_ptr = data_ptr + ebth.length;

	}

	} while (is_more == 1);

	free(buffer);
	close(s);

	return 0;

}


/*
** Function to send CCNx packets over bluetooth
*/
int send_ccn_over_bt( const char * eth_dev, const void *ccn_data, size_t length, int sock){

	int result = 0;

	// initialize random number generator
	srand( time(NULL) );

	// initialize random value for fragment id
	uint16_t id = rand();

	// send the packet
	result = send_ccn_over_bt_rec( eth_dev, ccn_data, length, 0, id, sock);
	
	return result;
}



/*
** Function receives CCNx fragments over bluetooth
** returns 0 if fragments are still on the way
** returns length of CCNx packet when last fragment has arrived
** each call to recv_ccn_over_eth() writes the received CCNX packet to ccnx_pkt
*/
int recv_ccn_over_bt(const char * eth_dev, char * ccnx_pkt, int sock) {

	printf("recv_ccn_over_bt bt_sock %i\n", sock);

	// temporary buffer to write ccnx fragment to 
	char * buffer = (char *)malloc(BT_FRAME_LEN);
	memset(buffer, 0, BT_FRAME_LEN);

	int res2;
	int total = 0;
	struct pollfd sd[1];

	struct ext_bt_header ebth;
	struct sockaddr_rc rem_addr = { 0 };
	socklen_t opt = sizeof(rem_addr);

	sd->fd = sock;
	sd->events = POLLIN;

	//Set STANDALONETEST to 0 if integrate with ccn
	//since ccn handles this 

	if (STANDALONETEST) {
	
	struct sockaddr_rc loc_addr = { 0 };
 	
   	// bind socket to port 1 of the first available 
   	// local bluetooth adapter
   	loc_addr.rc_family = AF_BLUETOOTH;
	loc_addr.rc_bdaddr = *BDADDR_ANY;
	loc_addr.rc_channel = (uint8_t) 1;
	bind(sock, (struct sockaddr *)&loc_addr, sizeof(loc_addr));

	// put socket into listening mode
	listen(sock, 30);
	
	}

	// accept one connection, 
	int client = accept(sock, (struct sockaddr *)&rem_addr, &opt);

  	char self_mac[18], dest[18];
	ba2str( &rem_addr.rc_bdaddr, dest);
	fprintf(stderr, "accepted connection from %s\n", dest);

	get_local_btaddr(self_mac, 18);
	printf("my local address is %s\n", self_mac);
    

 	int no_more = 0;

	do {

	res2 = recv(client, buffer, BT_FRAME_LEN, 0);
	if (res2 < 0){
		printf("bt recv error: %s\n", strerror(errno));
		return res2;
	}
	else{
		total += res2;
		memcpy(&ebth, buffer, sizeof(struct ext_bt_header));

		// check if packet is a CCNX packet
		if(ebth.protocol == CCNX_PROTO){

			if( strcmp(self_mac, ebth.src_mac)==0 ){
				// do nothing, return 0;
				printf("Why I am sending to myself\n");
			}

			// check if packet is addressed to this device
			else if( strcmp(ebth.dest_mac, self_mac) == 0) {

				printf("receiving %i\n", res2);

				// write the incoming data to disk
				memcpy( ccnx_pkt + (ebth.offset & BT_OFF_MASK) * BT_OFF_BLKSIZE, buffer + sizeof(struct ext_bt_header), ebth.length);

				if(DEBUG){
					printf("------------BT CCN RECV------------\n");
					printf("ebth.dest_mac=%s\n", ebth.dest_mac);
					printf("ebth.src_mac=%s\n", ebth.src_mac);

					printf("ebth.protocol=%x\n",ebth.protocol);
					printf("ebth.length=%d\n",ebth.length);
					printf("ebth.id=%d\n",ebth.id);
					printf("ebth.offset=%d\n",ebth.offset&BT_OFF_MASK);
					printf("ebth.checksum=%x\n",ebth.checksum);
					printf("morepackets? %c\n",(ebth.offset&(~BT_OFF_MASK))?'Y':'N');
				}


				if(ebth.offset&(~BT_OFF_MASK)) {
					if (DEBUG)
						printf("------------more fragments------------\n");
					no_more = 1;
				}
				else {
					if (DEBUG)
						printf("------------no more fragments------------\n");
					no_more  = 0; //(ebth.offset&BT_OFF_MASK)*BT_OFF_BLKSIZE+ebth.length;
				}
			}
		}
	}


	} while (no_more != 0);

	free(buffer);

	printf("exiting recv_ccn total receiving is %i \n", total);

	return total;
}
