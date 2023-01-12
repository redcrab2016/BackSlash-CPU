/* debug of BS3 CPU through telnet socket */
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h> // read(), write(), close()
#include <errno.h>
#include <fcntl.h>

#include "bs3_asm.h"
#include "bs3_debug.h"

#define BS3_DEBUG_STATE_RUNNING 0
#define BS3_DEBUG_STATE_STOPPED 1
#define BS3_DEBUG_STATE_TOSTOP  2

#define BS3_DEBUG_COMM_STATE_NOSERVICE      0
#define BS3_DEBUG_COMM_STATE_NOCONNECTION   1
#define BS3_DEBUG_COMM_STATE_CONNECTED      2

#define BS3_DEBUG_COMM_MAXSIZE 80
#define BS3_DEBUG_COMM_DEFAULTPORT 35853
#define SA struct sockaddr

struct bs3_debug_data
{
    struct bs3_cpu_data * pbs3;         /* BS3 cpu object reference                         */
    int debug_state;                    /* cpu debug state , running or stopped             */
    int comm_state;                     /* socket communication state                       */
    int debug_stop_at;                  /* address where the CPU must stop                  */
    int debug_step_count;               /* how may step                                     */
    int lastPC;                         /* last final PC at last unassemble cmd             */
    int canQuit;                        /* when CPU HALT status, can we quit the debugger   */
    /* communication tech data */
    int count;                          /* latency counter                                  */
    int port;                           /* listening port                                   */
    int connfd;                         /* connection descriptor                            */
    int sockfd;                         /* listening socket descriptor                      */
    struct sockaddr_in servaddr;        /* listening bind address                           */
    struct sockaddr_in cli;             /* client address                                   */
    /* client command */
    char buff[BS3_DEBUG_COMM_MAXSIZE];  /* input string from client                         */
    int n;                              /* current size of buff                             */
};

struct bs3_debug_data bs3debug;

void bs3_debug_finish(struct bs3_debug_data * pbs3debug)
{
    switch (pbs3debug->comm_state) /* close connection and listening socket */
    {
        case BS3_DEBUG_COMM_STATE_CONNECTED:
            if (pbs3debug->connfd)
            {
                close(pbs3debug->connfd);
                pbs3debug->connfd = 0;
            }
            pbs3debug->comm_state = BS3_DEBUG_COMM_STATE_NOCONNECTION;
        case BS3_DEBUG_COMM_STATE_NOCONNECTION:
            if (pbs3debug->sockfd)
            {
                close(pbs3debug->sockfd);
                pbs3debug->sockfd = 0;
            }
            pbs3debug->comm_state = BS3_DEBUG_COMM_STATE_NOSERVICE;
        case BS3_DEBUG_COMM_STATE_NOSERVICE:
            break;
    }
    pbs3debug->debug_state = BS3_DEBUG_STATE_RUNNING;
    pbs3debug->n = 0;
}

void bs3_debug_init(struct bs3_debug_data * pbs3debug, int port)
{
    bs3_debug_finish(pbs3debug);
    memset(pbs3debug, 0, sizeof(struct bs3_debug_data));
    pbs3debug->port = (port == 0)?BS3_DEBUG_COMM_DEFAULTPORT:port;/* if port is 0 then take default port */
    pbs3debug->debug_state = BS3_DEBUG_STATE_STOPPED;
    pbs3debug->comm_state = BS3_DEBUG_COMM_STATE_NOSERVICE;
    pbs3debug->canQuit = 0;
}

void bs3_debug_prepare(int port)
{
    bs3_debug_init(&bs3debug, port);
}

void bs3_debug_end()
{
     bs3_debug_finish(&bs3debug);
}


