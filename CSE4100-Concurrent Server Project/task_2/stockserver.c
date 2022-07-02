/* 
 * 
 */
/* $begin stockserver.c (threaded concurrent server) */
#include "csapp.h"
//need to include the contents of sbuf.c into a single file.
#define NTHREADS  20
#define SBUFSIZE  16


void *thread(void *vargp);
static void init_thread();

typedef struct {
    int *buf; //buffer array
    int n; //maximum slots (buffer size)
    int front; //buf[(front+1)%n] is the first item
    int rear; //buf[rear%n] is the last item
    sem_t mutex; //protects accesses to buf
    sem_t slots; //available slots to be produced
    sem_t items; //available items to be consumed
}sbuf_t;

typedef struct item {
    int id;
    int left_stock;
    int price;
    int readcnt;
    struct item* left;
    struct item* right;
    sem_t mutex; //lock for reading
    sem_t mutex_w; //lock for writing
}item;

static sem_t mutex; //mutex
//static int byte_cnt;
//int read_cnt; //number of threads running "show" command
//int write_cnt;
//static sem_t mutex_b; //initially = 1
char buf_temp[MAXLINE];

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);
void handler(int sig);
void show();
void init_tree();
void store_tree(FILE* fp, item* items);
void buy(int id, int num_to_buy);
void sell(int id, int num_to_sell);
void show_read(item* items);
item* buy_read(item* items, int id, int num_to_buy);
item* sell_read(item* items, int id, int num_to_sell);
void reset_tree(item* items);

item* items = NULL; //binary tree initialization
sbuf_t sbuf; // Shared buffer of connected descriptors

int main(int argc, char **argv) 
{
    int i, listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid; 
    
    Signal(SIGINT, handler); //Catch Ctrl+C to free tree before terminating the server

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }
    listenfd = Open_listenfd(argv[1]);
    init_tree();

    sbuf_init(&sbuf, SBUFSIZE); //line:conc:pre:initsbuf
    for (i = 0; i < NTHREADS; i++)  /* Create worker threads */ //line:conc:pre:begincreate
	Pthread_create(&tid, NULL, thread, NULL);               //line:conc:pre:endcreate
   
    while (1) { 
        clientlen = sizeof(struct sockaddr_storage);
	    connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
	    sbuf_insert(&sbuf, connfd); /* Insert connfd in buffer */
    }
    reset_tree(items); //release memory for the tree data structure
    exit(0);
}

