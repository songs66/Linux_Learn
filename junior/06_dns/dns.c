#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#define DNS_SERVER_PORT		53
#define DNS_SERVER_IP		"114.114.114.114"

//----------------------------------------------------------------------------------------------------------
//报文response解析
#define DNS_HOST			0x01
#define DNS_CNAME			0x05

struct dns_item {
	char *domain;
	char *ip;
};
static int is_pointer(int in);
static void dns_parse_name(unsigned char *chunk, unsigned char *ptr, char *out, int *len);
static int dns_parse_response(char *buffer, struct dns_item **domains);
//----------------------------------------------------------------------------------------------------------

//DNS协议报文请求头
struct dns_header {

    unsigned short id;
    unsigned short flags;

    unsigned short questions; // 1
    unsigned short answer;

    unsigned short authority;
    unsigned short additional;
};
//DNS协议报文正文 查询问题区域
struct dns_question {
    int length;//name的长度
    unsigned char *name;
    unsigned short qtype;
    unsigned short qclass;
};

//填充DNS协议报文请求头
int dns_create_header(struct dns_header *header) {

    if (header == NULL) return -1;
    memset(header, 0, sizeof(struct dns_header));

    //random
    srandom(time(NULL));
    header->id = random();

    header->flags = htons(0x0100);
    header->questions = htons(1);

    return 0;
}
//填充DNS协议报文正文 查询问题区域
//  hostname    : www.0voice.com
//question->name: 3www60voice3com0
int dns_create_question(struct dns_question *question, const char *hostname) {

    if (question == NULL || hostname == NULL) return -1;
    memset(question, 0, sizeof(struct dns_question));

    //填充question->length
    question->length = (int)strlen(hostname) + 2;
    //申请question->name的内存
    question->name = malloc(question->length);
    if (question->name == NULL) {
        return -2;
    }
    //填充qtype，qclass
    question->qtype = htons(1); //
    question->qclass = htons(1);

    // 填充name
    //  hostname    : www.0voice.com
    //question->name: 3www60voice3com0
    const char delim[2] = ".";//定义断点
    char *qname = (char*)question->name;//定义一个qname当替身干活用

    char *hostname_dup = strdup(hostname); // strdup执行时带有malloc步骤（将hostname复制一份）
    char *token = strtok(hostname_dup, delim); // 被截取字符串www.0voice.com被.截取后token中字符为www

    while (token != NULL) {
        size_t len = strlen(token);

        *qname = (char)len;//将数字放好
        qname ++;//指向数字下一位的内存

        strncpy(qname, token, len+1);//可+1（携带\0）可不+1（最末尾手动加\0）
        qname += len;//指向字母后方的\0

        token = strtok(NULL, delim); //被截取字符串0voice.com被.截取后token中字符为0voice
    }

    free(hostname_dup);
    return 0;
}

//构建完整请求
// struct dns_header *header
// struct dns_question *question
// char *request（由上面两个组合而成的完整请求）
int dns_build_request(struct dns_header *header, struct dns_question *question, char *request, int rlen) {
    if (header == NULL || question == NULL || request == NULL) {
        return -1;
    }
    memset(request, 0, rlen);

    int offset =0;//请求总长度
    // header --> request
    memcpy(request, header, sizeof(struct dns_header));
    offset += sizeof(struct dns_header);

    // question --> request（question->length多余，所以不能直接将结构体拷贝）
    memcpy(request+offset, question->name, question->length);
    offset += question->length;

    memcpy(request+offset, &question->qtype, sizeof(question->qtype));
    offset += sizeof(question->qtype);

    memcpy(request+offset, &question->qclass, sizeof(question->qclass));
    offset += sizeof(question->qclass);

    return offset;
}

