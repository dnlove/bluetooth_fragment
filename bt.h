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


#ifndef CCN_OVER_BT
#define CCN_OVER_BT

#define BT_MF 0x8000       /* more fragments flag */
#define BT_OFF_MASK 0x7fff /* mask for fragment bits */
#define BT_OFF_BLKSIZE 4   /* offset value of 1 = offset of 8 bytes */
#define BT_OFF_MAX 32767   /* 2^(15)-1 */ 

#define BT_FRAME_LEN 950   /* maximum without fragmentation is 957 */

#define CCNX_PROTO 0xffff  /* ccnx protocol number */


/***********************************
 * Send CCN packet over bluetooth device 
 *  handles fragmentation
 */
int send_ccn_over_bt(const char * bt_dev, const void *data, size_t length, int s);

/***********************************
 * Receives CCN packets over bluetooth device
 *   returns -1 if there is an error
 *   returns 0 if a non-terminating fragment is received
 *   returns length of data if terminating fragment is received
 *   data in packets are written to ccn_packet
 */
int recv_ccn_over_bt(const char * bt_dev, char * ccnx_pkt, int socket);

/***********************************
 * extended bluetooth header
 * containing fragmentation & checksum headers
 */
struct ext_bt_header{
	char dest_mac[18];	
	char src_mac[18];
	unsigned char  type;       // 4 bits
	uint16_t protocol;         // 2 bytes
	
	// bluetooth fragmentation & checksum
	uint32_t length;           // 4 bytes
	uint16_t id;               // 2 bytes
	uint16_t offset;           // 2 bytes

	//not implemented	
	uint32_t checksum;         // 4 bytes 
} __attribute__((packed));

#endif
