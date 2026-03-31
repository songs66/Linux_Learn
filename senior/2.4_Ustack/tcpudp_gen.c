

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

#include <stdio.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <unistd.h>

#define ENABLE_SEND		1
#define ENABLE_ARP		1



#define NUM_MBUFS (4096-1)

#define BURST_SIZE	32


#if ENABLE_SEND

static uint32_t gSrcIp; //
static uint32_t gDstIp;

static uint8_t gSrcMac[RTE_ETHER_ADDR_LEN];
static uint8_t gDstMac[RTE_ETHER_ADDR_LEN];

static uint16_t gSrcPort;
static uint16_t gDstPort;

#endif


int gDpdkPortId = 0;



static const struct rte_eth_conf port_conf_default = {
	.rxmode = {.max_rx_pkt_len = RTE_ETHER_MAX_LEN }
};

static void ng_init_port(struct rte_mempool *mbuf_pool) {

	uint16_t nb_sys_ports= rte_eth_dev_count_avail(); //
	if (nb_sys_ports == 0) {
		rte_exit(EXIT_FAILURE, "No Supported eth found\n");
	}

	struct rte_eth_dev_info dev_info;
	rte_eth_dev_info_get(gDpdkPortId, &dev_info); //
	
	const int num_rx_queues = 1;
	const int num_tx_queues = 1;
	struct rte_eth_conf port_conf = port_conf_default;
	rte_eth_dev_configure(gDpdkPortId, num_rx_queues, num_tx_queues, &port_conf);


	if (rte_eth_rx_queue_setup(gDpdkPortId, 0 , 1024, 
		rte_eth_dev_socket_id(gDpdkPortId),NULL, mbuf_pool) < 0) {

		rte_exit(EXIT_FAILURE, "Could not setup RX queue\n");

	}
	
#if ENABLE_SEND
	struct rte_eth_txconf txq_conf = dev_info.default_txconf;
	txq_conf.offloads = port_conf.rxmode.offloads;
	if (rte_eth_tx_queue_setup(gDpdkPortId, 0 , 1024, 
		rte_eth_dev_socket_id(gDpdkPortId), &txq_conf) < 0) {
		
		rte_exit(EXIT_FAILURE, "Could not setup TX queue\n");
		
	}
#endif

	if (rte_eth_dev_start(gDpdkPortId) < 0 ) {
		rte_exit(EXIT_FAILURE, "Could not start\n");
	}

	

}



static int ng_encode_tcp_pkt(uint8_t *msg, unsigned char *data, uint16_t total_len) {

	// encode 

	// 1 ethhdr
	struct rte_ether_hdr *eth = (struct rte_ether_hdr *)msg;
	rte_memcpy(eth->s_addr.addr_bytes, gSrcMac, RTE_ETHER_ADDR_LEN);
	rte_memcpy(eth->d_addr.addr_bytes, gDstMac, RTE_ETHER_ADDR_LEN);
	eth->ether_type = htons(RTE_ETHER_TYPE_IPV4);
	

	// 2 iphdr 
	struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(msg + sizeof(struct rte_ether_hdr));
	ip->version_ihl = 0x45;
	ip->type_of_service = 0;
	ip->total_length = htons(total_len - sizeof(struct rte_ether_hdr));
	ip->packet_id = 0;
	ip->fragment_offset = 0;
	ip->time_to_live = 64; // ttl = 64
	ip->next_proto_id = IPPROTO_TCP;
	ip->src_addr = gSrcIp;
	ip->dst_addr = gDstIp;
	
	ip->hdr_checksum = 0;
	ip->hdr_checksum = rte_ipv4_cksum(ip);

	// 3 tcphdr 
	struct rte_tcp_hdr *tcp = (struct rte_tcp_hdr *)(msg + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
	tcp->src_port = htons(gSrcPort);
	tcp->dst_port = htons(gDstPort);
	tcp->tcp_flags = 1<< 1;
	tcp->data_off = 0x50;
	tcp->rx_win = htons(65535);
	tcp->sent_seq = htonl(12345);
	tcp->recv_ack = 0x0;
	tcp->cksum = 0;
	tcp->cksum = rte_ipv4_udptcp_cksum(ip, tcp);
	

	struct in_addr addr;
	addr.s_addr = gSrcIp;
	printf(" --> tcp src: %s:%d, ", inet_ntoa(addr), gSrcPort);

	addr.s_addr = gDstIp;
	printf("dst: %s:%d, %s\n", inet_ntoa(addr), gDstPort, data);

	return 0;
}


static struct rte_mbuf * ng_tcp_send(struct rte_mempool *mbuf_pool, uint8_t *data, uint16_t length) {

	// mempool --> mbuf

	const unsigned total_len = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_tcp_hdr) + length;

	struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
	if (!mbuf) {
		rte_exit(EXIT_FAILURE, "rte_pktmbuf_alloc\n");
	}
	mbuf->pkt_len = total_len;
	mbuf->data_len = total_len;

	uint8_t *pktdata = rte_pktmbuf_mtod(mbuf, uint8_t*);

	ng_encode_tcp_pkt(pktdata, data, total_len);

	return mbuf;

}