//发送报文并接收数据
int dns_client_commit(const char *domain) {
    //造一个“电话机”
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return  -1;
    }
    //构建并填充目标地址结构体
    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(DNS_SERVER_PORT);
    servaddr.sin_addr.s_addr = inet_addr(DNS_SERVER_IP);

    //UDP编程中是不必要的，但是connect()一下是为了为下面的sendto()开路，防止网络丢包
    int ret = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	/**connect()解释：向远程服务器发起三次握手，把本地套接字从CLOSED状态推进到ESTABLISHED状态，建立一条已连接的TCP流
	 返回0为成功，返回-1为失败
	 参数1：sockfd	已由 socket() 创建的 TCP 套接字描述符（必须是 SOCK_STREAM）
	 参数2：addr		指向通用套接字地址结构的指针，实际传入 struct sockaddr_in（IPv4）或 sockaddr_in6（IPv6）
	 参数3：addrlen	上面结构体的 实际长度，如 sizeof(struct sockaddr_in)
	 **/
	if (ret!=0) {
		printf("连接不通");
		exit(-1);
	}
    //构建整体请求
    //头
    struct dns_header header = {0};
    dns_create_header(&header);
    //查询区域
    struct dns_question question = {0};
    dns_create_question(&question, domain);
    //头+查询区域=总报文
    char request[1024] = {0};
    int length = dns_build_request(&header, &question, request, 1024);//传入1024用于memset

    //发送请求
    const int slen = (int)sendto(sockfd, request, length, 0, (struct sockaddr *)&servaddr, sizeof(struct sockaddr));
    /**六个参数的含义
     sockfd：套接字描述符，标识要发送数据的套接字
     request：指向要发送数据的缓冲区的指针
     length：要发送的数据的字节数
     flags：通常设置为 0，表示没有特殊标志
     servaddr：指向目标地址结构体的指针（对于 UDP，这是接收方的地址）
     addrlen：地址结构体的长度（通常是 sizeof(struct sockaddr_in) 或 sizeof(struct sockaddr_in6)
     **/
    printf("成功发送%d字节数据\n",slen);

    //准备内存用来接收下一步从服务器返回的数据
    char response[1024] = {0};
    struct sockaddr_in addr;
    size_t addr_len = sizeof(struct sockaddr_in);

    //接收从服务器返回的数据
    const int n = (int) recvfrom(sockfd, response, sizeof(response), 0, (struct sockaddr *) &addr, (socklen_t *) &addr_len);
    /**
     sockfd：套接字描述符，标识要接收数据的套接字
     response：指向接收数据缓冲区的指针
     len：缓冲区的最大长度（以字节为单位）
     flags：通常设置为 0，表示没有特殊标志
     addr：指向地址结构体的指针，用于存储发送方的地址信息（对于 UDP，这是发送方的地址）
     addrlen：指向地址结构体长度的指针。调用前，应初始化为地址结构体的大小；调用后，会更新为实际地址的长度
     **/
    printf("成功接收到%d字节数据\n",n);

	//报文解析
    struct dns_item *dns_domain = NULL;
    dns_parse_response(response, &dns_domain);

    free(dns_domain);

    return n;

}
//-----------------------------------------------------------------------------------
//报文response解析
static int is_pointer(const int in) {
	return ((in & 0xC0) == 0xC0);
}

static void dns_parse_name(unsigned char *chunk, unsigned char *ptr, char *out, int *len) {
	int flag = 0, n = 0, alen = 0;
	char *pos = out + (*len);

	while (1) {
		flag = (int)ptr[0];
		if (flag == 0) break;

		if (is_pointer(flag)) {
			n = (int)ptr[1];
			ptr = chunk + n;
			dns_parse_name(chunk, ptr, out, len);
			break;
		}
		ptr ++;
		memcpy(pos, ptr, flag);
		pos += flag;
		ptr += flag;

		*len += flag;
		if ((int)ptr[0] != 0) {
			memcpy(pos, ".", 1);
			pos += 1;
			(*len) += 1;
		}
	}

}

