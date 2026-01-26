#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>


#define DNS_SERVER_PORT 53
#define DNS_SERVER_IP   "114.114.114.114"
#define DNS_HOST        0x01
#define DNS_CNAME       0x05

char *domain[] = {
    "www.ntytcp.com",
    "bojing.wang",
    "www.baidu.com",
    "tieba.baidu.com",
    "news.baidu.com",
    "zhidao.baidu.com",
    "music.baidu.com",
    "image.baidu.com",
    "v.baidu.com",
    "map.baidu.com",
    "baijiahao.baidu.com",
    "xueshu.baidu.com",
    "cloud.baidu.com",
    "www.163.com",
    "open.163.com",
    "auto.163.com",
    "gov.163.com",
    "money.163.com",
    "sports.163.com",
    "tech.163.com",
    "edu.163.com",
    "www.taobao.com",
    "q.taobao.com",
    "sf.taobao.com",
    "yun.taobao.com",
    "baoxian.taobao.com",
    "www.tmall.com",
    "suning.tmall.com",
    "www.tencent.com",
    "www.qq.com",
    "www.aliyun.com",
    "www.ctrip.com",
    "hotels.ctrip.com",
    "hotels.ctrip.com",
    "vacations.ctrip.com",
    "flights.ctrip.com",
    "trains.ctrip.com",
    "bus.ctrip.com",
    "car.ctrip.com",
    "piao.ctrip.com",
    "tuan.ctrip.com",
    "you.ctrip.com",
    "g.ctrip.com",
    "lipin.ctrip.com",
    "ct.ctrip.com"
};

struct dns_header {
    unsigned short id;
    unsigned short flags;
    unsigned short questions;
    unsigned short answer;
    unsigned short authority;
    unsigned short additional;
};

int dns_create_header(struct dns_header *header) {
    if (!header) {
        return -1;
    }
    memset(header,0,sizeof(struct dns_header));

    header->id=rand();
    header->flags=htons(0x0100);
    header->questions=htons(1);
    return 0;
}


struct dns_question {
    int length;
    unsigned char *name;
    unsigned short qtype;
    unsigned short qclass;
};

int dns_create_question(struct dns_question *q,const char *hostname) {
    if (!q||!hostname) {
        return -1;
    }
    memset(q,0,sizeof(struct dns_question));

    size_t hostlen =strlen(hostname);
    q->name=(malloc(hostlen+2));
    if (!q->name) {
        return -1;
    }

    q->length=hostlen+2;
    q->qtype=htons(1);
    q->qclass=htons(1);

    const char delim[] = ".";
    char *tmp=strdup(hostname);
    char *tok=strtok(tmp,delim);
    unsigned char *dst =q->name;

    while (tok) {
        size_t len =strlen(tok);
        *dst++ =(unsigned char)len;
        memcpy(dst,tok,len);
        dst+=len;
        tok=strtok(NULL,delim);
    }
    *dst=0;

    free(tmp);
    return 0;
}


int dns_build_request(struct dns_header *h,
                      struct dns_question *q,
                      char *req,int rlen) {
    if (!h||!q||!req) {
        return -1;
    }
    memset(req,0,rlen);

    int offset=0;

    memcpy(req+offset,h,sizeof(*h));
    offset+=sizeof(*h);

    memcpy(req+offset,q->name,q->length);
    offset+=q->length;

    memcpy(req+offset,&q->qtype,sizeof(q->qtype));
    offset+=sizeof(q->qtype);

    memcpy(req+offset,&q->qclass,sizeof(q->qclass));
    offset+=sizeof(q->qclass);

    return offset;
}


static int is_pointer(int in) {
    return (in &0xc0)==0xc0;
}

static void dns_parse_name(unsigned char *chunk,
                           unsigned char *ptr,
                           char *out,int len) {
    int jump=0,n=0;
    char *pos=out+len;

    while (*ptr) {
        if (is_pointer(*ptr)) {

        }
    }


}


struct dns_item {
    char *domain;
    char *ip;
};




int main() {

    return 0;
}
