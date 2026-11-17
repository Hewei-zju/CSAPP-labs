#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

typedef struct URLinfo{
    char hostname[256];
    char port[16];
    char path[1024];
}URLinfo;
int client_handler(int connfd);
ssize_t str_cat(char *bf,size_t s,char **strs,size_t n);
int parse_URL(char *url,URLinfo* info);
int parse_request_line(char *bf,char *URL);
int Memcpy(char *dest,size_t s, char *src, size_t n);
void *thread(void *vargp);



int parse_URL(char *url,URLinfo* info) {
    char *host = strstr(url,"http://");
    if (host == NULL) {
        fprintf(stderr,"Wrong URL\n");
        return -1;
    }else {
        host += 7;
        char *port = strchr(host,':');
        char *path = strchr(host,'/');
        if (port != NULL && path != NULL && port < path) {
            //port and path
            if (Memcpy(info->hostname,sizeof(info->hostname),host,port-host) < 0) return -1;
            if (Memcpy(info->port,sizeof(info->port),port+1,path-port-1) < 0) return -1;
            if (Memcpy(info->path,sizeof(info->path),path,strlen(path)) < 0) return -1;

            // memcpy(info->hostname,host,port-host);
            // info->hostname[port-host] = '\0';
            // memcpy(info->port,port+1,path-port-1);
            // info->port[path-port-1] = '\0';
            // strcpy(info->path,path);
        }else if (port != NULL && path == NULL) {
            //port and no path
            if (Memcpy(info->hostname,sizeof(info->hostname),host,port-host) < 0) return -1;
            if (Memcpy(info->port,sizeof(info->port),port+1,strlen(port+1)) < 0) return -1;
            if (Memcpy(info->path,sizeof(info->path),"/",1) < 0) return -1;
            // memcpy(info->hostname,host,port-host);
            // info->hostname[port-host] = '\0';
            // strcpy(info->port,port+1);
            // strcpy(info->path,"/");
        }else if ((port == NULL && path != NULL) || (port != NULL && path != NULL && port > path) ) {
            //path and no port
            if (Memcpy(info->hostname,sizeof(info->hostname),host,path-host) < 0) return -1;
            if (Memcpy(info->path,sizeof(info->path),path,strlen(path) ) < 0) return -1;
            if (Memcpy(info->port,sizeof(info->port),"80",2) < 0) return -1;
            // memcpy(info->hostname,host,path-host);
            // info->hostname[path-host] = '\0';
            // strcpy(info->path,path);
            // strcpy(info->port,"80");
        }
        else {
            //no port and no path
            if (Memcpy(info->hostname,sizeof(info->hostname),host,strlen(host)) < 0) return -1;
            if (Memcpy(info->port,sizeof(info->port),"80",2) < 0) return -1;
            if (Memcpy(info->path,sizeof(info->path),"/",1) < 0) return -1;
            // strcpy(info->hostname,host);
            // strcpy(info->port,"80");
            // strcpy(info->path,"/");
        }
    }
    return 0;
}
int main(int argc,char **argv)
{
    if (argc != 2) {
        fprintf(stderr,"Require 2 param but received %d",argc);
        return 1;
    }
    if (signal(SIGPIPE,SIG_IGN) == SIG_ERR) {
        perror("signal:");
        return 1;
    }
    char *listen_port = argv[1];
    int listen_fd;
    if ((listen_fd = open_listenfd(listen_port)) < 0) {
        perror("Listenfd open failed");
        return 1;
    }
    struct sockaddr_in client_sockaddr;
    int *connfdp;
    pthread_t tid;
    while(1) {
        connfdp = Malloc(sizeof(int));
        socklen_t addrlen = sizeof(struct sockaddr_in);
        if ((*connfdp = accept(listen_fd,(struct sockaddr *)&client_sockaddr,&addrlen)) == -1) {
            perror("Connect fd failed");
            free(connfdp);
            continue;
        }
        Pthread_create(&tid,NULL,thread,connfdp);
        // if (client_handler(connfd) < 0) {
        //     fprintf(stderr,"client failed\n");
        // }
        // close(connfd);
    }
    close(listen_fd);
    return 0;
}

