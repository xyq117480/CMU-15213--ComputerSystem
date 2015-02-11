/*
 *
 * Name: Aihua Peng
 * AndrewID: aihuap
 *
 * This is a multithreadable web proxy without cache.
 * It could delegate GET request from client.
 *
 */

#include <stdio.h>
#include "csapp.h"

/* Helper macro */
#define NOT_MATCH(a, b) (strncmp(a, b, strlen(b)))

/* Global static representatives */
static const char *_userAgent= "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *_accept = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *_acceptEncoding = "Accept-Encoding: gzip, deflate\r\n";
static const char *_connection = "Connection: close\r\n";
static const char *_proxyConnection = "Proxy-Connection: close\r\n";


/* Thread function */
void *doit(void *connfdp);

/* Helper functions */
int parse_uri(char *uri, char *host, char *port, char *suffix);
void construct_requestLine(char *requestLineBuf, char *suffix);
void construct_messageHeader(char *messageHeaderBuf, rio_t *clientriop, char *host);
void construct_header (rio_t *clientriop, char *to_server_buf, char *host, char *suffix);


/*
 * main
 */
int main(int argc, char *argv[]) {
    
    /* Check command argument */
    if (argc != 2) {
        printf("Command error. \n");
        exit(1);
    }
    int listenfd, clientlen, *connfd =0;
    char *port;
    struct sockaddr_in clientaddr;
    pthread_t tid;
    
    
    /* Open a listen port */
    port = argv[1];
    if((listenfd = open_listenfd(port)) < 0) {
        printf("Invalid port %s\n", argv[1]);
        exit(1);
    }
    
    /* Create thread for each connection */
    clientlen = sizeof(clientaddr);
    while (1) {
        connfd = Malloc(sizeof(int));
        if((*connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen))>=0){
            Pthread_create(&tid, NULL, doit, (void*)connfd);
        }
        else{
            printf("Accept socket error\n");
        }
    }
    return 0;
}

/*
 * doit - Analyze the request from client.
 *       Send request to server and write response to client.
 *       This is the thread routine.
 */

void *doit(void *connfdp) {
    Pthread_detach(pthread_self());
    char buf[MAXLINE],
    method[MAXLINE], uri[MAXLINE], version[MAXLINE],
    port[MAXLINE], host[MAXLINE], suffix[MAXLINE],
    header[MAXLINE];
    int clientfd = *(int *)connfdp;
    int serverfd = 0;
    long buflen = 0;
    rio_t clientrio, serverrio;
    
    /* Read init */
    Rio_readinitb(&clientrio, clientfd);
    /* Check read from client */
    if(Rio_readlineb(&clientrio, buf, MAXLINE)!=0){
        sscanf(buf, "%s %s %s", method, uri, version);
    }
    else{
        printf("Read error from client \n");
    }
    
    
    /* If the request method is GET */
    if (!strcmp(method, "GET")) {
        parse_uri(uri, host, port, suffix);
        construct_header(&clientrio, header, host, suffix);
        
        /* Return error to client */
        if ((serverfd = open_clientfd(host, port)) < 0) {
            Rio_writen(clientfd, "Request eror\r\n", strlen("Request error\r\n"));
            
        }
        /* Ask the server and write the response to client */
        else{
            Rio_readinitb(&serverrio, serverfd);
            Rio_writen(serverfd, header, strlen(header));
            while((buflen = Rio_readlineb(&serverrio, buf, MAXLINE)) != 0){
                Rio_writen(clientfd, buf, buflen);
            }
        }
        
        Close(serverfd);
    }
    else{
        Rio_writen(clientfd, "Method not support\r\n", sizeof("Method not support\r\n"));
    }
    
    Close(clientfd);
    return NULL;
}


/*
 * parse_uri - Parse the http uri in this format :
 *         http://www.cmu.edu:8080/cs/index.html
 *         The port is opotional.
 *
 *  Return 0 on success.
 */
int parse_uri(char *uri, char *host, char *port, char *suffix)
{
    /* Check the uri starts with http:// */
    if (strncasecmp(uri, "http://", 7)) {
        return -1;
    }
    *port = 80;
    
    /* Try to find the host */
    char *ptr = uri + 7;
    while (*ptr != ':') {
        *host = *ptr;
        if (*ptr == '/') {
            break;
        }
        host++;
        ptr++;
    }
    *host = '\0';
    
    /* Try to find the port */
    if (*ptr == ':') {
        *ptr = '\0';
        ptr++;
        while (*ptr != '/') {
            *port = *ptr;
            ptr++;
            port++;
        }
        *port = '\0';
        
    }
    
    /* The remaining is suffix */
    strcpy(suffix, ptr);
    
    return 0;
}



/*
 * construct_requestLine - Construct the first line (request line)
 *               of a http message. GET is the one we support.
 */
void construct_requestLine(char *requestLineBuf, char *suffix){
    sprintf(requestLineBuf, "GET %s HTTP/1.0\r\n", suffix);
}


/* 
 * construct_messageHeader - Construct the header of a http message.
 *               The format can be found in global statics.
 *               In order to support HTTP 1.1, the "Host: xxx" is optional.
 */
 
void construct_messageHeader(char *messageHeaderBuf, rio_t *clientriop, char *host){
    char host_buf[MAXLINE] ="";
    char temp_buf[MAXLINE];
    
    /* Try to find host */
    while (Rio_readlineb(clientriop,temp_buf,MAXLINE)>0) {
        if (!strcmp(temp_buf, "\r\n")) {
            break;
        }
        if (!NOT_MATCH(temp_buf, "Host")) {
            strcpy(host_buf, temp_buf);
        }
    }
    
    /* Host not found from request */
    if (!strlen(host_buf)) {
        sprintf(host_buf, "Host: %s\r\n", host);
    }
    sprintf(messageHeaderBuf, "%s%s%s%s%s%s\r\n",
            host_buf,
            _userAgent,
            _accept,
            _acceptEncoding,
            _connection,
            _proxyConnection);
}


/*
 * construct_header - Construct the overall header of a http message.
 *          This function combines the above two construct functions.
 *
 */
void construct_header (rio_t *clientriop, char *header, char *host, char *suffix) {
    
    char requestLine_buf[MAXLINE];
    construct_requestLine(requestLine_buf, suffix);
    
    char messsageHeader_buf[MAXLINE];
    construct_messageHeader(messsageHeader_buf, clientriop,host);
    
    /* Combine requestLine and messageHeader */
    sprintf(header, "%s%s", requestLine_buf, messsageHeader_buf);
    return;
}




