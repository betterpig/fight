#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <sys/uio.h>

class HttpConn
{
public:
    static const int FILENAME_LEN=200;
    static const int READ_BUFFER_SIZE=2048;
    static const int WRITE_BUFFER_SIZE=1024;
    enum METHOD {GET=0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT,PATCH};
    enum CHECK_STATE {  CHECK_STATE_REQUESTLINE=0,
                        CHECK_STATE_HEADER,
                        CHECK_STATE_CONTENT};
    enum HTTP_CODE {NO_REQUEST, 
                    GET_REQUEST, 
                    BAD_REQUEST, 
                    NO_RESOURCE,
                    FORBIDDEN_REQUEST, 
                    FILE_REQUEST,
                    INTERNAL_ERROR, 
                    CLOSED_CONNECTION};
    enum LINE_STATUS {LINE_OK=0,LINE_BAD,LINE_OPEN};

public:
    static int m_epollfd;
    static int m_user_count;

private:
    int m_sockfd;
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];
    int m_read_idx;
    int m_checked_idx;
    int m_start_line;
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;

    CHECK_STATE m_check_state;
    METHOD m_method;
    char m_real_file[FILENAME_LEN];
    char* m_url;
    char* m_version;
    char* m_host;
    int m_content_length;
    bool m_linger;
    char* m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    
public:
    HttpConn(){}
    ~HttpConn(){}

    void Init(int sockfd,const sockaddr_in& addr);
    void CloseConn(bool real_close=true);
    void Process();
    bool Read();
    bool Write();

private:
    void Init();
    HTTP_CODE ProcessRead();
    bool ProcessWrite(HTTP_CODE ret);
    HTTP_CODE ParseRequestLine(char* text);
    HTTP_CODE ParseHeaders(char* text);
    HTTP_CODE ParseContent(char* text);
    HTTP_CODE DoRequest();
    char* GetLine() {return m_read_buf+m_start_line; }
    LINE_STATUS ParseLine();

    void Unmap();
    bool AddResponse(const char* format,...);
    bool AddContent(const char* content);
    bool AddStatusLine(int status,const char* title);
    bool AddHeaders(int content_length);

};

#endif