int client_handler(int connfd) {
    ssize_t n;
    rio_t client_rp;
    char bf[MAXBUF];
    rio_readinitb(&client_rp,connfd);
    n = rio_readlineb(&client_rp,bf,MAXLINE); 
    if(n < 0) {
        perror("rio_readlineb");
        return -1;
    }else if (n == 0) {
        fprintf(stdout,"EOF\n");
        return -1;
    }
    char URL[MAXLINE];
    if (parse_request_line(bf,URL) < 0) {
        return -1;
    }
    // if(sscanf(bf,"GET %s %*s",URL) != 1) {
    //     fprintf(stderr,"400 Bad Request\n");
    //     return -1;
    // }
    URLinfo info;
    if (parse_URL(URL,&info) < 0) {
        return -1;
    }
    char *strs[] = {"GET ",info.path," HTTP/1.0\r\n"};
    if (str_cat(bf,sizeof(bf),strs,3) < 0) {
        return -1;
    } 
    //create connection between proxy and server
    int serverfd;
    if((serverfd = open_clientfd(info.hostname,info.port)) < 0) {
        return -1;
    }
    //send request line
    if (rio_writen(serverfd,bf,strlen(bf)) < 0) {
        perror("rio_writen:");
        goto cleanup;
    }
    //handle the request heads
    int sendhost = 0;
    int send_con = 0;
    int send_pro_con = 0;
    while((n = rio_readlineb(&client_rp,bf,MAXLINE)) > 0) {
        if (strcmp(bf,"\r\n") == 0) break;
        char *colomn = strchr(bf,':');
        char request_head[MAXLINE];
        if (colomn == NULL) {
            fprintf(stderr,"Invalid request head\n");
            goto cleanup;
        }
        if (Memcpy(request_head,MAXLINE,bf,colomn - bf) < 0) {
            goto cleanup;
        }
        request_head[colomn-bf] = '\0';
        if (strcasecmp(request_head,"Host") == 0) {
            sendhost = 1;
        }else if(strcasecmp(request_head,"Connection") == 0) {
            send_con = 1;
            strcpy(bf,"Connection: close\r\n");
        }else if (strcasecmp(request_head,"Proxy-Connection") == 0) {
            send_pro_con = 1;
            strcpy(bf,"Proxy-Connection: close\r\n");
        }else if(strcasecmp(request_head,"User-Agent") == 0) {
            strcpy(bf,user_agent_hdr);
        }
        if (rio_writen(serverfd,bf,strlen(bf)) < 0) {
            perror("rio_write:");
            goto cleanup;
        }
    }
    if(n == -1) {
        perror("rio_read:");
        goto cleanup;
    }else if(n==0) {
        fprintf(stderr,"Unexpected EOF in request\n");
        goto cleanup;
    }
    if (sendhost == 0) {
        char *hosthead[] = {"Host: ",info.hostname,":",info.port,"\r\n"};
        if (str_cat(bf,sizeof(bf),hosthead,5) < 0) {
            goto cleanup;
        }
        if (rio_writen(serverfd,bf,strlen(bf)) < 0) {
            perror("rio_write:");
            goto cleanup;
        }
    }
    if (send_con == 0) {
        strcpy(bf,"Connection: close\r\n");
        if (rio_writen(serverfd,bf,strlen(bf)) < 0) {
            perror("rio_write:");
            goto cleanup;
        }
    }
    if (send_pro_con == 0) {
        strcpy(bf,"Proxy-Connection: close\r\n");
        if (rio_writen(serverfd,bf,strlen(bf)) < 0) {
            perror("rio_write:");
            goto cleanup;
        }
    }
    //empty line
    strcpy(bf,"\r\n");
    if (rio_writen(serverfd,bf,strlen(bf)) < 0) {
        perror("rio_write:");
        goto cleanup;
    }
    fprintf(stdout,"Received request from client and send it to server\n");
    //wait for answer from server
    char server_buf[MAXLINE];
    rio_t server_rp;
    rio_readinitb(&server_rp,serverfd);
    while((n = rio_readnb(&server_rp,server_buf,sizeof(server_buf))) > 0) {
        if (rio_writen(connfd,server_buf,n) != n) {
            perror("rio_write:");
            goto cleanup;
        }
    }
    if (n < 0) {
        perror("rio_read:");
        goto cleanup;
    }
    close(serverfd);
    return 0;

cleanup:
    close(serverfd);
    return -1;
}
ssize_t str_cat(char *bf,size_t s, char **strs,size_t n) {
    ssize_t len = 0;
    ssize_t str_len = 0;
    for(ssize_t i = 0 ; i < n;i++) {
        str_len = strlen(strs[i]);
        if (Memcpy(bf+len,s-len,strs[i],str_len) < 0) {
            return -1;
        }
        len += str_len;
    }
    bf[len] = '\0';
    return len;
}

int parse_request_line(char *bf,char *URL) {
    char method[16];
    char version[16];
    if (sscanf(bf,"%15s %8191s %15s",method,URL,version) != 3 || strcmp(method,"GET") != 0) {
        fprintf(stderr,"Invalid request\n");
        return -1;
    }
    return 0;
}

int Memcpy(char *dest, size_t s, char *src, size_t n) {
    if (n >= s) {
        fprintf(stderr,"Memcpy: out of bound\n");
        return -1;
    }
    memcpy(dest,src,n);
    dest[n] = '\0';
    return 0;
}

void *thread(void *vargp) {
    Pthread_detach(Pthread_self());
    int connfd = *(int *)vargp;
    free(vargp);
    if (client_handler(connfd) < 0) {
        fprintf(stderr,"Client failed\n");
    }
    close(connfd);
    return NULL;
}