void bs3_debug_comm_send(struct bs3_debug_data * pbs3debug, const char * msg)
{
    int isok;
    int i,j;
    char linebuffer[1024];
    if ( pbs3debug             == ((void *)0)                    ||
         pbs3debug->comm_state != BS3_DEBUG_COMM_STATE_CONNECTED || 
         pbs3debug->connfd     == 0                              ||
         msg                   == ((void *)0)                    ||
         msg[0]                == 0) return;


    for (i = 0,j = 0 ; i < strlen(msg); i++,j++)
    {
        switch(msg[i])
        {
            case '\n':
                linebuffer[j++] = '\r';
                linebuffer[j] = '\n';
            default:
                linebuffer[j] = msg[i];                
        }
    }
    linebuffer[j] = 0;

    isok = 0;
    do
    {
        if (write(pbs3debug->connfd, linebuffer, strlen(linebuffer)) < 0) 
        {
            switch (errno)
            {
                case EAGAIN:
#if EAGAIN != EWOULDBLOCK                
                case EWOULDBLOCK:
#endif                
                case EINTR:
                    usleep(100000); /* 1/10 of second wait*/
                    break;
                default:
                    isok = 1;
                    close(pbs3debug->connfd);
                    pbs3debug->connfd = 0;
                    pbs3debug->comm_state = BS3_DEBUG_COMM_STATE_NOCONNECTION;
            }
        } 
        else 
        {
            isok = 1;
        }
    } while (!isok);

}

void bs3_debug_comm_welcome(struct bs3_debug_data * pbs3debug)
{
    bs3_debug_comm_send(pbs3debug,"BS3 debugger\nYou may type 'h' for help\n");
}

void bs3_debug_comm_prompt(struct bs3_debug_data * pbs3debug)
{
    if (pbs3debug == ((void *)0)) return;
    if (pbs3debug->comm_state != BS3_DEBUG_COMM_STATE_CONNECTED) return;
    switch (pbs3debug->debug_state)
    {
        case BS3_DEBUG_STATE_RUNNING:
            bs3_debug_comm_send(pbs3debug,"\rrunning >");
            break;
        case BS3_DEBUG_STATE_TOSTOP:
        case BS3_DEBUG_STATE_STOPPED:
            bs3_debug_comm_send(pbs3debug,"\rstopped >");
            break;
        default:
            bs3_debug_comm_send(pbs3debug,"\runknown >"); /* should not occur */
    }
    if (pbs3debug->n > 0) {
        bs3_debug_comm_send(pbs3debug,pbs3debug->buff);
    }
}

long bs3_debug_util_getaddress(const char * address)
{
    long value = 0;
    int i;
    for (i=0; i < 4; i++)
    {
        switch (address[i])
        {
            case '0' ... '9':
                value = (value << 4) + (address[i]-'0');
                break;
            case 'a' ... 'f':
                value = (value << 4) + (address[i]-'a'+10);
                break;
            case 'A' ... 'F':
                value = (value << 4) + (address[i]-'A'+10);
                break;
            default:
                return -1;
        }
    }
    if (address[i] != 0 && address[i] != ' ') return -1;
    return value;
}

