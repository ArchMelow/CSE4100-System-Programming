/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"

typedef struct item{
    int id;
    int left_stock;
    int price;
    struct item* left;
    struct item* right;
}item;

typedef struct {
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
}pool;

int byte_cnt = 0;
char buf_temp[MAXLINE]; //cmd for service_clients()
item* items = NULL; //binary tree data structure for storing the stock data

void handler(int sig); //catch CTRL+C to free the tree and terminate.
void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void service_clients(pool *p);
void show(item* items);
void init_tree();
void store_tree(FILE* fp, item* items);
void reset_tree(item* items);
void buy(item* items, int id, int num_to_buy);
void sell(item* items, int id, int num_to_sell);

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    char client_port[MAXLINE];
    char client_hostname[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    static pool pool; //Pool of connected descriptors (connfd)
    Signal(SIGINT, handler); //server gets CTRL+C
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }

    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);
    init_tree();
    while (1) {
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

        //If listening descriptor is ready, add new client to pool
        if (FD_ISSET(listenfd, &pool.ready_set)) {
            clientlen = sizeof(struct sockaddr_storage); //independent of protocol
            connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
            Getnameinfo((SA*)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
            printf("\033[32mConnected\033[0m to (%s, %s)\n", client_hostname, client_port);
            add_client(connfd, &pool);
        }

        //Service the clients
        service_clients(&pool);
        /*FILE* fp = fopen("stock.txt", "w");
        if (!fp) {
           perror("stock.txt does not exist\n");
           continue;
        }                           
        store_tree(fp, items);
        fclose(fp);
        */
    }
    reset_tree(items);
    exit(0);
}
/* $end echoserverimain */

void init_pool(int listenfd, pool *p)
{
    int i;
    p->maxi = -1;
    for (i=0; i< FD_SETSIZE; i++)
        p->clientfd[i] = -1;
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p)
{
    int i;
    p->nready--;
    for (i = 0; i< FD_SETSIZE; i++) //find available slot to add client fd
        if (p->clientfd[i] < 0) {
            //add connfd to the pool
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);

            //add the descriptor to descriptor set
            FD_SET(connfd, &p->read_set);

            //Update max descriptor and pool high water mark
            if (connfd > p->maxfd)
                p->maxfd = connfd;
            if (i > p->maxi)
                p->maxi = i;
            break;
        }
    if (i == FD_SETSIZE) //Failed to find an empty slot; pool is full of fds
        app_error("add_client error : Too many clients");
}

void service_clients(pool *p)
{
    int i, connfd, n;
    int id, num_s; //num_s : buying / selling n stocks
    char buf[MAXLINE];
    char choice[20];
    rio_t rio;
    //init_tree(&items);
    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];
        //If the descriptor is ready, write or read the data in stock.txt
        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
            p->nready--; 
            //echo(connfd);
            
            if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
                byte_cnt += n;
                printf("Server received %d (%d total) bytes on fd %d\n", n, byte_cnt, connfd);
                          
                if (!strcmp(buf, "show\n")) {
                    show(items);
                    strcat(buf, buf_temp); //copy the produced contents to buf
                    Rio_writen(connfd, buf, MAXLINE);
                    buf_temp[0] = '\0'; //reset the cmd string
                } 
           
                else {
                    sscanf(buf, "%s %d %d\n", choice, &id, &num_s);
                    if (!strcmp(choice, "buy")) {
                        buy(items, id, num_s);
                        strcat(buf, buf_temp);
                        Rio_writen(connfd, buf, MAXLINE);
                        buf_temp[0] = '\0';
                    } else if (!strcmp(choice, "sell")) {
                        sell(items, id, num_s);
                        strcat(buf, buf_temp);
                        Rio_writen(connfd, buf, MAXLINE);
                        buf_temp[0] = '\0';
                    } else {
                        Rio_writen(connfd, buf, MAXLINE);
                        //acts like echo server
                    }
                    choice[0] = '\0'; //reset choice
                }
            }
            //EOF detected : remove fd from the pool
             else {
                FILE* fp = fopen("stock.txt", "w");
                store_tree(fp, items);
                fclose(fp);
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
             }
        }
    }
}

void init_tree()
{
    int id, left_stock, price;
    item* cur;
    item* prev; 
    item* tmp;
    FILE* fp = fopen("stock.txt", "r"); //there must be stock.txt
    if (!fp) {
        perror("stock.txt NULL\n");
        return;
    }
    while(fscanf(fp, "%d %d %d\n", &id, &left_stock, &price) > 0) {
        cur = items;
        prev = NULL;
        tmp = (item*)malloc(sizeof(item));
        tmp-> id = id;
        tmp-> left_stock = left_stock;
        tmp-> price = price;
        tmp-> left = NULL;
        tmp-> right = NULL;
        while (cur) {
            prev = cur;
            if (id < cur->id)
                cur = cur->left;
            else if (id > cur->id)
                cur = cur->right;
        }
        //insert the data in the position.
        if (!prev) {
            prev = tmp;
            items = prev;
        } else if (id < prev->id) {
            prev->left = tmp;
        } else if (id > prev->id) {
            prev->right = tmp;
        }
    }
    fclose(fp);
}

void store_tree(FILE* fp, item* items)
{
    if (items) {
        fprintf(fp, "%d %d %d\n", items->id, items->left_stock, items->price);
        store_tree(fp, items->left);
        store_tree(fp, items->right);
    }
}

void buy(item* items, int id, int num_to_buy)
{
    if (!items) {
        return; //base case
    }
    if (id == (items->id)) {
        if (num_to_buy > (items->left_stock)){
            sprintf(buf_temp, "Not enough left stock\n");
            return;
        }
        items->left_stock = (items->left_stock) - num_to_buy;
        sprintf(buf_temp, "[buy] \033[32msuccess\033[0m\n");
        return;
    } else if (id < (items)->id) {
        buy(items->left, id, num_to_buy);
    } else if (id > (items)->id) {
        buy(items->right, id, num_to_buy);
    }
}

void sell(item* items, int id, int num_to_sell)
{
    if (!items)
        return; //base case
    if (id == (items->id)) {
        (*items).left_stock += num_to_sell;
        sprintf(buf_temp, "[sell] \033[32msuccess\033[0m\n");
        return;
    } else if (id < (items)->id) {
        sell(items->left, id, num_to_sell);
    } else if (id > (items)->id) {
        sell(items->right, id, num_to_sell);
    }
}

void show(item* items)
{
    if (items) {
        sprintf(buf_temp+strlen(buf_temp),"%d %d %d\n", items->id, items->left_stock, items->price);
        show(items->left);
        show(items->right);
    }
}

void handler(int sig) 
{
    reset_tree(items);
    Sio_puts("Server Terminated By CTRL+C\n");
    exit(0); //terminate the server process
}

void reset_tree(item* items)
{
    if (!items)
        return;
    reset_tree(items->left);
    reset_tree(items->right);
    free(items);
}