static int ng_encode_udp_pkt(uint8_t *msg, uint8_t *data, uint16_t total_len) {

	// encode 

	// 1 ethhdr
	struct rte_ether_hdr *eth = (struct rte_ether_hdr *)msg;
	rte_memcpy(eth->s_addr.addr_bytes, gSrcMac, RTE_ETHER_ADDR_LEN);
	rte_memcpy(eth->d_addr.addr_bytes, gDstMac, RTE_ETHER_ADDR_LEN);
	eth->ether_type = htons(RTE_ETHER_TYPE_IPV4);
	

	// 2 iphdr 
	struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(msg + sizeof(struct rte_ether_hdr));
	ip->version_ihl = 0x45;
	ip->type_of_service = 0;
	ip->total_length = htons(total_len - sizeof(struct rte_ether_hdr));
	ip->packet_id = 0;
	ip->fragment_offset = 0;
	ip->time_to_live = 64; // ttl = 64
	ip->next_proto_id = IPPROTO_UDP;
	ip->src_addr = gSrcIp;
	ip->dst_addr = gDstIp;
	
	ip->hdr_checksum = 0;
	ip->hdr_checksum = rte_ipv4_cksum(ip);

	// 3 udphdr 

	struct rte_udp_hdr *udp = (struct rte_udp_hdr *)(msg + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
	udp->src_port = htons(gSrcPort);
	udp->dst_port = htons(gDstPort);
	uint16_t udplen = total_len - sizeof(struct rte_ether_hdr) - sizeof(struct rte_ipv4_hdr);
	udp->dgram_len = htons(udplen);

	rte_memcpy((uint8_t*)(udp+1), data, udplen);

	udp->dgram_cksum = 0;
	udp->dgram_cksum = rte_ipv4_udptcp_cksum(ip, udp);

	struct in_addr addr;
	addr.s_addr = gSrcIp;
	printf("--> udp  src: %s:%d, ", inet_ntoa(addr), gSrcPort);

	addr.s_addr = gDstIp;
	printf("dst: %s:%d --> %s\n", inet_ntoa(addr), gDstPort, data);

	return 0;
}


static struct rte_mbuf * ng_udp_send(struct rte_mempool *mbuf_pool, uint8_t *data, uint16_t length) {

	// mempool --> mbuf

	const unsigned total_len = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr) + length;

	struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
	if (!mbuf) {
		rte_exit(EXIT_FAILURE, "rte_pktmbuf_alloc\n");
	}
	mbuf->pkt_len = total_len;
	mbuf->data_len = total_len;

	uint8_t *pktdata = rte_pktmbuf_mtod(mbuf, uint8_t*);

	ng_encode_udp_pkt(pktdata, data, total_len);

	return mbuf;

}




// ./pktgen -u -sip 192.168.243.131 -sport 9999  -dmac 01:23:45:67:89:AB -dip 192.168.1.27 -dport 2048
static struct option args[] = {
	{"sip", required_argument, 0, 's'},
	{"sport", required_argument, 0, 'p'},
	{"dmac", required_argument, 0, 'm'},
	{"dip", required_argument, 0, 'i'},
	{"dport", required_argument, 0, 'd'},
	{"count", required_argument, 0, 'c'},
	{"udp", no_argument, 0, 'u'},
	{0, 0, 0, 0}
};


int main(int argc, char *argv[]) {

	int ret = rte_eal_init(argc, argv);
	if (ret < 0) {
		rte_exit(EXIT_FAILURE, "Error with EAL init\n");
	}

	
	argc -= ret;
	argv += ret;

	char opt;
	int flag = 0; // default tcp, 1 udp
	int count = 1;
	
	while ((opt = getopt_long(argc, argv, "s:p:m:i:d:c:u:?", args, NULL)) != -1) {

		switch (opt) {

			case 's':
				printf("-s : %s\n", optarg);
				inet_pton(AF_INET, optarg, &gSrcIp);
				break;
			case 'p':
				printf("-p : %s\n", optarg);
				gSrcPort = atoi(optarg);
				break;
			case 'm':
				printf("-m : %s\n", optarg);
				sscanf(optarg, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &gDstMac[0], &gDstMac[1], &gDstMac[2], 
					&gDstMac[3], &gDstMac[4], &gDstMac[5]);
				break;
			case 'i':
				printf("-i : %s\n", optarg);
				inet_pton(AF_INET, optarg, &gDstIp);
				break;
			case 'd':
				printf("-d : %s\n", optarg);
				gDstPort = atoi(optarg);
				break;
			case 'u':
				printf("-u : %s\n", optarg);
				flag = 1;
				break;
			case 'c':
				printf("-c : %s\n", optarg);
				count = atoi(optarg);
				count = count > 1 ? count : 1;
				break;
			case '?':
                printf("Invalid option\n");
                return -1;
			default:
				break;

		}

	}

	struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create("mbuf pool", NUM_MBUFS,
		0, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (mbuf_pool == NULL) {
		rte_exit(EXIT_FAILURE, "Could not create mbuf pool\n");
	}

	ng_init_port(mbuf_pool);

	rte_eth_macaddr_get(gDpdkPortId, (struct rte_ether_addr *)gSrcMac);


	int i = 0;
	for (i = 0;i < count;i ++) {

		
		char str[] = "Hello, King";
		uint16_t len =  (uint16_t)strlen(str);
		

		struct rte_mbuf *txbuf = NULL;

		if (flag) {
			txbuf = ng_udp_send(mbuf_pool, (uint8_t *)str, len);
		} else {
			gSrcPort += i % 10000;
			txbuf = ng_tcp_send(mbuf_pool, (uint8_t *)str, len);
			usleep(10);
		}
		
		rte_eth_tx_burst(gDpdkPortId, 0, &txbuf, 1);
		rte_pktmbuf_free(txbuf);
	}
	
	

}