void bs3_debug_comm_cmd(struct bs3_debug_data * pbs3debug) 
{
    WORD address;
    int count;
    int value;
    char linebuffer[1024];
    WORD pc;
    int i;
    long addr;
    int len;

    if (pbs3debug == ((void *)0) || pbs3debug->n == 0 || pbs3debug->buff[0] == '\n')
    {
        bs3_debug_comm_prompt(pbs3debug);
        pbs3debug->n = 0;
        return;
    }
    if (pbs3debug->buff[1] != 0 && pbs3debug->buff[1] != ' ') pbs3debug->buff[0] = 0;
    
    len = strlen(pbs3debug->buff);
    switch (pbs3debug->buff[0])
    {
        case 'u':
            if (len >2 && len < 6)
            {
                bs3_debug_comm_send(pbs3debug,"Incorrect 'u' command\n");
                break;
            }
            if (len >=6)
            {
                addr = bs3_debug_util_getaddress(pbs3debug->buff+2);
                if (addr < 0)
                {
                    bs3_debug_comm_send(pbs3debug,"Incorrect 'u' command\n");
                    break;
                }
                pbs3debug->lastPC = (WORD)addr;
            }
            else  pbs3debug->lastPC = (pbs3debug->lastPC == 0)?pbs3debug->pbs3->r.PC:pbs3debug->lastPC;
            pc = pbs3debug->lastPC;
            for (i = 0 ; i < 4 ; i++)
            {
                pc = bs3_cpu_disassemble_(pc,
                                        pbs3debug->pbs3->m[pc], 
                                        pbs3debug->pbs3->m[(pc+1) & 0xFFFF], 
                                        pbs3debug->pbs3->m[(pc+2) & 0xFFFF], 
                                        pbs3debug->pbs3->m[(pc+3) & 0xFFFF], 
                                        linebuffer);
                strcat(linebuffer, "\n");
                bs3_debug_comm_send(pbs3debug,linebuffer);
            }
            pbs3debug->lastPC = pc;
            break;
        case 'g':
            if (len >2 && len < 6)
            {
                bs3_debug_comm_send(pbs3debug,"Incorrect 'g' command\n");
                break;
            }
            if (len >=6)
            {
                addr = bs3_debug_util_getaddress(pbs3debug->buff+2);
                if (addr < 0)
                {
                    bs3_debug_comm_send(pbs3debug,"Incorrect 'g' command\n");
                    break;
                }
                pbs3debug->debug_stop_at = (WORD)addr;
            }
            else   pbs3debug->debug_stop_at = 0;
            pbs3debug->debug_step_count = 0;
            pbs3debug->debug_state = BS3_DEBUG_STATE_RUNNING;
            break;
        case 't':
            pbs3debug->debug_step_count = 1;
            pbs3debug->debug_state = BS3_DEBUG_STATE_RUNNING;
            break;
        case 's':
            pbs3debug->debug_step_count = 0;
            pbs3debug->debug_state = BS3_DEBUG_STATE_STOPPED;
            break;
        case 'r':
            bs3_cpu_state(pbs3debug->pbs3, linebuffer);
            bs3_debug_comm_send(pbs3debug,linebuffer);
            break;
        case 'e':
            break;
        case 'E':
            break;
        case 'd':
            break;
        case 'z':
            pbs3debug->pbs3->status =  BS3_STATUS_RESET;
            pbs3debug->debug_step_count = 0;
            pbs3debug->lastPC = 0;
            pbs3debug->debug_state = BS3_DEBUG_STATE_TOSTOP;
            break;
        case 'Z':
            pbs3debug->pbs3->status =  BS3_STATUS_HALT;
            pbs3debug->debug_step_count = 1;
            pbs3debug->canQuit = 1;
            pbs3debug->debug_state = BS3_DEBUG_STATE_RUNNING;
            break;
        case 'q':
            close(pbs3debug->connfd);
            pbs3debug->comm_state = BS3_DEBUG_COMM_STATE_NOCONNECTION;
            break;
        case 'h':
            bs3_debug_comm_send(pbs3debug,"     Debuggger commands (address always 4 hexa digits, count is decimal, value is 0/1 or 2/4 hexa digits )\n");
            bs3_debug_comm_send(pbs3debug,"        u [address]        : unassemble code at 'address' or at PC if 'address' not provided\n");
            bs3_debug_comm_send(pbs3debug,"        g [address]        : unpause CPU until 'address' is reached (PC register value),\n");
            bs3_debug_comm_send(pbs3debug,"                             or continue as long as no pause is requested  ('s' command) if no 'address'\n");
            bs3_debug_comm_send(pbs3debug,"        t [count]          : execute one step 'count' times, \n");
            bs3_debug_comm_send(pbs3debug,"                             or only once if 'count' not provided \n");
            bs3_debug_comm_send(pbs3debug,"        s                  : pause cpu execution when running\n");
            bs3_debug_comm_send(pbs3debug,"        r [register value] : 'r' then show all register values\n");
            bs3_debug_comm_send(pbs3debug,"                             'r register value' set 'value' to 'register'\n");
            bs3_debug_comm_send(pbs3debug,"                             register amongst PC, SP, W0-3, B0-7, I, V, N, Z, C \n");
            bs3_debug_comm_send(pbs3debug,"        e address value    : set 'value' byte to address\n");
            bs3_debug_comm_send(pbs3debug,"        E address value    : set 'value' word to address (little endian)\n");
            bs3_debug_comm_send(pbs3debug,"        d [address]        : dump data at adress, or continue dump from previous dump address\n");
            bs3_debug_comm_send(pbs3debug,"        z                  : CPU reset (with data reset)\n");
            bs3_debug_comm_send(pbs3debug,"        Z                  : CPU halt (with debugger deconnection)\n");
            bs3_debug_comm_send(pbs3debug,"        q                  : quit debugger (deconnection but CPU program and state continue )\n");
            bs3_debug_comm_send(pbs3debug,"        h                  : this help\n");
            break;
        default:
            bs3_debug_comm_send(pbs3debug,"Unknown command. type 'h' for help\n");
    }
    pbs3debug->n = 0; /* command treated */
    //bs3_debug_comm_prompt(pbs3debug);
}


