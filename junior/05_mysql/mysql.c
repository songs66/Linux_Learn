//编译指令：gcc mysql.c -o mysql -I /usr/include/mysql/ -lmysqlclient
#include <stdio.h>
#include <string.h>
#include <mysql.h>


#define STU_DB_SERVER_IP    "192.168.174.130"
#define STU_DB_SERVER_PORT  3306

#define STU_DB_USERNAME     "admin"
#define STU_DB_PASSWORD     "songzihao1212"

#define STU_DB_DEFAULTDB    "student_db"


#define SQL_INSERT_TBL_CLASS_1  "insert tbl_class_1(stuname,gender,amount) values ('宋哥','1','666');"
#define SQL_SELECT_TBL_CLASS_1	"select * from tbl_class_1;"
#define SQL_DELETE_TBL_CLASS_1	"call proc_delete_stuname('宋哥')"

#define SQL_INSERT_TBL_CLASS_1_IMG  "insert tbl_class_1(stuname,gender,amount,head_img) values ('宋哥','1','666',?);"
#define SQL_SELECT_TBL_CLASS_1_IMG  "select head_img from tbl_class_1 where stuname='宋哥';"

#define FILE_IMAGE_LENGTH   (64*1024)

int mysql_select_tbl_class_1(MYSQL *handle) { //

    // mysql_real_query --> sql
    if (mysql_real_query(handle, SQL_SELECT_TBL_CLASS_1,
        strlen(SQL_SELECT_TBL_CLASS_1))) {
        printf("mysql_real_query : %s\n", mysql_error(handle));
        return -1;
    }

    // store -->
    MYSQL_RES *res = mysql_store_result(handle);
    if (res == NULL) {
        printf("mysql_store_result : %s\n", mysql_error(handle));
        return -2;
    }

    // rows / fields
    const int rows = mysql_num_rows(res);
    printf("rows: %d\n", rows);

    const int fields = mysql_num_fields(res);
    printf("fields: %d\n", fields);

    // fetch
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {

        int i = 0;
        for (i = 0;i < fields;i ++) {
            printf("%s\t", row[i]);
        }
        printf("\n");
    }
    //释放存储用的res
    mysql_free_result(res);

    return 0;
}

//先将图片从本地读入内存
int read_image(char *filename,char *buffer) {
    if (filename==NULL||buffer==NULL) {
        return -1;
    }
    FILE *fp=fopen(filename,"rb");
    if (fp==NULL) {
        perror("fopen failed\n");
        return -2;
    }

    fseek(fp,0,SEEK_END);
    const long long int length =ftell(fp);//返回当前读写指针相对于文件开头的偏移量
    fseek(fp,0,SEEK_SET);

    const long long int size =fread(buffer,1,length,fp);
    if (size!=length) {
        perror("fread failed");
        return -3;
    }

    fclose(fp);
    return size;
}
//将buffer中的存入磁盘中
int write_image(char *filename,char *buffer,int length) {

    if (filename == NULL || buffer == NULL || length <= 0) return -1;

    FILE *fp = fopen(filename, "wb+"); //
    if (fp == NULL) {
        printf("fopen failed\n");
        return -2;
    }

    int size = fwrite(buffer, 1, length, fp);
    if (size != length) {
        printf("fwrite failed: %d\n", size);
        return -3;
    }

    fclose(fp);

    return size;
}

//将本地缓存的图片二进制文件传入mysql服务器
int mysql_write(MYSQL *handle,char *buffer,int length) {
    if (handle == NULL || buffer == NULL || length <= 0) {
        return -1;
    }

    MYSQL_STMT *stmt = mysql_stmt_init(handle);
    if (mysql_stmt_prepare(stmt, SQL_INSERT_TBL_CLASS_1_IMG, strlen(SQL_INSERT_TBL_CLASS_1_IMG))) {
        printf("mysql_stmt_prepare : %s\n", mysql_error(handle));
        return -2;
    }
    //作用：告诉 MySQL “预处理语句里的 ? 占位符应该用什么值”。
    MYSQL_BIND param={0};
    param.buffer_type=MYSQL_TYPE_LONG_BLOB;
    param.buffer=NULL;
    param.is_null=0;
    param.length=NULL;
    //绑定输入参数
    if (mysql_stmt_bind_param(stmt,&param)) {
        printf("mysql_stmt_bind_param : %s\n", mysql_error(handle));
        return -3;
    }
    //发送数据（分块）                         参数序号 0 对应 SQL 里第一个 ?
    if (mysql_stmt_send_long_data(stmt,0,buffer,length)) {
        printf("mysql_stmt_send_long_data : %s\n", mysql_error(handle));
        return -4;
    }
    //执行预处理语句
    if (mysql_stmt_execute(stmt)) {
        printf("mysql_stmt_execute : %s\n", mysql_error(handle));
        return -5;
    }
    //关闭stmt
    if (mysql_stmt_close(stmt)) {
        printf("mysql_stmt_close : %s\n", mysql_error(handle));
        return -6;
    }

    return 0;
}

