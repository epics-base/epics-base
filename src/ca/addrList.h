




union caAddr{
        struct sockaddr_in      inetAddr;
        struct sockaddr         sockAddr;
};

typedef struct {
        ELLNODE                 node;
        union caAddr            srcAddr;
        union caAddr            destAddr;
}caAddrNode;