void bs3_debug_comm(struct bs3_debug_data * pbs3debug)
{
    int len;
    int i;
    int eol;
    struct sockaddr_in cli;
    int newco;
    const char * reject = "Sorry but BS3 debugger is already attached\n";
    char buff[BS3_DEBUG_COMM_MAXSIZE];
    int fdf;  /* file descriptor flag */
    if (pbs3debug == ((void *) 0)) return;
    if (((pbs3debug->count++) & 0x0FF) != 0 && pbs3debug->debug_state == BS3_DEBUG_STATE_RUNNING) return; /* consider socket reading , once every 256 call to this function : latency */
    if (pbs3debug->debug_state == BS3_DEBUG_STATE_STOPPED) usleep(100000);
    switch (pbs3debug->comm_state) 
    {
        case BS3_DEBUG_COMM_STATE_CONNECTED:
            if ((pbs3debug->count & 0xFFF) == 0 || (((pbs3debug->count & 0x0F) == 0) && (pbs3debug->debug_state == BS3_DEBUG_STATE_STOPPED)))
            {
               len = sizeof(pbs3debug->cli); 
               newco = accept(pbs3debug->sockfd, (SA*)&cli, &len); 
               if (newco > 0) {
                    write(newco,reject, strlen(reject) );
                    close(newco);
               }
            }
            /* manage communication with client , handle rejection of other connection request, handle communication failure */
            len = read(pbs3debug->connfd, buff, sizeof(buff)-1 );
            switch (len)
            {
                case -1:
                    switch (errno)
                    {
                        case EAGAIN:
#if EAGAIN != EWOULDBLOCK
                        case EWOULDBLOCK:
#endif                        
                        case EINTR:
                            break;
                        default:
                            close(pbs3debug->connfd);
                            pbs3debug->connfd = 0;
                            pbs3debug->comm_state = BS3_DEBUG_COMM_STATE_NOCONNECTION;
                    }
                    break;
                case 0: /* zero data read returned (End of File) : not sure if the case can occur when reading socket in non blocking mode */
                            close(pbs3debug->connfd);
                            pbs3debug->connfd = 0;
                            pbs3debug->comm_state = BS3_DEBUG_COMM_STATE_NOCONNECTION;
                    break;
                default: /* something is read */
                    i = 0;
                    eol = 0;
                    while ((i < len) && 
                           (pbs3debug->n < (BS3_DEBUG_COMM_MAXSIZE-1)) && 
                           buff[i] )
                    {
                        switch (buff[i])
                        {
                            case '\r':
                            case '\n':
                                eol = 1; /* end of line detected*/
                                break;
                            default:
                                if (buff[i]> 31) pbs3debug->buff[pbs3debug->n++] = buff[i];   
                                break;
                        }
                        i++;
                    }
                    pbs3debug->buff[pbs3debug->n] = 0;
                    if (eol) /* if end of line detected , then submit the command */
                    {
                        bs3_debug_comm_cmd(pbs3debug);
                        bs3_debug_comm_prompt(pbs3debug);
                    }
            }
            break;

        case BS3_DEBUG_COMM_STATE_NOCONNECTION:
            /* non blocking accept connection */
            /* Accept the data packet from client and verification */
            len = sizeof(pbs3debug->cli);
            pbs3debug->connfd = accept(pbs3debug->sockfd, (SA*)&pbs3debug->cli, &len);
            if (pbs3debug->connfd < 0) 
            {
                switch(errno)
                {
                    case EAGAIN:
#if EAGAIN != EWOULDBLOCK                    
                    case EWOULDBLOCK:
#endif                    
                        break;
                    default:
                        pbs3debug->comm_state = BS3_DEBUG_COMM_STATE_NOSERVICE;
                        close(pbs3debug->sockfd);
                        pbs3debug->sockfd = 0;
                }
                pbs3debug->connfd = 0;
                break;
            }
            /* set non blocking connection file descriptor */
            fdf = fcntl(pbs3debug->connfd, F_GETFL, 0); /* get current flag of socket file descriptor */
            fcntl(pbs3debug->connfd, F_SETFL, fdf | O_NONBLOCK); /* add non blocking flag to socket file descriptor */
            pbs3debug->comm_state = BS3_DEBUG_COMM_STATE_CONNECTED;
            bs3_debug_comm_welcome(pbs3debug);
            for (i = 1 ; i <= 10;i++)
            {
                 bs3_debug_comm(pbs3debug);
                 usleep(100000);
            }
            pbs3debug->n = 0;
            bs3_debug_comm_prompt(pbs3debug); 
            pbs3debug->lastPC = 0;
            break;

        case BS3_DEBUG_COMM_STATE_NOSERVICE:
            /* prepare the listening service */
            /* socket create and verification */
            pbs3debug->sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (pbs3debug->sockfd == -1) {
                printf("Debug socket creation failed...errno:%d\n",errno);
                exit(0);
            }
            bzero(&pbs3debug->servaddr, sizeof(pbs3debug->servaddr));
        
            /* assign IP, PORT */
            pbs3debug->servaddr.sin_family = AF_INET;
             /* pbs3debug->servaddr.sin_addr.s_addr = htonl(INADDR_ANY); */
            pbs3debug->servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
            pbs3debug->servaddr.sin_port = htons(pbs3debug->port);
        
            /* Binding newly created socket to given IP and verification */
            if ((bind(pbs3debug->sockfd, (SA*)&pbs3debug->servaddr, sizeof(pbs3debug->servaddr))) != 0) {
                printf("Debug socket bind failed... errno:%d\n",errno);
                exit(0);
            }

            /* Now server is ready to listen and verification */
            if ((listen(pbs3debug->sockfd, 5)) != 0) {
                printf("Debug socket listen failed...errno:%d\n",errno);
                exit(0);
            }

            /* non blocking socket accept */
            
            fdf = fcntl(pbs3debug->sockfd ,F_GETFL, 0); /* get current flag of socket file descriptor */
            fcntl(pbs3debug->sockfd, F_SETFL, fdf | O_NONBLOCK); /* add non blocking flag to socket file descriptor */
            pbs3debug->comm_state = BS3_DEBUG_COMM_STATE_NOCONNECTION;
            break;
    }
}