//从服务器中将图片传入本地缓存
int mysql_read(MYSQL *handle,char *buffer,int length) {
    if (handle == NULL || buffer == NULL || length <= 0) {
        return -1;
    }

    MYSQL_STMT *stmt = mysql_stmt_init(handle);
    if (mysql_stmt_prepare(stmt, SQL_SELECT_TBL_CLASS_1_IMG, strlen(SQL_SELECT_TBL_CLASS_1_IMG))) {
        printf("mysql_stmt_prepare : %s\n", mysql_error(handle));
        return -2;
    }
    //告诉 MySQL “查询回来的每一行数据应该放到我哪些变量里”。
    MYSQL_BIND result={0};
    result.buffer_type=MYSQL_TYPE_LONG_BLOB;
    unsigned long total_length =0;
    result.length=&total_length;
    //绑定结果参数
    if (mysql_stmt_bind_result(stmt, &result)) {
        printf("mysql_stmt_bind_result : %s\n", mysql_error(handle));
        return -3;
    }
    //执行预处理语句
    if (mysql_stmt_execute(stmt)) {
        printf("mysql_stmt_execute : %s\n", mysql_error(handle));
        return -4;
    }
    //把所有行预先拉到本地缓冲区，后续 fetch 不再做网络 IO
    if (mysql_stmt_store_result(stmt)) {
        printf("mysql_stmt_store_result : %s\n", mysql_error(handle));
        return -5;
    }

    while (1) {
        int ret = mysql_stmt_fetch(stmt);
        if (ret != 0 && ret != MYSQL_DATA_TRUNCATED) {  //相当于 if (ret == MYSQL_NO_DATA)
            break;
        }

        int start = 0;
        const size_t chunk = 64 * 1024;
        while (start < total_length) {
            size_t want = (chunk < total_length - start) ? chunk : total_length - start;
            result.buffer        = buffer + start;
            result.buffer_length = want;
            mysql_stmt_fetch_column(stmt, &result, 0, start);
            start += result.buffer_length;   // 库返回实际读到的字节数
        }
        // while (start < (int)total_length) {
        //     result.buffer = buffer + start;
        //     result.buffer_length = 1;
        //     mysql_stmt_fetch_column(stmt, &result, 0, start);
        //     start += result.buffer_length;
        // }
    }

    mysql_stmt_close(stmt);
    return total_length;
}

int main() {
    MYSQL mysql;
    //初始化MySQL结构体
    if (mysql_init(&mysql)==NULL) {
        printf("mysql_init:%s\n",mysql_error(&mysql));
        return -1;
    }
    //连接数据库
    if (!mysql_real_connect(&mysql,STU_DB_SERVER_IP,STU_DB_USERNAME,STU_DB_PASSWORD,
        STU_DB_DEFAULTDB,STU_DB_SERVER_PORT,NULL,0)) {
        printf("mysql_real_connect:%s\n",mysql_error(&mysql));
        return -2;
    }
#if 1
    printf("case : mysql --> read image and write mysql\n");

    char buffer[FILE_IMAGE_LENGTH]={0};
    int length=read_image("../resources/0voice.jpg",buffer);
    if (length<=0) {
        goto Exit;
    }
    //插入数据库
    mysql_write(&mysql,buffer,length);

    printf("case : mysql --> read mysql and write image\n");

    memset(buffer,0,FILE_IMAGE_LENGTH);
    length=mysql_read(&mysql,buffer,FILE_IMAGE_LENGTH);

    write_image("result.jpg",buffer,length);


#endif

#if 0
    // mysql --> insert
    printf("case : mysql --> insert \n");

    if (mysql_real_query(&mysql, SQL_INSERT_TBL_CLASS_1, strlen(SQL_INSERT_TBL_CLASS_1))) {
        printf("mysql_real_query : %s\n", mysql_error(&mysql));
        goto Exit;
    }
    mysql_select_tbl_class_1(&mysql);

    // mysql --> delete
    printf("case : mysql --> delete \n");

    if (mysql_real_query(&mysql, SQL_DELETE_TBL_CLASS_1, strlen(SQL_DELETE_TBL_CLASS_1))) {
        printf("mysql_real_query : %s\n", mysql_error(&mysql));
        goto Exit;
    }
    mysql_select_tbl_class_1(&mysql);
#endif

    Exit:
        mysql_close(&mysql);
    printf("-----------------------finish-----------------------\n");
    return 0;
}