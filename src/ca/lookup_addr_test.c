#include                <types.h>
#include                <socket.h>
#include                <in.h>
#include                <netdb.h>
#include                <inet.h>

main(argc, argv)
int argc;
char **argv;
{
	struct in_addr	net_addr;
    	struct hostent	*ent;   
    	struct hostent	*gethostbyaddr();       

	unsigned long	net;
	unsigned long	addr;

        if(argc != 2){
                printf("test <inet addr with dots>\n");
                exit();
        }
printf("%s\n",argv[1]);
	net = inet_network(argv[1]);
	addr = inet_addr(argv[1]);
printf("%x %x\n", addr, net);
	net_addr = inet_makeaddr(net, addr);
printf("%x\n", net_addr.s_addr);



	/* the above seems to be broken ? */
    	ent = gethostbyaddr(&addr, sizeof(net_addr), AF_INET);
    	if(ent)
      		printf("%s\n", ent->h_name);   


}