static int dns_parse_response(char *buffer, struct dns_item **domains) {

	int i = 0;
	unsigned char *ptr = (unsigned char*)buffer;

	ptr += 4;
	int querys = ntohs(*(unsigned short*)ptr);

	ptr += 2;
	int answers = ntohs(*(unsigned short*)ptr);

	ptr += 6;
	for (i = 0;i < querys;i ++) {
		while (1) {
			int flag = (int)ptr[0];
			ptr += (flag + 1);

			if (flag == 0) break;
		}
		ptr += 4;
	}

	char cname[128], aname[128], ip[20], netip[4];
	int len, type, ttl, datalen;

	int cnt = 0;
	struct dns_item *list = calloc(answers, sizeof(struct dns_item));
	if (list == NULL) {
		return -1;
	}

	for (i = 0;i < answers;i ++) {

		bzero(aname, sizeof(aname));
		len = 0;

		dns_parse_name((unsigned char*)buffer, ptr, aname, &len);
		ptr += 2;

		type = htons(*(unsigned short*)ptr);
		ptr += 4;

		ttl = htons(*(unsigned short*)ptr);
		ptr += 4;

		datalen = ntohs(*(unsigned short*)ptr);
		ptr += 2;

		if (type == DNS_CNAME) {

			bzero(cname, sizeof(cname));
			len = 0;
			dns_parse_name((unsigned char*)buffer, ptr, cname, &len);
			ptr += datalen;

		} else if (type == DNS_HOST) {

			bzero(ip, sizeof(ip));

			if (datalen == 4) {
				memcpy(netip, ptr, datalen);
				inet_ntop(AF_INET , netip , ip , sizeof(struct sockaddr));

				printf("%s 存在地址:%s\n" , aname, ip);
				printf("\tTime to live: %d minutes , %d seconds\n", ttl / 60, ttl % 60);

				list[cnt].domain = (char *)calloc(strlen(aname) + 1, 1);
				memcpy(list[cnt].domain, aname, strlen(aname));

				list[cnt].ip = (char *)calloc(strlen(ip) + 1, 1);
				memcpy(list[cnt].ip, ip, strlen(ip));

				cnt ++;
			}

			ptr += datalen;
		}
	}

	*domains = list;
	ptr += 2;

	return cnt;
}
//报文response解析完成
//-----------------------------------------------------------------------------------
char *domain[] = {
	"www.baidu.com","www.ntytcp.com","bojing.wang","tieba.baidu.com","news.baidu.com",
	"zhidao.baidu.com","music.baidu.com","image.baidu.com","v.baidu.com","map.baidu.com",
	"baijiahao.baidu.com","xueshu.baidu.com","cloud.baidu.com","www.163.com","open.163.com",
	"auto.163.com","gov.163.com","money.163.com","sports.163.com","tech.163.com",
	"edu.163.com","www.taobao.com","q.taobao.com","sf.taobao.com","yun.taobao.com",
	"baoxian.taobao.com","www.tmall.com","suning.tmall.com","www.tencent.com","www.qq.com",
	"www.aliyun.com","www.ctrip.com","hotels.ctrip.com","hotels.ctrip.com","vacations.ctrip.com",
	"flights.ctrip.com","trains.ctrip.com","bus.ctrip.com","car.ctrip.com","piao.ctrip.com",
	"tuan.ctrip.com","you.ctrip.com","g.ctrip.com","lipin.ctrip.com","www.songzihao.top"
};
#if 1
int main(int argc, char *argv[]) {
	int i = 0;
	while (domain[i]) {
		printf("正在查询第%d号网址%s的IP地址...\n",i+1,domain[i]);
		dns_client_commit(domain[i]);
		printf("------------------------\n");
		printf("\n");
		i++;
	}
	return 0;
}
#endif

#if 0
int main(int argc, char *argv[]) {
	if (argc!=2) {
		printf("param error\n");
		return -1;
	}
	dns_client_commit(argv[1]);
	return 0;
}
#endif

