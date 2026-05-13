#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/time.h>


#define MAX_MSG_LENGTH		1024
#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)


// 测试客户端统一通过这两个函数和服务端收发消息
int send_msg(int connfd, char *msg, int length) {

	int res = send(connfd, msg, length, 0);
	if (res < 0) {
		perror("send");
		exit(1);
	}
	return res;
}

int recv_msg(int connfd, char *msg, int length) {

	int res = recv(connfd, msg, length, 0);
	if (res < 0) {
		perror("recv");
		exit(1);
	}
	return res;

}

// 一条最小测试用例：发送命令、读取响应、直接比对预期结果
void testcase(int connfd, char *msg, char *pattern, char *casename) {

	if (!msg || !pattern || !casename) return ;

	send_msg(connfd, msg, strlen(msg));

	char result[MAX_MSG_LENGTH] = {0};
	recv_msg(connfd, result, MAX_MSG_LENGTH);

	if (strcmp(result, pattern) == 0) {
		printf("==> PASS ->  %s\n", casename);
	} else {
		printf("==> FAILED -> %s, '%s' != '%s' \n", casename, result, pattern);
		exit(1);
	}

}


// 建立到 kvstore 服务端的测试连接
int connect_tcpserver(const char *ip, unsigned short port) {

	int connfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(struct sockaddr_in));

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);
	server_addr.sin_port = htons(port);

	if (0 !=  connect(connfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in))) {
		perror("connect");
		return -1;
	}
	
	return connfd;
	
}

// 基础功能回归测试
void testcase_basics(int connfd) {

	testcase(connfd, "SET Teacher King", "OK\r\n", "SET-Teacher");
	testcase(connfd, "GET Teacher", "King\r\n", "GET-Teacher");
	testcase(connfd, "MOD Teacher Darren", "OK\r\n", "MOD-Teacher");
	testcase(connfd, "GET Teacher", "Darren\r\n", "GET-Teacher");
	testcase(connfd, "EXIST Teacher", "EXIST\r\n", "GET-Teacher");
	testcase(connfd, "DEL Teacher", "OK\r\n", "DEL-Teacher");
	testcase(connfd, "GET Teacher", "NO EXIST\r\n", "GET-Teacher");
	testcase(connfd, "MOD Teacher KING", "NO EXIST\r\n", "MOD-Teacher");
	testcase(connfd, "EXIST Teacher", "NO EXIST\r\n", "GET-Teacher");

}

// 高频循环测试：用于观察基础功能稳定性和粗略性能
void testcase_1w(int connfd) {

	int count = 10000;
	int i = 0;

	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);

	for (i = 0;i < count;i ++) {

		testcase(connfd, "SET Teacher King", "OK\r\n", "SET-Teacher");
		testcase(connfd, "GET Teacher", "King\r\n", "GET-Teacher");
		testcase(connfd, "MOD Teacher Darren", "OK\r\n", "MOD-Teacher");
		testcase(connfd, "GET Teacher", "Darren\r\n", "GET-Teacher");
		testcase(connfd, "EXIST Teacher", "EXIST\r\n", "EXIST-Teacher");
		testcase(connfd, "DEL Teacher", "OK\r\n", "DEL-Teacher");
		testcase(connfd, "GET Teacher", "NO EXIST\r\n", "GET-Teacher");
		testcase(connfd, "MOD Teacher KING", "NO EXIST\r\n", "MOD-Teacher");
		testcase(connfd, "EXIST Teacher", "NO EXIST\r\n", "GET-Teacher");

	}

	struct timeval tv_end;
	gettimeofday(&tv_end, NULL);

	int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms

	printf("array testcase --> time_used: %d, qps: %d\n", time_used, 90000 * 1000 / time_used);

}


// 多 key 批量读写测试
void testcase_3w(int connfd) {

	int count = 10000;
	int i = 0;

	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);

	for (i = 0;i < count;i ++) {

		char cmd[128] = {0};
		snprintf(cmd, 128, "SET Teacher%d King%d", i, i);
		testcase(connfd, cmd, "OK\r\n", "SET-Teacher");
	}

	for (i = 0;i < count;i ++) {

		char cmd[128] = {0};
		snprintf(cmd, 128, "GET Teacher%d", i);

		char result[128] = {0};
		snprintf(result, 128, "King%d\r\n", i);
		
		testcase(connfd, cmd, result, "GET-King-Teacher");
	}

	for (i = 0;i < count;i ++) {

		char cmd[128] = {0};
		snprintf(cmd, 128, "MOD Teacher%d King%d", i, i);
		testcase(connfd, cmd, "OK\r\n", "GET-King-Teacher");
	}

	struct timeval tv_end;
	gettimeofday(&tv_end, NULL);

	int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms

	printf("rbtree testcase --> time_used: %d, qps: %d\n", time_used, 30000 * 1000 / time_used);

}


// testcase 192.168.243.131 2000 2 
int main(int argc, char *argv[]) {

	if (argc != 4) {
		printf("arg error\n");
		return -1;
	}

	char *ip = argv[1];
	int port = atoi(argv[2]);
	int mode = atoi(argv[3]);

	int connfd = connect_tcpserver(ip, port);

	if (mode == 0) {
		testcase_basics(connfd);
	} else if (mode == 1) {
		testcase_1w(connfd);
	} else if (mode == 2) {
		testcase_3w(connfd);
	}

	return 0;
	
}