void bs3_debug(struct bs3_cpu_data * pbs3)
{
    if (pbs3 == ((void *)0))
    {
        bs3_debug_finish(&bs3debug);
        return; /* no debug to do, but possibly close debug connection*/
    }
    bs3debug.pbs3 = pbs3;
    if (pbs3->status == BS3_STATUS_HALT)
    {
        pbs3->status = (bs3debug.canQuit)?pbs3->status:BS3_STATUS_DEFAULT;
        bs3debug.debug_state = BS3_DEBUG_STATE_TOSTOP;
        bs3debug.lastPC = 0;
        return;
    }
    if (bs3debug.debug_state == BS3_DEBUG_STATE_TOSTOP) 
    {
        bs3debug.debug_state = BS3_DEBUG_STATE_RUNNING;
        bs3debug.debug_step_count = 1;
    }
    do 
    {
      
      if (bs3debug.debug_state == BS3_DEBUG_STATE_RUNNING) 
      {
        /* should we have to stop */
        if (((WORD)bs3debug.debug_stop_at) == pbs3->r.PC ) {
            bs3debug.debug_state = BS3_DEBUG_STATE_STOPPED;
            bs3debug.lastPC = bs3debug.pbs3->r.PC;
            bs3_debug_comm_send(&bs3debug,"\n");
            strcpy(bs3debug.buff, "r");
            bs3debug.n = 1;
            bs3_debug_comm_cmd(&bs3debug);
            strcpy(bs3debug.buff, "u");
            bs3debug.n = 1;
            bs3_debug_comm_cmd(&bs3debug);
            bs3_debug_comm_prompt(&bs3debug);
            bs3debug.debug_step_count = 0;
        } 
        else
        {
            if (bs3debug.debug_step_count > 0) {
                bs3debug.debug_step_count--;
                if (bs3debug.debug_step_count == 0) 
                {
                    bs3debug.debug_state = BS3_DEBUG_STATE_STOPPED;
                    bs3debug.lastPC = bs3debug.pbs3->r.PC;
                    bs3_debug_comm_send(&bs3debug,"\n");
                    strcpy(bs3debug.buff, "r");
                    bs3debug.n = 1;
                    bs3_debug_comm_cmd(&bs3debug);
                    strcpy(bs3debug.buff, "u");
                    bs3debug.n = 1;
                    bs3_debug_comm_cmd(&bs3debug);
                    bs3_debug_comm_prompt(&bs3debug);
                     /*bs3debug.n = 0; */
                }
            }
        }
      }
      bs3_debug_comm(&bs3debug);
    } while(bs3debug.debug_state == BS3_DEBUG_STATE_STOPPED);

}