void *thread(void *vargp) 
{  
    Pthread_detach(pthread_self()); 
    int n;
    int id, num_s;
    char buf[MAXLINE];
    char choice[15];
    rio_t rio;
    while (1) { 
	    int connfd = sbuf_remove(&sbuf); /* Remove connfd from buffer */ //line:conc:pre:removeconnfd
        static pthread_once_t once = PTHREAD_ONCE_INIT;
        Pthread_once(&once, init_thread);
        Rio_readinitb(&rio, connfd);
        while ((n= Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
            P(&mutex);
            printf("Server(Worker Thread) %u received %d bytes on fd %d\n", (unsigned int) pthread_self(), n, connfd);
            buf_temp[0] = '\0'; //reset the cmd string
            choice[0] = '\0'; //reset choice
            V(&mutex);

            if (!strcmp(buf, "show\n")) {
                
                show();
                strcat(buf, buf_temp);
                Rio_writen(connfd, buf, MAXLINE);
            }
             
            else {
                sscanf(buf, "%s %d %d\n", choice, &id, &num_s);
                if (!strcmp(choice, "buy")) {
                    buy(id, num_s);
                    strcat(buf, buf_temp);
                } else if (!strcmp(choice, "sell")) {
                    sell(id, num_s);
                    strcat(buf, buf_temp);
                }
                Rio_writen(connfd, buf, MAXLINE);
            }
        }
	    Close(connfd);
        //If the buffer is empty, write the changes to stock.txt
        if (sbuf.front == sbuf.rear) {
            FILE *fp = fopen("stock.txt", "w");
            if (!fp) {
                perror("stock.txt NULL\n");
            }
            store_tree(fp, items);
            fclose(fp);
            //reset_tree(items); //reset tree to start over 
        }
    }
}

static void init_thread()
{
    Sem_init(&mutex, 0, 1);
    //read_cnt = 0; //number of threads reading the stock data (show)
    //write_cnt = 0;
    //byte_cnt = 0;   
}

void init_tree()
{
    int id, left_stock, price;
    item* cur;
    item* prev;
    item* tmp;
    FILE* fp = fopen("stock.txt", "r");
    if (!fp) {
        perror("stock.txt NULL\n");
        return;
    }
    while (fscanf(fp, "%d %d %d\n", &id, &left_stock, &price) > 0) {
        cur = items;
        prev = NULL;
        tmp = (item*)malloc(sizeof(item));
        tmp-> id = id;
        tmp-> left_stock = left_stock;
        tmp-> price = price;
        tmp-> left = NULL;
        tmp-> right = NULL;
        tmp-> readcnt = 0;
        Sem_init(&(tmp->mutex), 0, 1); //mutex of the data to 1 (initialize)
        Sem_init(&(tmp->mutex_w), 0, 1); 
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

void buy(int id, int num_to_buy)
{
    
    item* stock = buy_read(items, id, num_to_buy);   
    if (!stock)
        return;
    else {
        P(&(stock-> mutex_w));
        if (num_to_buy > (stock->left_stock)){
            sprintf(buf_temp, "Not enough left stock\n");
        }
        else {
            stock->left_stock = stock->left_stock - num_to_buy;
            sprintf(buf_temp, "[buy] \033[32msuccess\033[0m\n");
        }
        V(&(stock-> mutex_w));
    }                                                                                                                    
                                                                                           
}

void sell(int id, int num_to_sell)
{
    item* stock = sell_read(items, id, num_to_sell);
    if (!stock)
        return;
    else {
        P(&(stock-> mutex_w));
        stock->left_stock += num_to_sell;
        sprintf(buf_temp, "[sell] \033[32msuccess\033[0m\n");
        V(&(stock-> mutex_w));
    }
}

void show()
{
    show_read(items);
}

void show_read(item* items)
{
    if (items) {
        P(&(items->mutex));
        (items->readcnt)++;
        if ((items->readcnt) == 1)
            P(&(items-> mutex_w));
        V(&(items->mutex));

        sprintf(buf_temp+strlen(buf_temp),"%d %d %d\n",items->id, items->left_stock, items->price);
        
        P(&(items->mutex));
        (items->readcnt)--;
        if ((items->readcnt) == 0)
            V(&(items-> mutex_w));
        V(&(items->mutex));

        show_read(items->left);
        show_read(items->right);    
    }
}

item* buy_read(item* items, int id, int num_to_buy)
{
    if (!items)
        return NULL; //base case
    if (id == (items->id)) {
        P(&(items->mutex)); //semaphore for LOCKING access to variable readnum   
        (items->readcnt)++; //increment the number of reader
        if ((items->readcnt) == 1) //the first reader in
            P(&(items-> mutex_w)); //lock the access to corresponding items->left_stock
        V(&(items->mutex));
            
        P(&(items->mutex));
        (items->readcnt)--;
        if ((items->readcnt) == 0) //the last reader out
            V(&(items-> mutex_w));
        V(&(items->mutex));
        return items;     
            /*if (num_to_buy > (items->left_stock)){
                sprintf(buf_temp, "Not enough left stock\n");
                (items->readnum)--; //decrement the number of reader
                V(&(items->mutex));
                return items;
            }
            items->left_stock = items->left_stock - num_to_buy;
            sprintf(buf_temp, "[buy] success\n");
            (items->readnum)--;
            V(&(items->mutex));*/
    } else if (id < (items)->id) {
        buy_read(items->left, id, num_to_buy);
    } else if (id > (items)->id) {
        buy_read(items->right, id, num_to_buy);
    }
    //return items;
}



item* sell_read(item* items, int id, int num_to_sell)
{
    //when we compile the code, there may be warning indicating that the control might reach the end of the function, which is caused by semaphore. It's okay because we know that this function will always return NULL or items. Same explanation for buy_read().

    if (!items)
        return NULL; //base case
    if (id == (items->id)) {
        P(&(items->mutex));
        (items->readcnt)++;
        if ((items->readcnt) == 1)
            P(&(items-> mutex_w));
        V(&(items->mutex));
        /*
        items->left_stock += num_to_sell;
        sprintf(buf_temp, "[sell] success\n");
        */

        P(&(items->mutex));
        (items->readcnt)--;
        if ((items->readcnt) == 0)
            V(&(items-> mutex_w));
        V(&(items->mutex));
        return items;
    } else if (id < (items->id)) {
        sell_read(items->left, id, num_to_sell);
    } else if (id > (items->id)) {
        sell_read(items->right, id, num_to_sell);
    }
    //return items;
}

void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n; //maximum # of items to hold
    sp->front = sp->rear = 0; //init buffer
    Sem_init(&sp->mutex, 0, 1); //binary semaphore for locking
    Sem_init(&sp->slots, 0, n); //Counting semaphore for slots to produce
    Sem_init(&sp->items, 0, 0); //Counting semaphore for items to consume
}

void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf); //clean up the buffer
}

void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots); //wait for available slot
    P(&sp->mutex); //lock the buffer
    sp->buf[(++sp->rear)%(sp->n)] = item; //insert the item (critical section)
    V(&sp->mutex); //unlock the buffer
    V(&sp->items); //Announce available items
}

int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp->items);
    P(&sp->mutex);
    item = sp->buf[(++sp->front)%(sp->n)]; //remove the item (critical section)
    V(&sp->mutex);
    V(&sp->slots);
    return item;
}

void handler(int sig)
{
    reset_tree(items);
    Sio_puts("Server Terminated By CTRL+C\n");
    exit(0);
}

void reset_tree(item* items)
{
    if (!items)
        return;
    reset_tree(items->left);
    reset_tree(items->right);
    free(items);
}

/* $end stockserver.c (thread based concurrent server) */
